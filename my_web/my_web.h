/*
 * @Date: 2022-03-10 02:57:27
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 08:24:41
 * @FilePath: /data/my_web/my_web.h
 */
//先进行最基础的重写 到后面再来进行封装简化

//使用线程池实现的web服务器

#ifndef _MY_WEB_H
#define _MY_WEB_H
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
#include <iostream>

#include "./locker/locker.h"
#include "./threadpool/threadpool.h"
#include "./http_conn/http_conn.h"
#include "./strdecode/strdecode.h"
//#include "./timer/lis_timer.h"
#include "./timer/min_heap.h" //try min_heap
#include "./log/log.h"
#include "./sql_conn/sql_conn.h"
#include "./LRU/LRU.h"  
//最大用户数
const int MAX_FD = 65536;
//最大事件数
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

//信号传输管道
//用于处理信号函数 也和子函数通信有关
static int m_pipefd[2];


// static client_data* users_timer;
//在http_conn中完成上树
extern int epoll_addfd(int epollfd, int fd, bool epoll_one_shot, int trig_model);

//同理 完成下树和关闭
extern int epoll_removefd(int epollfd, int fd);

class my_web
{

public:
    my_web(): m_time_heap(8){};

  
    //析构函数
    ~my_web();

    
    //初始化设置
    void init(int port, string user, string passWord, string databaseName, int sql_num, int trig_model, int actor_model, int close_log = 0);

    //创建监听文件描述符
    void create_listen(); 

    //创建epoll树并将m_listen上树
    void create_epoll();
    
    //将新的客户放入客户数组中
    void client_init(int connfd, struct sockaddr_in client_address);

    //接收一个用户的请求
    bool accept_client();

    //接收到信号后的处理
    bool time_siganl();

    //处理读事件
    void read_thing(int sockfd);


    //写事件
    void write_thing(int sockfd);


    


    //主循环
    void main_loop();

    

    //创建用户数组 用于管理
    void create_http_conn();

    //初始化线程池
    bool create_threadpool();

    //初始化日志
    void log_write();
    
    //初始化连接池
    void create_mysqlpool();

    //创建定时器
    void create_timer();

    //添加定时器任务 并重新计时
    void time_handler();

    //定时器的回调函数 它删除非活动连接socket上的注册事件,并关闭
    //void cb_func(client_data *user_data);

    //发送给客户端 (错误信息)
    void send_error(int connfd, const char *to_error);

    //若用户被使用 比如读或写 则更新定时器
    //void adjust_timer(util_timer *timer);
    void adjust_timer(heap_timer *timer);
    //若用户断开连接 则将定时器删除
    //void deal_timer(util_timer *timer, int sockfd);
    void deal_timer(heap_timer *timer, int sockfd);
    //先写好信号处理函数 最后看要不要封装
    //void sig_handler(int sig);

    //添加信号函数处理
    void addsig(int sig, void(handler)(int), bool restart = true);

    //设置非阻塞
    int setnonblocking_main(int fd);
    
private:
    //基础网络
    int m_port;//端口号
    int m_listenfd;//主监听端口
    
    int m_epollfd;//epoll树

    //信号传输管道
   // int m_pipefd[2];
    my_http_conn *users; //用户数组

    epoll_event events[MAX_EVENT_NUMBER];//epoll事件数组

    //线程池相关
    my_threadpool<my_http_conn>* m_pool;//线程池
     //int m_thread_num; 线程数量
    
    client_data * users_timer;//用户定时器和用户数据

    
    //计时器
   // sort_timer_lst m_timer_lst; //计时器链表
    //信号触发事件
    bool m_timeout; //计时器触发

    bool m_stop_server;//循环结束


    //日志
    int m_close_log; //是否关闭日志
    int m_log_write;  //日志写的方法 : 同步 异ss


   


    //数据库连接池
    connection_pool *m_connPool;
    string m_user;  //登录的数据库用户名
    string m_passWord;  //登录数据库密码
    string m_databaseName;  //使用数据库名
    int m_sql_num; //连接池的连接数
    
    
     //模式选择 后面会写 大概
    int m_trig_model; //触发模式: ET 1 LT 2
    int m_actor_model; //反应堆模式: proactor 0 reactor 1


    //尝试时间堆
    time_heap m_time_heap;
};



#endif