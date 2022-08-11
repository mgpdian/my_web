#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

#include "noncopyable.h"


//线程锁
template<class T>
struct ScopedLockImpl{
    ScopedLockImpl(T& mutex) : m_mutex(mutex)
    {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl(){
        unlock();
    }

    void lock()
    {
        if(!m_locked)
        {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if(m_locked)
        {
            
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};


//自旋锁
class Spinlock : Noncopyable{
public:

    typedef ScopedLockImpl<Spinlock> Lock;

    Spinlock(){
        pthread_spin_init(&m_mutex, 0);
    }

    ~Spinlock(){
        pthread_spin_destroy(&m_mutex);
    }

    void lock(){
        pthread_spin_lock(&m_mutex);

    }

    void unlock(){
        pthread_spin_unlock(&m_mutex);
    }
private:
    pthread_spinlock_t m_mutex;
};




#endif