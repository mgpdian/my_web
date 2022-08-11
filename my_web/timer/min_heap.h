/*
 * @Date: 2022-03-17 01:21:40
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-17 04:58:39
 * @FilePath: /data/my_web/timer/min_heap.h
 */


#ifndef _MIN_HEAP_H
#define _MIN_HEAP_H

#include<iostream>
#include<netinet/in.h>
#include<time.h>

using std::exception;

#define BUFFER_SIZE 64
class heap_timer;
//绑定socket和定时器
struct client_data
{
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
	heap_timer* timer;
};

//定时器类
class heap_timer
{
public:
    heap_timer(int delay)
    {
		expire = time(NULL) + delay;
	}
public:
    time_t expire; //计时器生效的绝对时间
    void (*cb_func)(client_data*); //计时器的回调函数
    client_data* user_data; //用户数据
};

//时间堆类
class time_heap
{
public:
    time_heap(){
        time_heap(64);
    }
    //初始化一个大小cap的空堆
    time_heap(int cap = 64);  //sort_timer_lst(); 

    //用已有的数组来初始化堆
    time_heap(heap_timer** init_array, int size, int capacity);

    //摧毁时间堆
    ~time_heap(); //~time_heap

public:
    //添加目标定时器timer
    void add_timer(heap_timer* timer) ;////add_timer(util_timer* timer); 

    //删除目标定时器timer;
    void del_timer(heap_timer* timer);// del_timer(util_timer * timer); 

    //获取堆顶部的定时器
    heap_timer* top() const;       //adjust_timer(util_timer * timer);
    
    //删除堆顶部的定时器
	void pop_timer();   
 
    //心搏函数
    void tick();

    bool empty() const;

private:
    //最小堆的下滤操作 它确保堆数组中以第hole个节点作为根的子树拥有最小堆性质
    void percolate_down(int hole);

    //堆数组容量扩大一倍
    void resize();
private:
    heap_timer** array; //堆数组
    int capacity;   //堆数组的容量
    int cur_size; //堆数组当前包含的元素个数
};
#endif