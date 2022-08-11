/*
 * @Date: 2022-03-17 01:39:34
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-17 06:19:00
 * @FilePath: /data/my_web/timer/min_heap.cpp
 */
#include "min_heap.h"

#include "../log/new_log.h"

static Logger::ptr g_logger = MY_LOG_NAME("system");

//时间堆类

    //初始化一个大小cap的空堆
    time_heap::time_heap(int cap) : capacity(cap), cur_size(0)
    {
        //创建堆数组
        array = new heap_timer* [capacity];
        if(!array) throw std::exception();
        for(int i = 0; i < capacity; ++i)
        {
            array[i] = NULL;
        }

    }

    //用已有的数组来初始化堆
    time_heap::time_heap(heap_timer** init_array, int size, int capacity): cur_size(size), capacity(capacity)
    {
        if(capacity < size)
        {
            throw std::exception();
        }
        array = new heap_timer* [capacity]; //创建堆数组
        if(!array)
        {
            throw std::exception();
        }
        for(int i = 0; i < capacity; ++i)
        {
            array[i] = NULL;
        }

        if(size != 0)
        {
            //初始化堆数组
            for(int i = 0; i < size; ++i)
            {
                array[i] = init_array[i];
            }
            for(int i = (cur_size - 1) / 2; i >= 0; --i)
            {
                //对数组中的第[(cur_size-1)/2]~0个元素执行下滤操作
                percolate_down(i);
            }
        }
    }

    //摧毁时间堆
    time_heap::~time_heap()
    {
        for(int i = 0; i < cur_size; ++i)
        {
            delete array[i];
        }
        delete[] array;
    }

    //添加目标定时器timer
    void time_heap::add_timer(heap_timer* timer) 
    {
        if (!timer)
        {
            return;
        }
        if(cur_size >= capacity) //如果当前堆数组的容量不够 将其扩大一倍
        {
            resize();
        }
        //插入新元素 当前堆大小加1 hole是新建空穴位置
        int hole = cur_size++;
        int parent = 0;
        //对从空穴到根节点的路径上的所有节点执行上滤操作
        for ( ; hole > 0; hole = parent)
        {
            parent = (hole - 1) / 2;
            if( array[parent] -> expire <= timer -> expire)
            {
                break;
                //当遇到父节点被自己小的时候停止 跳出循环
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
        
        
        
    }
    //删除目标定时器timer;
    void time_heap::del_timer(heap_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        //仅仅将目标定时器的回调函数设置为空 即所谓的延迟销毁, 这样节省真正删除该定时器的开销 但容易造成堆数组膨胀
        timer -> cb_func = NULL;
    }
    //获取堆顶部的定时器
    heap_timer* time_heap::top() const
    {
        if(empty())
        {
            return NULL;
        }
        return array[0];
    }

    //删除堆顶部的定时器
	void time_heap::pop_timer()
    {
        if(empty())
        {
            return;
        }
        if(array[0])
        {
            //printf("%d\n", cur_size);
            delete array[0];
            ////将原来的堆顶元素替换为堆数组中最后一个元素
            array[0] = array[--cur_size];
            percolate_down(0);
            //新的堆顶元素进行下滤操作;
        }

    }
    //心搏函数
    void time_heap::tick()
    {
        heap_timer* tmp = array[0];
        time_t cur = time(NULL); //循环处理堆中到期的定时器
        while(!empty())
        {
            if(!tmp)
            {
                break;
            }
            //如果堆顶定时器还没到期 则退出循环
            if(tmp -> expire > cur)
            {
                break;
            }
            //否则执行堆顶定时器中的任务
            if(array[0] -> cb_func)
            {
                array[0] -> cb_func(array[0] -> user_data);
            }
            //将堆顶元素删除 同时生成新的堆顶定时器
            //printf("tick() pop\n");
            pop_timer();
            tmp = array[0];
        }
    
    
    
    }
    bool time_heap::empty() const
    {
        return cur_size == 0;
    }

    //最小堆的下滤操作 它确保堆数组中以第hole个节点作为根的子树拥有最小堆性质
    void time_heap::percolate_down(int hole)
    {
        heap_timer* temp = array[hole];
        int child = 0;
        for(;((hole * 2 + 1) <= (cur_size - 1)); hole = child)
        {
            child = hole * 2 + 1;
            if((child < (cur_size - 1)) && (array[child + 1] -> expire < array[child] -> expire))
            {
                ++child;
            }
            if(array[child] -> expire < temp -> expire)
            {
                array[hole] = array[child];
                //child 小于他的话就换 都大于他的话就跳出来
            }
            else
            {
                break;
            }
        }
        array[hole] = temp;
    }
    //堆数组容量扩大一倍
    void time_heap::resize()
    {
        heap_timer** temp = new heap_timer* [2 * capacity];
        for(int i = 0; i < 2 * capacity; ++i)
        {
            temp[i] = NULL;
        }
        if(!temp)
        {
            throw std::exception();
        }
        capacity = 2 * capacity;
        for(int i = 0 ; i < cur_size; ++i)
        {
            temp[i] = array[i];
        }
        delete[] array;
        array = temp;
    }