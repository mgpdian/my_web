#ifndef __THREAD_SAFE_QUEUE_HPP__
#define __THREAD_SAFE_QUEUE_HPP__
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

//普通的安全队列

template<typename T>
class thread_safe_queue
{
private:
    typedef std::mutex MutexType;

public:
    //thread_safe_queue():data_queue(std::queue<std::shared_ptr<T>>()){}
    thread_safe_queue(){}
    // thread_safe_queue(const thread_safe_queue<T>& other){
    //     std::lock_guard<MutexType> lock(m);
    //     this->data_queue = other.data_queue;
    // }

    void push(T new_value){
        std::shared_ptr<T> data(std::make_shared<T>(std::move(new_value)));

        std::lock_guard<MutexType> lock(m);

        data_queue.emplace(data);

        data_cond.notify_one();
    }


    void wait_and_pop(T& value){
        std::unique_lock<std::mutex> lock(m);

        data_cond.wait(lock, [this]{return !data_queue.empty();});

        value = std::move(*data_queue.front());

        data_queue.pop();
    }

    std::shared_ptr<T> wait_and_pop(){
        std::unique_lock<MutexType> lock(m);

        data_cond.wait(lock, [this]{return !data_queue.empty();});

        std::shared_ptr<T> res = data_queue.front();

        data_queue.pop();

        return res;
    }


    bool try_pop(T& value){
        std::lock_guard<MutexType> lock(m);

        if(data_queue.empty()){
            return false;
        }

        value = std::move(*data_queue.front());

        data_queue.pop();

        return true;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<MutexType> lock(m);

        if(data_queue.empty()){
            return std::shared_ptr<T>();
        }

        std::shared_ptr<T> res = data_queue.front();

        data_queue.pop();
        return true;
    }

    bool empty() const{
        std::lock_guard<MutexType> lock(m);
        return data_queue.empty();
    }
private:
    mutable MutexType m;
    std::condition_variable data_cond;
    std::queue<std::shared_ptr<T> > data_queue; 
};

#endif