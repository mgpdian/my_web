//线程池
//基于c++11的线程池
//同时为每个线程提供了 消息队列  来减少乒乓缓存 提高效率
//同时 在队列空闲的时候 可以到别的队列中取消息来工作

#ifndef __NEW_THREADPOOL_H__
#define __NEW_THREADPOOL_H__
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

//#include "../locker/locker.h"
#include "../sql_conn/sql_conn.h"
#include "function_wrapper.hpp"
#include "join_threads.h"
//#include "thread_safe_queue.hpp"
#include "thread_safe_queue_lockable_and_waiting.hpp"
#include "work_stealing_queue.hpp"
#include "transmit_data.hpp"
//实现一个无锁队列，让其拥有线程在其他线程窃取任务的时候，
//能够推送和弹出一个任务是可能的；
//为了证明这种方法的可行性，将使用一个互斥量来保护队列中的数据。
//我们希望任务窃取是一个不常见的现象，这样就会减少对互斥量的竞争，并且使得简单队列的开销最小。

template <typename F>
class transmit_data;


template <typename T>
class my_thread_pool {
private:
    
    friend class transmit_data<T>;
    typedef function_wrapper task_type;

    //循环终止 结束线程 的符号
    std::atomic_bool done;

    thread_safe_queue_lockable_and_waiting<task_type> pool_work_queue;

    std::vector<std::unique_ptr<work_stealing_queue> > queues;

    std::vector<std::thread> threads;  // 存放待执行的线程

    join_threads joiner;  //析构 回收线程池

    static thread_local work_stealing_queue* local_work_queue;
    //每个线程都有一个work_stealing_queue，而非只是普通的`std::queue<>

    static thread_local std::size_t my_index;
    //当前线程的序号

    connection_pool* m_connPool;  //数据库连接池

    int m_actor_model;  // reactor 和 proactor模型切换

    //获取自己的工作队列 并一直执行它
    void work_thread(unsigned my_index_) {
        my_index = my_index_;

        local_work_queue = queues[my_index].get();

        while (!done) {
            run_pending_task();
        }
    }

    void run_pending_task() {
        function_wrapper task;

        if (pop_task_from_local_queue(task) || pop_task_from_pool_queue(task) ||
            pop_task_from_other_thread_queue(task)) {
            task();
            
            //run(task.get());
        }

        else {
            std::this_thread::yield();
        }
    }

    



    //退队 自己的  当自己的完成的工作从队头退出
    bool pop_task_from_local_queue(task_type& task) {
        return local_work_queue && local_work_queue->try_pop(task);
    }

    //从 主线程的队列中退队  作用是从主线程那工作后 退队
    bool pop_task_from_pool_queue(task_type& task) {
        return pool_work_queue.try_pop(task);
    }

    //从其他线程队列中偷窃
    bool pop_task_from_other_thread_queue(task_type& task) {
        for (std::size_t i = 0; i < queues.size() - 2; ++i) {
            unsigned const index = (my_index + i + 1) % queues.size();
            if (queues[index]->try_steal(task)) {
                return true;
            }
        }

        return false;
    }

public:
    my_thread_pool() : done(false), joiner(threads) {
        size_t const threads_count = std::thread::hardware_concurrency();

        try {
            for (unsigned i = 0; i < threads_count - 2; ++i) {
                queues.emplace_back(std::make_unique<work_stealing_queue>(
                    new work_stealing_queue));

                threads.emplace_back(
                    std::thread(&my_thread_pool::work_thread, this, i));
            }
        } catch (...) {
            done = true;
            throw;
        }
    }

    //重载以前的多个参数
    my_thread_pool(connection_pool* connPool, int actor_model)
        : done(false),
          joiner(threads),
          m_connPool(connPool),
          m_actor_model(actor_model) {
        size_t const threads_count = std::thread::hardware_concurrency();

        try {
            for (unsigned i = 0; i < threads_count - 2; ++i) {
                queues.emplace_back(std::make_unique<work_stealing_queue>());

                threads.emplace_back(
                    std::thread(&my_thread_pool::work_thread, this, i));
            }
        } catch (...) {
            done = true;
            throw;
        }
    }

    ~my_thread_pool() { done = true; }

    template <typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> submit(
        FunctionType f) {
        typedef typename std::result_of<FunctionType()>::type result_type;

        std::packaged_task<result_type()> task(std::move(f));
        std::future<result_type> res(task.get_future());

        if (local_work_queue) {
            local_work_queue->push(std::move(task));
        } else {
            pool_work_queue.push(std::move(task));
        }

        return res;
    }

    //还需要一个函数来将任务添加到请求队列中
    //template <typename FunctionType>
    bool increase(T* request) {
        submit(transmit_data<T>(request, this));
        return true;
    }

    //重载 对reactor模式使用
    //template <typename FunctionType>
    bool increase(T* request, int state) {
        submit(transmit_data<T>(request, this));
        //就靠这个切换reactor的读写状态了!
        request->m_state = state;
        return true;
    }
};
template <typename T>
thread_local work_stealing_queue* my_thread_pool<T>::local_work_queue;
//每个线程都有一个work_stealing_queue，而非只是普通的`std::queue<>

template <typename T>
thread_local std::size_t my_thread_pool<T>::my_index;
//当前线程的序号
#endif