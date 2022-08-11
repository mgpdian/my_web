/*
 * @Date: 2022-03-13 01:43:44
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-16 02:29:52
 * @FilePath: /data/my_web/timer/lis_timer.cpp
 */
#include "lis_timer.h"
#include "../log/new_log.h"

static Logger::ptr g_logger = MY_LOG_NAME("system");
 sort_timer_lst::sort_timer_lst()
 {
      head = NULL;
     tail = NULL;
 }
sort_timer_lst::~sort_timer_lst()
{
    util_timer* tmp = head;
    while(tmp)
    {
        head = tmp -> next;
        delete tmp;
        tmp = head;
    }
}

 //将目标定时器timer添加到链表中
void sort_timer_lst::add_timer(util_timer* timer)
{
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head = tail = timer;
        return;
    }

    //为了链表的有序性；我们根据目标定时器的超过时间来排序，
    //（按照超时时间从小到大排序）如果超时时间小于head，成为head，
    //负责根据顺序插入
    if(timer -> expire < head -> expire)
    {
        timer -> next = head;
        head -> prev = timer;

        head = timer;
        return;
    }
    //如果timer比头节点大 则开始遍历查询位置
    add_timer(timer, head);
}
    //当某个定时任务发送变化时 调整对应的定时器在链表中的位置, 这个函数只考虑被调整的定时器的超时时间延长情况, 即定时器需要往链表尾部移动
	//只考虑时间延长的情况
void sort_timer_lst::adjust_timer(util_timer * timer)
{
    if(!timer)
    {
        return;
    }
    util_timer* tmp = timer -> next;
    //如何被调整的目标定时器处在链表尾部, 或者该定时器新的超时值
    //仍然小于其下一个定时器的超时值, 则不用调整

    if(!tmp || (timer -> expire < tmp -> expire))
    {
        return;
    }
    //如果目标定时器是链表的头节点 则该定时器从链表中取出并重新插入链表
    if(timer == head)
    {
        head = head -> next;
        head -> prev = NULL;
        timer -> next = NULL;
        add_timer(timer, head);
    }
    ////如果目标定时器不是链表头节点 则将该定时器从链表中取出 然后插入其原来所在位置之后的部分链表中
    else
    {
        timer -> prev -> next = timer -> next;
        timer -> next -> prev = timer -> prev;

        add_timer(timer, timer->next);
    }

}
    //将定时器timer从链表中删除
void sort_timer_lst::del_timer(util_timer * timer)
{
    if(!timer)
    {
        return;
    }
    //下面这个条件成立表示链表只有一个定时器 即目标定时器
    if((timer == head)  && (timer == tail))
    {
        delete timer;
        timer = NULL;
        head = NULL;
        tail = NULL;
        return;
    }
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        timer = NULL;
        return;
    }
     //如果链表至少有俩个定时器 且目标定时器是链表的尾节点,则将链表的尾结点重置为原尾节点的前一个节点, 然后删除目标定时器
     if(timer == tail)
     {
         tail = tail -> prev;
         tail -> next = NULL;
         delete timer;
         timer = NULL;
         return;
     }   
     //如果目标定时器位于链表之间
     timer -> prev -> next = timer -> next;
     timer -> next -> prev = timer -> prev;
     delete timer;
     timer = NULL;
     return;
}
    //SIGLRM信号每次被触发就在其信号处理函数中执行一次tick, 处理链表上到期的任务
void sort_timer_lst::tick()
{
    if(!head)
    {
        return;
    }

    //printf("timer tick\n");
    time_t cur = time(NULL); //获取当前系统时间
    util_timer* tmp = head;
    //遍历处理每个定时器, 值到一个未到时间的定时器
		//这就是定时器的核心逻辑
    while(tmp)
    {
        if(cur < tmp -> expire)
        {
            break;
        }
        //调用定时器的回调函数 以执行定时任务
        //下数加断开连接
        tmp -> cb_func(tmp -> user_data);
        //执行完定时器中的定时任务 将它删除 重置头节点
        head = tmp -> next;
        if(head)
        {
            head -> prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}


    ////一个重载的辅助函数, 它被add_timer和adjust_timer调用
	//用途 插入链表
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* tmp = prev -> next;
    //遍历节点 找到一个时间大于目标定时器的超时时间的节点, 并将目标定时器插入节点之前

    while(tmp)
    {
        if(timer -> expire < tmp -> expire)
        {
            prev -> next = timer;
            timer -> next = tmp;
            tmp -> prev = timer;
            timer -> prev = prev;
            break;

        }
        prev  = tmp;
        tmp = tmp -> next;
    }

    //遍历完仍没有找到节点 则插入尾部
	if(!tmp)
	{
			prev -> next = timer;
			timer -> prev = prev;
			timer -> next = NULL;
			tail = timer;
	}
}

