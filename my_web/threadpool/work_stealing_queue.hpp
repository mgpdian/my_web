#ifndef __WORK_STEALING_QUEUE_HPP__
#define __WORK_STEALING_QUEUE_HPP__
#include "function_wrapper.hpp"
#include <deque>
#include <mutex>
//简单的基于锁的任务窃取队列
class work_stealing_queue
{
private:
    typedef std::mutex MutexType;
private:
    typedef function_wrapper data_type;
    std::deque<data_type> the_queue;
    //这个方向是给本线程拿的 一个方向是给其他线程偷的
    mutable MutexType the_mutex;

public:
    work_stealing_queue() = default;

    work_stealing_queue(const work_stealing_queue& other) = delete;
    work_stealing_queue& operator=(const work_stealing_queue& other) = delete;

    void push(data_type data){
        std::lock_guard<MutexType> lock(the_mutex);
        the_queue.push_front(std::move(data));

    }

    bool empty() const
    {
        std::lock_guard<MutexType> lock(the_mutex);
        return the_queue.empty();
    }

    //这个对自己进行使用
    bool try_pop(data_type& res)
    {
        std::lock_guard<MutexType> lock(the_mutex);
        if(the_queue.empty()){
            return false;
        }

        res = std::move(the_queue.front());
        the_queue.pop_front();
        return true;
    }

    bool try_steal(data_type& res)
    {
        std::lock_guard<MutexType> lock(the_mutex);
        if(the_queue.empty()){
            return false;
        }

        res = std::move(the_queue.back());
        the_queue.pop_back();

        return true;
    }
};




#endif