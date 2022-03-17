/*
 * @Date: 2022-03-10 03:03:06
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 08:59:06
 * @FilePath: /data/my_web/my_web.cpp
 */
/*
 * @Date: 2022-03-10 03:24:01
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-14 06:43:35
 * @FilePath: /data/my_web/my_mian.cpp
 */

//先进行最基础的重写 到后面再来进行封装简化

//使用线程池实现的web服务

#include "my_web.h"

//释放空间
my_web::~my_web()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

//初始化设置
void my_web::init(int port, string user, string passWord, string databaseName, int sql_num, int trig_model, int actor_model, int close_log)
{

    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_close_log = 0;
    m_log_write = 0;
    m_sql_num = sql_num;
    m_actor_model = actor_model;
    m_trig_model = trig_model;
    
}

// extern int setnonblocking(int fd);

//先写好信号处理函数 最后看要不要封装
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    if (msg == SIGALRM) 
    {
        printf("sig == SIGALRM = %d\n", msg);
    }
    else if (msg == SIGTERM)
    {
        printf("sig == SIGTERM = %d\n", msg);
    }
    else
    {
        printf("sig == %d", msg);
    }

    send(m_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
//添加信号函数处理
void my_web::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//设置非阻塞
int my_web::setnonblocking_main(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
// void epoll_addfd(int epollfd, int fd)
// {
//     epoll_event event;
//     event.data.fd = fd;
//     event.events = EPOLLIN | EPOLLET;
//     epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
//     setnonblocking(fd);
// }

//添加定时器任务
void my_web::time_handler()
{
    //定时器任务
    // m_timer_lst.tick();
    // alarm(TIMESLOT);

    m_time_heap.tick();
    printf("tick()\n");
    if (!m_time_heap.empty())
    {
       // m_time_heap.pop_timer();
        heap_timer *heap = m_time_heap.top();
        alarm(heap->expire);
        printf("wwww\n");
    }
    else{
       alarm(TIMESLOT); 
    }
}

//定时器的回调函数 它删除非活动连接socket上的注册事件,并关闭
void cb_func(client_data *user_data)
{
    // epoll_ctl(my_http_conn::m_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    epoll_ctl(my_http_conn::m_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);

    assert(user_data);
    close(user_data->sockfd);
    my_http_conn::m_client_number--;
    printf("close fd %d\n", user_data->sockfd);
    LOG_INFO("fd==[%d] closed by time cb_func \n", user_data->sockfd);
}
//若有数据传输 将定时器延后3个单位
// void adjust_timer(util_timer* timer)
// {
//     time_t cur = time(NULL);
//     timer -> expire = cur + 3 * TIMEOUT;
//     timer_lst.adjust_timer(timer);

// }
// void deal_timer(util_timer* timer, int sockfd)
// {
//     timer->cb_func(&users_timer[sockfd]);
//     if(timer)
//     {
//         timer_lst.del_timer(timer);
//     }
// }

//发送给客户端 (错误信息)
void my_web::send_error(int connfd, const char *to_error)
{
    //打印下错误
    printf("%s", to_error);
    size_t len = strlen(to_error);
    send(connfd, to_error, len, 0);
    //报错后关闭和客户端的连接
    close(connfd);
}

// //初始化日志
void my_web::log_write()
{

    if (0 == m_close_log)
    {

        if (1 == m_log_write)
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
        }
    }
}
//初始化连接池
// init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
void my_web::create_mysqlpool()
{
    //创建连接池的单例模型
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
 
    //初始化数据库读取表
    users->mysql_reslut(m_connPool);
}
//初始化线程池
bool my_web::create_threadpool()
{
    //创建线程池和初始化线程池
    m_pool = NULL;
    //单例初始化去要指向null
    try
    {
        m_pool = new my_threadpool<my_http_conn>(m_connPool, m_actor_model);
        LOG_INFO("THE threadpool Created successfully\n");
    }

    catch (const std::exception &e)
    {
        printf("the pthread_pool stop");
        LOG_ERROR("THE threadpool error\n");
        return false;
    }
    return true;
}

//创建用户数组 用于管理
void my_web::create_http_conn()
{
    //预先准备好客户连接分配的http_conn
    users = new my_http_conn[MAX_FD];
    assert(users);
    LOG_INFO("my_http_conn create over\n");
}

//创建监听文件描述符
void my_web::create_listen()
{
    //开始创建连接

    //创建socket套接字
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

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
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    //创建address
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    // inet_pton(AF_INET, , &address.sin_addr);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);
 
    //设置端口复用
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(m_listenfd, 5);
    assert(ret != -1);

    LOG_INFO("listen create successful\n");
 }

//创建epoll树并将m_listen上树
void my_web::create_epoll()
{
    //创建epoll树
    // epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    //将listenfd作为根节点放入epoll树
    //且不做EPOLLONESHOT处理
    epoll_addfd(m_epollfd, m_listenfd, false, m_trig_model);

    my_http_conn::m_epollfd = m_epollfd;
    LOG_INFO("epoll create successful, listen is in the epoll\n");
}

//创建定时器
void my_web::create_timer()
{
    int ret;
    //将pipefd设置为双向
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    setnonblocking_main(m_pipefd[1]);
    // epoll_addfd(m_epollfd, m_pipefd[0], false, m_trig_model);
    epoll_addfd(m_epollfd, m_pipefd[0], false, 0);
    //防止服务器端被关闭
    //添加屏蔽信号

    addsig(SIGPIPE, SIG_IGN);
    //设置信号处理函数
    addsig(SIGALRM, sig_handler); //时钟
    addsig(SIGTERM, sig_handler); //结束进程
    //设置信号处理函数后
    //通过sigaction进行监控
    //当信号传输过来时
    //触发调用输入给pipefd[1]
    //然后pipefd[0]收到, 触发epoll树

    alarm(TIMESLOT);
    //服务器停止运行标志
    bool m_stop_server = false;

    //创建客户数据数组，其中包含了客户对应的定时器信息
    // m_users_timer = new client_data[MAX_FD];
}

//将新的客户放入客户数组中
void my_web::client_init(int connfd, struct sockaddr_in client_address)
{
    //将新的客户放入客户数组中
    //并上树
    users[connfd].init(connfd, client_address, m_trig_model, m_close_log);

    //将客户的数据放入定时器
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    //创建定时器 设置回调函数和超时时间, 绑定定时器和用户数据, 最后将定时器添加到链表timer_lst中
    // util_timer *timer = new util_timer;
    heap_timer *timer = new heap_timer(3 * TIMESLOT);

    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    // timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;

    // m_timer_lst.add_timer(timer);
    m_time_heap.add_timer(timer);
    printf("users_timers\n");
}

//若用户被使用 比如读或写 则更新定时器
// void my_web::adjust_timer(util_timer *timer)
// {
//     time_t cur = time(NULL);
//     timer->expire = cur + 3 * TIMESLOT;
//     m_timer_lst.adjust_timer(timer);

//     LOG_INFO("%s", "adjust timer");

//     printf("adjust timer by read!\n");
// }
void my_web::adjust_timer(heap_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
   // m_time_heap.del_timer(timer);
    m_time_heap.add_timer(timer);

    LOG_INFO("%s", "adjust timer");

    printf("adjust timer by read!\n");
}
//若用户断开连接 则将定时器删除
// void my_web::deal_timer(util_timer *timer, int sockfd)
// {
//     //如果小于等于0 则任务发送错误或断开连接
//     //断开连接
//     // users[sockfd].close_conn();
//     // util_timer *timer = users[sockfd].get_timer();
//     LOG_ERROR(" happened error\n");
//     timer->cb_func(&users_timer[sockfd]);
//     if (timer)
//     {
//         m_timer_lst.del_timer(timer);
//     }
// }
void my_web::deal_timer(heap_timer *timer, int sockfd)
{
    //如果小于等于0 则任务发送错误或断开连接
    //断开连接
    // users[sockfd].close_conn();
    // util_timer *timer = users[sockfd].get_timer();
    LOG_ERROR(" happened error\n");
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        m_time_heap.del_timer(timer);
    }
}

