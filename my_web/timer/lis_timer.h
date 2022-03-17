/*
 * @Date: 2022-03-13 00:55:27
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-17 03:28:44
 * @FilePath: /data/my_web/timer/lis_timer.h
 */

#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <iostream>
#include <time.h>
#define BUFFER_SIZE 64
class util_timer; //前向声明 ?

//存储用户数据结构
//:客户端socket地址 socket文件描述符 读缓存和定时器
struct client_data
{
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
	util_timer* timer;
};

//定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL){}



public:
    time_t expire; //任务超时时间 这里使用绝对时间
    void (*cb_func) (client_data*); //任务回调函数
    client_data* user_data;
    util_timer * prev; //指向前一个定时器
    util_timer* next; //指向下一个定时器

};

//定时器链表 它是一个升序 双向链表 且常头节点和尾节点
class sort_timer_lst
{
public:
    sort_timer_lst();   //time_heap(int cap)
    //链表被摧毁是 删除其中所有的定时器
    ~sort_timer_lst();  //~time_heap

    //将目标定时器timer添加到链表中
    void add_timer(util_timer* timer); //add_timer(heap_timer* timer)

    //当某个定时任务发送变化时 调整对应的定时器在链表中的位置, 这个函数只考虑被调整的定时器的超时时间延长情况, 即定时器需要往链表尾部移动
	//只考虑时间延长的情况
    void adjust_timer(util_timer * timer); 

    //将定时器timer从链表中删除
    void del_timer(util_timer * timer);  //del_timer(heap_timer* timer)

    //SIGLRM信号每次被触发就在其信号处理函数中执行一次tick, 处理链表上到期的任务
    void tick();    //tick()


private:
    ////一个重载的辅助函数, 它被add_timer和adjust_timer调用
	//用途 插入链表
    void add_timer(util_timer* timer, util_timer* lst_head);

private:
    util_timer* head;
    util_timer* tail;
};





#endif