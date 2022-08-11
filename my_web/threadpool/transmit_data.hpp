//这里来写 传输的数据和 数据运行会调用的run方法

#ifndef __TRANSMIT_DATA_HPP__
#define __TRANSMIT_DATA_HPP__

#include "new_threadpool.h"
template <typename T>
class my_thread_pool;


template <typename F>
class transmit_data {
public:
    F* request;

    //template <typename T>
    my_thread_pool<F>* pool;

    //template <typename T>
    transmit_data(F* f_, my_thread_pool<F>* pool_) : request(f_) , pool(pool_){}
    //transmit_data(F&& f_) : f(std::move(f_)) {}
    void operator()() {
        // reactor和proactor模型的核心区别
        if (pool->m_actor_model == 0)  // reactor
        {
            // m_state为读写状态
            if (request->m_state == 0) {
                if (request->read()) {
                    request->improv = 1;
                    connectionRAII mysqlconn(&request->mysql, pool->m_connPool);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else {
            connectionRAII mysqlconn(&request->mysql, pool->m_connPool);
            //获取一个空闲连接 将他赋给http_conn的mysql连接
            //当process运行结束 就会自动析构掉
            //疑问点 request 运行时也会去调用connectionRAII 为什么现在要调用
            request->process();
        }
    }
};

#endif