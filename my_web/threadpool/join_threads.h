
#ifndef __JOIN_THREADS_H__
#define __JOIN_THREADS_H__
#include <thread>
#include <vector>
//负责线程池的线程回收
class join_threads
{
    std::vector<std::thread>& threads;

public:
    explicit join_threads(std::vector<std::thread>& threads_):threads(threads_){}

    ~join_threads()
    {

        for(auto& thread : threads)
        {
            if(thread.joinable())
            {
                thread.join();
            }
        }
    }
};
#endif