//接收一个用户的请求
bool my_web::accept_client()
{
    //接收新的客户
    // ET模式下要循环读
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd;
    printf("new client\n");
    if (m_trig_model == 1)
    {
        while (1)
        {
            connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            LOG_DEBUG("create new connfd fd==[%d]\n", connfd);
            if (connfd <= 0)
            {
                LOG_DEBUG("error is %d\n", errno);
                break;
            }
            if (my_http_conn::m_client_number >= MAX_FD)
            {
                send_error(connfd, "Internet server busy");
                LOG_ERROR("%s", "Internal sever busy");
                break;
            }
            client_init(connfd, client_address);
        }
        return false;
    }
    else
    {
        connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        LOG_DEBUG("create new connfd fd==[%d]\n", connfd);
        if (connfd <= 0)
        {
            LOG_DEBUG("error is %d\n", errno);
            return false;
        }
        if (my_http_conn::m_client_number >= MAX_FD)
        {
            send_error(connfd, "Internet server busy");
            LOG_ERROR("%s", "Internal sever busy");
            return false;
        }
        client_init(connfd, client_address);
    }
    return true; // LT模式返回true表示流程继续，ET模式读完可以去读下一个
}

//接收到信号后的处理
bool my_web::time_siganl()
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);

    if (ret == -1)
    {
        LOG_ERROR("从管道中接受信号失败,接受失败,failed to accept signal from pipeline, failed to accept!!\n");
        return false;
    }
    else if (ret == 0)
    {
        LOG_ERROR("failed to receive signal from pipeline, no data in pipeline! \n");
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            printf("siganls[%d] is %d go\n", i, signals[i]);
            switch (signals[i])
            {
            case SIGALRM:
            {
                //用timeout变量标记有定时任务需要处理 但不立即处理定时任务,
                //因为定时任务优先级不高, 优先处理其他更重要的任务
                printf("happened SIGALRM\n");
                LOG_ERROR("happened SIGALRM\n");
                m_timeout = true;
                break;
            }

            case SIGTERM:
            {
                printf("happened SIGTERM\n");
                LOG_ERROR("happened SIGTERM\n");
                m_stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}
//处理读事件
void my_web::read_thing(int sockfd)
{
    //读取客户发送来的数据
    //若对于0 则将任务分给请求队列
    //请求队列再发给子线程处理
    LOG_INFO("epollin事件:\n")
    LOG_DEBUG("read_thing\n");

    // util_timer *user_hav_timer = users_timer[sockfd].timer;
    heap_timer *user_hav_timer = users_timer[sockfd].timer;
    printf("read now\n");

    //通过m_actor_model 决定Reactor 0和 proactor 1
    if (m_actor_model == 0)
    {

        if (user_hav_timer)
        {
            adjust_timer(user_hav_timer);
        }
        //就靠这个切换reactor的读写状态了!
        m_pool->increase(users + sockfd, 0);
        if (user_hav_timer)
        {
            adjust_timer(user_hav_timer);
        }

        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(user_hav_timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        if (users[sockfd].read())
        {

            m_pool->increase(users + sockfd);
            if (user_hav_timer)
            {
                adjust_timer(user_hav_timer);
            }
        }
        else
        {
            deal_timer(user_hav_timer, sockfd);
        }
    }
}

//写事件
void my_web::write_thing(int sockfd)
{
    LOG_DEBUG("write_thing\n");
    // util_timer *user_hav_timer = users_timer[sockfd].timer;
    heap_timer *user_hav_timer = users_timer[sockfd].timer;
    printf("write now\n");
    // reactor 和 proactor
    if (m_actor_model == 0)
    {
        if (user_hav_timer)
        {
            adjust_timer(user_hav_timer);
        }
        //就靠这个切换reactor的读写状态了!
        m_pool->increase(users + sockfd, 1);
        if (user_hav_timer)
        {
            adjust_timer(user_hav_timer);
        }

        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(user_hav_timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        if (user_hav_timer)
        {
            // adjust_timer(user_hav_timer);
            adjust_timer(user_hav_timer);
            printf("adjust timer by write if before!\n");
        }
        //根据写的结果 决定是否关闭
        if (users[sockfd].write())
        {
            printf("write\n");
            LOG_DEBUG("write\n");
            // users[sockfd].close_conn();

            if (user_hav_timer)
            {
                // adjust_timer(user_hav_timer);
                adjust_timer(user_hav_timer);
                printf("adjust timer by write! after\n");
            }
        }
        else
        {
            //如果小于等于0 则任务发送错误或断开连接
            //断开连接
            // users[sockfd].close_conn();
            // deal_timer(user_hav_timer, sockfd);
            deal_timer(user_hav_timer, sockfd);
        }
    }
}

//主循环
void my_web::main_loop()
{

    //定时器
    // client_data *users_timer = new client_data[MAX_FD];

    m_timeout = false;
    m_stop_server = false;
    printf("will being\n");
    //开始监听
    while (!m_stop_server)
    {
        LOG_DEBUG("main_loop!\n");
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        printf("epoll\n");
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure!\n");
            break;
        }
        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;

            if (sockfd == m_listenfd)
            {
                bool flag = accept_client();
                if (flag == false)
                {
                    continue;
                }
            }
            //有异常的情况
            else if (events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP))
            {
                //有异常关闭客户端
                LOG_INFO("fd = [%d] have a errno", sockfd);
                if (events[i].events & EPOLLRDHUP)
                {
                    LOG_INFO("EPOLLRDHUP happened");
                    printf("---------EPOLLRDHUP--------");
                }
                if (events[i].events & EPOLLHUP)
                {
                    LOG_INFO("EPOLLHUP happened");
                    printf("---------EPOLLHUP--------");
                }
                if (events[i].events & EPOLLERR)
                {
                    LOG_INFO("EPOLLERR happened");
                    printf("---------EPOLLERR--------");
                }
                printf("accpt error! and close fd \n");
                printf("have errno\n");
                LOG_INFO("\n");
                // util_timer *user_hav_timer = users_timer[sockfd].timer;
                heap_timer *user_hav_timer = users_timer[sockfd].timer;
                deal_timer(user_hav_timer, sockfd);
                // users[sockfd].close_conn();
            }
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int flag = time_siganl();
                if (flag == false)
                {
                    continue;
                }
            }

            //客户端传来数据 触发读操作
            else if (events[i].events & EPOLLIN)
            {
                read_thing(sockfd);
            }
            //收到子进程发来的写操作
            else if (events[i].events & EPOLLOUT)
            {
                write_thing(sockfd);
            }
            //添加其他操作
            //比如定时器
            //比如自身的信号
        }
        printf("timeout is %d\n", m_timeout);
        if (m_timeout)
        {
            time_handler();

            m_timeout = false;
        }
    }
}
