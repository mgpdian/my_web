/*
 * @Date: 2022-03-13 23:51:26
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 04:03:50
 * @FilePath: /data/my_web/log/block_queue.h
 */
//在创建日志文件之前 先创建阻塞队列
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../locker/locker.h"

using namespace std;

template <typename T>
class block_queue
{
public:
        //设置队列初始化
        block_queue(int max_size = 1000)
        {
            if( max_size <= 0)
            {
                exit(-1);
            }
            //队列最大长度
            m_max_size = max_size;
            //初始化数组
            m_array = new T[max_size];
            //队列当前长度
            m_size = 0;
            m_front = -1;
            m_back = -1;
        }
        //清空
        void clear()
        {
            m_mutex.lock();
            m_size = 0;
            m_front = -1;
            m_back = -1;
            m_mutex.unlock();
        }


        ~block_queue()
        {
            m_mutex.lock();
            if(m_array != NULL)
            {
                delete[] m_array;
            }

            m_mutex.unlock();
        }

        //判断队列是否满了
        bool is_full()
        {
            m_mutex.lock();
            if( m_size >= m_max_size)
            {
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }

        //判断队列是否尾空
        bool is_empty()
        {
            m_mutex.lock();
            if(0 == m_size)
            {
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }


        //返回队首元素
        bool front(T &value)
        {
            m_mutex.lock();
            if(0 == m_size)
            {
                m_mutex.unlock();
                return false;
            }
            value = m_array[m_front];
            m_mutex.unlock();
            return true;            
        }

        //返回队尾元素
        bool back(T &value)
        {
            m_mutex.lock();
            if(0 == m_size)
            {
                m_mutex.unlock();
                return false;
            }
            value = m_array[m_back];
            m_mutex.unlock();
            return true;
        }

        int size()
        {
            int tmp = 0;
            m_mutex.lock();
            tmp = m_size;

            m_mutex.unlock();
            return tmp;
        }
        int max_size()
        {
            int tmp = 0;
            m_mutex.lock();
            tmp = m_max_size;
            m_mutex.unlock();
            return tmp;
        }
        //往队列添加元素 需要将所有使用队列的线程唤醒
        //当有元素金队列时  相当于生产者生产了一个元素
        //若当前没有线程等待条件变量 则唤醒无意义
        bool push(const T &item)
        {
            m_mutex.lock();
            if(m_size >= m_max_size)
            {
                //全部唤醒
                m_cond.broadcast();
                m_mutex.unlock();
                return false;
            }
            if (m_back == -1 && m_front == -1)
            {
                m_back = 0;
                m_front = 0;
            }
            else
                m_back = (m_back + 1) % m_max_size;
            m_array[m_back] = item;
            m_size++;
            m_cond.broadcast();
            m_mutex.unlock();
            return true;
        }

        //pop时,如果当前队列没有元素,将会等待条件变量
        bool pop(T &item)
        {
            m_mutex.lock();
            while(m_size <= 0)
            {
                if(!m_cond.wait(m_mutex.get()))
                {
                    m_mutex.unlock();
                    return false;
                }

            }
            item = m_array[m_front];
            m_front = (m_front + 1 ) % m_max_size;
            
            m_size--;
            m_mutex.unlock();
            return true;
        }
        
        //超时处理
        //在pthread_cond_wait基础上增加了等待的时间，只指定时间内能抢到互斥锁即可
        bool pop(T &item, int ms_timeout)
        {
            struct timespec t = {0, 0};
            struct timeval now = {0, 0 };
            gettimeofday(&now, NULL);//获取当前时间
            m_mutex.lock();
            if(m_size <= 0)
            {
                t.tv_sec = now.tv_sec + ms_timeout / 1000;
                t.tv_nsec = (now.tv_usec + (ms_timeout % 1000) * 1000) * 1000;
                //看是否能获取到资源
                if(!m_cond.timewait(m_mutex.get(), t))
                {
                    m_mutex.unlock();
                    return false;
                }
            }

            if( m_size <= 0)
            {
                m_mutex.unlock();
                return false;
            }
            item = m_array[m_front];
            m_front = (m_front + 1) % m_max_size;
            
            m_size--;
            m_mutex.unlock();
            return true;
        }

private:
    my_locker m_mutex;//互斥锁
    my_cond m_cond; //条件变量

    T* m_array; //队列数组
    int m_size;//当前长度
    int m_max_size;//总长度
    int m_front;//队头
    int m_back;//队尾
};



#endif