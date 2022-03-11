/*
 * @Date: 2022-03-10 03:24:01
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-11 07:44:12
 * @FilePath: /data/my_web/my_mian.cpp
 */

//先进行最基础的重写 到后面再来进行封装简化

//使用线程池实现的web服务器
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

#include "./locker/locker.h"
#include "./threadpool/threadpool.h"
#include "./http_conn/http_conn.h"
//#include "my_web.h"

//最大用户数
const int MAX_FD = 65536;
//最大事件数
const int MAX_EVENT_NUMBER = 10000;

//在http_conn中完成上树
extern int epoll_addfd(int epollfd, int fd, bool epoll_one_shot);

//同理 完成下树和关闭
extern int epoll_removefd(int epollfd, int fd);

//添加信号函数处理
void  addsig(int sig, void ( handler) (int), bool restart = true)
{
    struct sigaction sa;
    sa.sa_handler = handler;
    if( restart)
    {
        sa.sa_flags |= SA_RESTART;

    }   
    sigfillset( &sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//发送给客户端 (错误信息)
void send_error(int connfd, const char* to_error)
{
    //打印下错误
    printf("%s", to_error);
    size_t len = strlen(to_error);
    send(connfd, to_error, len, 0);
    //报错后关闭和客户端的连接
    close(connfd);
}


int main(int argc, char * argv[])
{
    if(argc <= 2)
    {
        printf("Insufficient number of inputs\n");
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    //后面将其变成守护进程
    

    //防止服务器端被关闭
    //添加屏蔽信号
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池和初始化线程池
    my_threadpool<my_http_conn> * pool = NULL;
    //单例初始化去要指向null
    try
    {
        pool = new my_threadpool<my_http_conn>;
    }
    catch(const std::exception& e)
    {
        return 1;
    }
     
    //预先准备好客户连接分配的http_conn
    my_http_conn* users = new my_http_conn[MAX_FD];
    assert(users);
    int user_count = 0;

    //开始创建连接

    //创建socket套接字    
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert( listenfd >= 0);

    
    /*设置优雅断开
    #include <arpa/inet.h>
    struct linger {
　　    int l_onoff;
　　    int l_linger;
    };
    三种断开方式：

    1. l_onoff = 0; l_linger忽略
    close()立刻返回，底层会将未发送完的数据发送完成后再释放资源，即优雅退出。

    2. l_onoff != 0; l_linger = 0;
    close()立刻返回，但不会发送未发送完成的数据，而是通过一个REST包强制的关闭socket描述符，即强制退出。

    3. l_onoff != 0; l_linger > 0;
    close()不会立刻返回，内核会延迟一段时间，这个时间就由l_linger的值来决定。如果超时时间到达之前，发送
    完未发送的数据(包括FIN包)并得到另一端的确认，close()会返回正确，socket描述符优雅性退出。否则，close()
    会直接返回错误值，未发送数据丢失，socket描述符被强制性退出。需要注意的时，如果socket描述符被设置为非堵
    塞型，则close()会直接返回值。
    */
    struct linger tmp = {1, 0};
    setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));


    int ret = 0;
    //创建address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);    
    
    //设置端口复用
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    //创建epoll树
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    //将listenfd作为根节点放入epoll树
    //且不做EPOLLONESHOT处理
    epoll_addfd(epollfd, listenfd, false);

    my_http_conn::m_epollfd = epollfd;
     
    //设置信号处理函数
    //addsig(S)
    printf("will being\n");
    //开始监听
    while(true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER ,-1);
        printf("epoll\n");
        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            
            if(sockfd == listenfd)
            {
                //接收新的客户
                struct sockaddr_in client;
                socklen_t client_length = sizeof(client);
                int connfd = accept(listenfd, (struct sockaddr*)&client, &client_length);
                printf("new client");
                if( connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                //还有用户数组满的情况
                if(my_http_conn::m_client_number >= MAX_FD)
                {
                    send_error(connfd, "Internet server busy");
                    continue;
                }


                //将新的客户放入客户数组中
                //并上树
                users[connfd].init(connfd, client);


            }
            //有异常的情况
            else if(events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP))
            {
                //有异常关闭客户端
                users[sockfd].close_conn();
            }
            //客户端传来数据 触发读操作
            else if(events[i].events & EPOLLIN)
            {
                //读取客户发送来的数据
                //若对于0 则将任务分给请求队列
                //请求队列再发给子线程处理
                if(users[sockfd].read() > 0)
                {
                    pool -> increase( users + sockfd);

                }
                else{
                    //如果小于等于0 则任务发送错误或断开连接
                    //断开连接
                    users[sockfd].close_conn();
                }
            }
            //收到子进程发来的写操作
            else if(events[i].events & EPOLLOUT)
            {
                //根据写的结果 决定是否关闭
                if(!users[sockfd].write())
                {
                    printf("write\n");
                    users[sockfd].close_conn();
                }
            }
            //添加其他操作
            //比如定时器
            //比如自身的信号
            else{}
        }
    }
    printf("ji");
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;


}


