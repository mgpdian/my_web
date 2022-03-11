/*
 * @Date: 2022-03-10 03:26:04
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-11 07:15:20
 * @FilePath: /data/my_web/locker/locker.h
 */
//线程同步类
//locker.h
#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
//信号类 起到通知请求队列的作用
//封装信号类
class my_sem
{
public:
    //信号量的初始化
    my_sem()
    {
        if(sem_init( &m_sem, 0, 0) != 0)
        {
             /* 构造函数没有返回值，可以通过抛出异常来报告错误 */
             throw std::exception();
        }
    }
    my_sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~my_sem()
    {
        sem_destroy( &m_sem );
    }

    //等待信号量
    bool wait()
    {
        return sem_wait( &m_sem ) == 0;
    }
    //增加信号量
    bool post()
    {
        return sem_post( &m_sem ) == 0;
    }

private:
    sem_t m_sem;
};


//封装互斥锁
class my_locker
{
public:
    //初始化锁
    my_locker()
    {
        if(pthread_mutex_init( &m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }

    //销毁互斥锁
    ~my_locker()
    {
        pthread_mutex_destroy( &m_mutex);
    }    

    //获取互斥锁 上锁
    bool lock()
    {
        return pthread_mutex_lock( &m_mutex) == 0;
    }
    //释放互斥锁
    bool unlock()
    {
        return pthread_mutex_unlock( &m_mutex) == 0;
    } 

private:
    pthread_mutex_t m_mutex;

};


//封装条件变量
class my_cond
{
public:
    //初始化条件变量
    my_cond()
    {
        if( pthread_mutex_init( &m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
        if( pthread_cond_init( &m_cond, NULL) != 0)
        {
            pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    //销毁条件变量
    ~my_cond()
    {
        pthread_cond_destroy( &m_cond);
        pthread_mutex_destroy( &m_mutex);

    }
    //等待条件变量
    bool wait()
    {
        int ret = 0;
        //上互斥锁避免出问题
        pthread_mutex_lock( &m_mutex);
        //等待条件变量
        //失败则引起线程阻塞并解锁
        //成功则解除线程阻塞并加锁
        ret = pthread_cond_wait( &m_cond, &m_mutex);
        pthread_mutex_unlock( &m_mutex);
        return ret == 0;
    }

    //唤醒等待条件变量的进程
    bool signal()
    {
        return pthread_cond_signal( &m_cond) == 0;
    }
private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif