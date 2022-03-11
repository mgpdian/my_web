/*
 * @Date: 2022-03-10 04:15:11
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-11 07:46:39
 * @FilePath: /data/my_web/threadpool/threadpool.h
 */
//线程池
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>


#include "../locker/locker.h"
//线程池
template< typename T>
class my_threadpool
{
public:
    //构造函数
    //设置线程池的线程量thread_number
    //设置请求队列上限 max_queue_number
    my_threadpool(int thread_number = 8, int max_queue_number = 10000);
    
    //析构函数
    ~my_threadpool();
    
    //还需要一个函数来将任务添加到请求队列中
    bool increase(T* requset);
private:
    //工作函数 不断从请求队列中取取任务并运行运行汉
    static void* worker(void *arg);
    //运行函数
    void run();
private:
    int m_thread_number; //线程池中的线程数

    //线程池的数组 ,因为线程数量是动态 所以用指针
    pthread_t* m_threads;    
    //为什么要加*
    //应该是队列公用
    std::list<T*>  m_request_queue;//请求队列

    //请求队列的最大值
    int m_max_queue_number; 
    
    my_locker m_queuemutex;//保护请求队列的互斥锁

    //通知进程请求队列不为空 可以工作的信号量
    my_sem m_queuestart;

    //循环终止 结束线程 的符号
    bool m_stop;
    
};
//构造函数
//设置线程池的线程量thread_number
//设置请求队列上限 max_queue_number
template< typename T>
my_threadpool<T> :: my_threadpool(int thread_number, int max_queue_number)
 : m_thread_number(thread_number), m_max_queue_number(max_queue_number),
 m_stop(false), m_threads(NULL)
{
    if((thread_number <= 0 ) || (max_queue_number <= 0))
    {
        throw std::exception();
    }

    //对指针线程数组进行初始化
    m_threads = new pthread_t[m_thread_number];
    if( !m_threads)
    {
        throw std::exception();
    }

    for(int i = 0; i < m_thread_number; ++i)
    {
        //创建thread_number个线程 
        //然后将他们设置为脱离线程
        printf("create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker , this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

    }
    
}

//构析函数
template < typename T>
my_threadpool<T>:: ~my_threadpool()
{
    //销毁线程池
    delete[] m_threads;
    m_stop = true;
}

//添加请求到请求队列
template <typename T>
bool my_threadpool<T>:: increase(T* requires)
{
    //因为请求队列是共享的
    //所以在添加请求时必须保持线程同步
    m_queuemutex.lock();
    //如果队满则跳过解锁
    if(m_request_queue.size() >= m_max_queue_number)
    {
       m_queuemutex.unlock();
       return false; 
    }
    m_request_queue.push_back(requires);
    
    m_queuemutex.unlock();
    m_queuestart.post();
    return true;
}

 /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
template< typename T>
void* my_threadpool<T>:: worker(void* arg)
{
     my_threadpool* pool = (my_threadpool*) arg;
     pool -> run();
     return pool;
}


//运行函数
template< typename T>
void my_threadpool<T>:: run()
{
    //取请求队列内的一个请求
    while(!m_stop)
    {
        //等待信号量
        m_queuestart.wait();
        m_queuemutex.lock();
        if(m_request_queue.empty())
        {
            m_queuemutex.unlock();
            return;
        }
        T* requires = m_request_queue.front();
        m_request_queue.pop_front();
        m_queuemutex.unlock();
        if(!requires)
        {
            continue;
        }
        //开始分析请求的内容
        printf("begin requires run\n");
        requires -> process();
    }
    
}
#endif