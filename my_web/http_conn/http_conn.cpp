/*
 * @Date: 2022-03-11 00:06:04
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 08:51:17
 * @FilePath: /data/my_web/http_conn/http_conn.cpp
 */

//解析http类 http_conn

#include "http_conn.h"
#include "../log/new_log.h"

static Logger::ptr g_logger = MY_LOG_NAME("system");
//定义HTTP响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n";

//网站根目录
const char *doc_root = "/home/mgpdian/my_web/html";


std::map<std::string, std::string> users;
//LRU缓存
static LRUCache lru(100);
//数据库连接
void my_http_conn::mysql_reslut(connection_pool *connPool) //获取数据库结果
{
    //先从连接池取一个连接
    //疑问: 之前不是取过吗
    //这是初始化数据库读取表 和thread_pool的不一样
    
    MYSQL *mysql = NULL;
    connectionRAII mysqlconn(&mysql, connPool);
    //mysql_query(mysql,"SET NAMES GBK");
    //mysql_set_character_set(mysql,"utf8"); 
    //mysql_query(mysql, "SET NAMES GB2312");
    //通过sql语句搜索username和passwd数据
    //printf("在数据库的user中搜索username和passwd的数据\n");
    // mysql_query 数据库搜索语句
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        //LOG_ERROR("SELECT error:%s\n", mysql_errno(mysql));
        MY_LOG_ERROR(g_logger) << "SELECT error: " << mysql_errno(mysql);
    }

    //从表中检索完整的结果集
    //printf("从表中检索完整的结果集\n");
    // mysql_store_result 获取完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //printf("返回结果集中的列数\n");
    // mysql_num_fields 返回的结果集的列数
    if (result == NULL){
        MY_LOG_ERROR(g_logger) << "result is empty";
    }
        //printf("result is empty\n");
    int num_filelds = mysql_num_fields(result);

    //printf("返回所有字段结构的数组\n");
    // mysql_fetch_fields 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_field(result);

    //从结果集中获得下一行
    //printf("从结果集中获取下一行,将对应的用户名和密码,存入map中\n");
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1 = row[0];
        string temp2 = row[1];

       
       // cout << temp1 << endl << temp2<< endl;
       // printf("%s %s \n", m , m1);
       users[temp1] = temp2;
    }
}

//设置非阻塞
int setnonblocking(int fd)
{
    //增加作用于fd 不用转换
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
//在http_conn中完成上树
void epoll_addfd(int epollfd, int fd, bool epoll_one_shot, int trig_model)
{
    epoll_event event;
    event.data.fd = fd;
    // trig_model == 1 表示是ET触发模式
    // trig_model == 0 表示是LT触发模式
    if (trig_model == 1)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    //当socket接收到对方关闭连接时的请求之后触发，
    //有可能是TCP连接被对方关闭，
    //也有可能是对方关闭了写操作。
    if (epoll_one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //设置非阻塞
    setnonblocking(fd);
}

//下树加关闭文件描述符
void epoll_removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//当客户任务完成后
//理解错误
//应该是 即使在EPOLLONESHOT的情况下也 触发一次ev
void epoll_modfd(int epollfd, int fd, int ev, int trig_model)
{
    epoll_event event;
    event.data.fd = fd;
    if(trig_model == 1)
    {
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }
    else
    {

        
        event.events = ev |  EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
//静态函数要在类外初始化
int my_http_conn::m_client_number = 0;
int my_http_conn::m_epollfd = -1;

//初始化新接收的连接
void my_http_conn::init(int sockfd, const sockaddr_in &addr, int trig_model, int close_log)
{
    m_sockfd = sockfd;
    //这是线程当前负责的客户端

    m_address = addr;

    char _ip[64];
    inet_ntop(AF_INET, &m_address.sin_addr, _ip, sizeof(_ip));
    //将传输来的用户ip转换成主机字节序

    //LOG_INFO("接入 ip = [%s] fd == [%d]\n", _ip, sockfd);
    MY_LOG_INFO(g_logger) << "Access ip = [" << _ip << "] fd == [" << sockfd << "]";
    m_trig_model = trig_model;
    m_close_log = close_log;

    //下俩行是为了避免TIME_WAIT 仅用于测试也就是端口复用
    //可能导致在TIME_WAIT的返回数据无了
    // int reuse = 1;
    // setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    epoll_addfd(m_epollfd, sockfd, true, m_trig_model);
    //用户连接数加一
    ++m_client_number;
    
    init(); 
}
//初始化连接
void my_http_conn::init()
{
    m_check_state = CHECK_STATE_LINE;
    m_linger = false;
    // m_linger = true;
    m_method = GET;
    m_url = 0;
    // m_pFile = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_state = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', MAX_FILENAME_LENGTH);

    bytes_have_send = 0;
    bytes_to_send = 0;
    m_resquest_data = NULL;
    mysql = NULL;

    timer_flag = 0;
    improv = 0;
}
//关闭客户端连接
void my_http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        epoll_removefd(m_epollfd, m_sockfd);
        //让客户连接变-1
        m_sockfd = -1;
        //将当前所有客户总数减1
        --m_client_number;
    }
}

//核心
//处理客户请求()
//线程池中的工作线程调用 这是处理HTTP请求的入口函数
void my_http_conn::process()
{
    HTTP_RETURN read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        //数据没读完 重置EPOLLONESHOT继续读
        epoll_modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_model);
        return;
    }
    //写入写缓冲
    bool write_ret = process_write(read_ret);

    if (!write_ret)
    {
        //printf("process_write fail, fd == [%d] closed\n", m_sockfd);
        MY_LOG_ERROR(g_logger) << "process_write fail, fd == [" << m_sockfd << "] closed";
        close_conn();
    }
    //切换状态
    epoll_modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_model);
}
//读操作
//循环读取客户数据 直到无数据可读或者对方关闭连接
bool my_http_conn::read()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    if (m_trig_model == 0) // LT模式
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                          READ_BUFFER_SIZE - m_read_idx, 0);

        if (bytes_read <= 0)
        {
            //printf("LT read() EAGAIN\n");
            MY_LOG_ERROR(g_logger) << "LT read() EAGAIN";
            return false;
        }
        

        m_read_idx += bytes_read;
    }
    // ET
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                              READ_BUFFER_SIZE - m_read_idx, 0);

            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    //在某些套接字的函数操作不能立即完成时 会出现错误码EWOULDBLOOK或者EAGAIN
                    if (errno == EAGAIN)
                    {
                        //printf("read() EAGAIN\n");
                        MY_LOG_INFO(g_logger) << "read() EAGAIN";
                    }
                        
                    if (errno == EWOULDBLOCK){
                        //printf("read() EWOULDBLOCK\n");
                        MY_LOG_INFO(g_logger) << "read() EWOULDBLOCK";
                    }
                        
                    break;
                }
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }

            m_read_idx += bytes_read;
        }
    }

    return true;
}

//有限状态机
//解析客户的HTTP请求
my_http_conn::HTTP_RETURN my_http_conn::process_read()
{
    //设置默认
    LINE_STATUS line_status = LINE_OK;
    HTTP_RETURN ret = NO_REQUEST;
    char *text = 0;
    while (((m_check_state == CHECK_STATE_CONTENT) &&
            (line_status == LINE_OK)) ||
           ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_check_idx;
        //printf("got 1 http line: %s\n", text);
        //MY_LOG_INFO(g_logger) << "got 1 http line: " << text;
        switch (m_check_state)
        {
        case CHECK_STATE_LINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            if (ret == GET_REQUEST)
            {
                // do_request() 查询文件是否存在
                //若存在则发送文件
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
            {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
        {
            return INTERNAL_ERROR;
        }
        }
    }
    return NO_REQUEST;
}

//通过一组函数来被上面的process_read()调用来分析HTTP请求

//获取一行
my_http_conn::LINE_STATUS my_http_conn::parse_line()
{
    char temp;
    for (; m_check_idx < m_read_idx; ++m_check_idx)
    {
        temp = m_read_buf[m_check_idx];
        if (temp == '\r')
        {
            if ((m_check_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_check_idx + 1] == '\n')
            {
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if ((m_check_idx > 1) && (m_read_buf[m_check_idx - 1] == '\r'))
            {
                m_read_buf[m_check_idx - 1] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
//解析HTTP请求行 获得请求的方法 目标url HTTP版本号
my_http_conn::HTTP_RETURN my_http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    //检索字符串 str1 中第一个匹配字符串 str2 中字符的字符，不包含空结束字符。
    //也就是说，依次检验字符串 str1 中的字符，
    //当被检验字符在字符串 str2 中也包含时，则停止检验，并返回该字符位置。
    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
    //请求行中最先含有空格和\t任一字符的位置并返回
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        m_is_post = 1;
    }
    else
    {
        return BAD_REQUEST;
    }
    // m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
    //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符
    // strspn检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
    {
        return BAD_REQUEST;
    }

    *m_version++ = '\0';
    //检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_version += strspn(m_version, " \t");

    //if (strcasecmp(m_version, "HTTP/1.1") != 0)
    if (strcasecmp(m_version, "HTTP/1.1") != 0 && strcasecmp(m_version, "HTTP/1.0") != 0)
    {
        return BAD_REQUEST;
    }
    //检查url是否合法
    //这里的url是客户请求
    //也就是第一行的第二端
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        //在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
    //当url为/时 显示默认主页面
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "choice.html");
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST; //让循环继续
}
//分析请求头
my_http_conn::HTTP_RETURN my_http_conn::parse_headers(char *text)
{
    //一开始就遇到'\0'表示请求头分析完毕
    if (text[0] == '\0')
    {
        //如果http有消息体 则需要读取m_content_length字节的消息体
        //将状态机变成CHECK_STATE_CONTENT状态
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经到了一个完整的HTTP请求
        return GET_REQUEST;
    }
    //开始处理头部字段
    //表示请求长连接
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        //索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
       // printf("the client want to have a long keep? %d", m_linger);
    }
    //处理content-length字段
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    //处理HOST头部
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        //printf("oop ! unknow header %s\n", text);
        MY_LOG_ERROR(g_logger) << "oop ! unknow header" << text;
    }
    return NO_REQUEST;
}
//我们没有真正解析HTTP请求的信息体
//只是判断它是否被完整的读入//
//改进
//加入post 需要读取 后面的内容
my_http_conn::HTTP_RETURN my_http_conn::parse_content(char *text)
{
    //printf("http后面的请求内容");
    if (m_read_idx >= (m_content_length + m_check_idx))
    {
        //printf("内容行的内容为：%s\n", text);
        text[m_content_length] = '\0';
        m_resquest_data = text; //读取剩下的内容行
        //printf("内容行的内容为：%s", m_resquest_data);
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
//当得到一个完整 正确的HTTP请求时 我们就分析目标文件的属性
//如果目标文件存在 对所有用户可读 且不是目录 则使用mmap将其映射到
//内存地址m_file_address中 并告诉调用者获得文件成功
//文件共享
my_http_conn::HTTP_RETURN my_http_conn::do_request()
{
    strcpy(m_real_file, doc_root); // doc_root是网站的根目录 在最上面就定义了
    int len = strlen(doc_root);
    char *pFile = m_url;

    //转换汉字编码
    //不需要
    // if(strlen(m_url)<=1)
    //{
    //	strcpy(pFile, "./");
    //}
    // else
    //{
    //	pFile = m_url +1;
    //}
    /// printf("[%s]\n", m_url);
    // strdecode(pFile, pFile);
    // printf("[%s]\n", pFile);
    const char *p = strrchr(m_url, '/');

    //处理登录界面 和注册界面 load or register
    if (m_method == POST && (*(p + 1) == '2') || *(p + 1) == '3')
    {

        //???
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        
        //printf("加载登录界面load\n");
        char name[100], password[100];
        int i = 0;
        //读取m_resquest_data中的数据 users 和 password
        // user= 长度为5 所以i = 5开始计算
        for (i = 5; m_resquest_data[i] != '&'; ++i)
        {
            name[i - 5] = m_resquest_data[i];
        }
        name[i - 5] = '\0';
        //printf("用户账号为:%s\n", name);

        // password= 长度为9 加上之前的& 长度为10
        int j = 0;
        for (i = i + 10; m_resquest_data[i] != '\0'; ++i, ++j)
        {
            password[j] = m_resquest_data[i];
        }
        password[j] = '\0';
        //printf("用户密码为:%s\n", password);

        //printf("登录\n");
        //printf("*(p+1) == %c", *(p+ 1));
        if (*(p + 1) == '2')
        {
            //printf("???");
            if (users.find(name) != users.end() && users[name] == password)
            {
                //printf("YES?");
                lru.put(m_address.sin_addr.s_addr, name);
                 strcpy(m_url, "/index.html");

            }
            else
            {
                //printf("NO?");
                strcpy(m_url, "/logerror.html");
            }
        }
        else if (*(p + 1) == '3')
        {
            //printf("注册\n");
            //注册的话 需要先查看数据库中是否有重名
            if (users.find(name) == users.end())
            {

                //写sql语句
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");

                int res = 0;
                //m_locker.lock();
                {
                    std::unique_lock<MutexType> lock(the_mutex);
                    res = mysql_query(mysql, sql_insert);
                    users.insert(pair<string, string>(name, password));
                }
                //printf("语句插入: [%s]\n", sql_insert);
                //写入数据库
                
                //printf("插入成功");
                //m_locker.unlock();

                if (!res)
                {
                    //printf("注册成功 跳转到登录页面");
                    strcpy(m_url, "/log.html");
                }
                else
                {
                    //printf("注册失败 数据库读取失败\n");
                    strcpy(m_url, "/registerError.html");
                }
            }
            else
            {
                //printf("注册失败 已经有重名\n");
                MY_LOG_ERROR(g_logger) << "Registration failed. There is already a duplicate name";
                strcpy(m_url, "/registerError.html");
            }
        }
    }
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '4')
    {
        if(lru.get(m_address.sin_addr.s_addr) == "")
        {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/nolog.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }
        else
        {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/index.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/think.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/study.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '8')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/choice.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, MAX_FILENAME_LENGTH - len - 1);

    // strncpy(m_real_file + len, pFile, MAX_FILENAME_LENGTH - len - 1);
    // strncpy(m_real_file + len, m_url, MAX_FILENAME_LENGTH - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0)
    {
        //获取不到
        return NO_REQUEST;
    }
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        //用户权限不足
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }
    /*
    #inlcude<sys/mann.h>
    void mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
    int munmap(void *start, size_t length);

    void *start 允许用户使用某一个人特定的地址为有这段内存的起始位置。如果他被设置为NULL，则系统自动分配一个地址。
    size_t length 此参数制定了内存段的长度
    int prot 此参数设置内存段访问权限：
            PROT_READ:可读
            PROT_WRITE:可写
            PROT_EXEC:可执行
            PROT_NONE:内存段不能被访问
    int flags 此参数控制内存段内容被修改后程序的行为。它可以被设置为以下值的按位或（MAP_SHARED和MAP_PRIVATE是互斥的，不能同时指定）
            MAP_SHARED:在进程间共享这段内存。对该内存段的修改将反应到被映射的文件中。它提供了进程间共享内存的POSIX方法
            MAP_PRIVATE:内存段调用为进程私有，对该内存段的修改不会反应到被映射的文件中
            MAP_ANONYMOUS:这段内存不是从文件映射而来的，其内容被初始化为全0，这种情况下，mmap函数的最后两个参数将被忽略
            MAP_FIXED:内存段必须位于start参数指定的地址处。start必须是内存页面大小（4096）的整数倍
            MAP_HUGETLB:按照大内存页面来分配内存空间。大内存页面的大小可以通过/pro/meminfo文件来查看
    int fd 此参数是被映射文件对应的文件描述符。他一般通过open系统调用获得。
    off_t offset此参数设置从文件的何处开始映射（对于不需要读入整个文件的情况）

    mmap函数成功时返回指向目标内存区域的指针，失败则返回MAO_FAILED((void*)-1)并设置errno

    munmap函数成功返回0.失败返回-1并设置errno

    */
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ,
                                  MAP_PRIVATE, fd, 0);

    close(fd);
    return FILE_REQUEST;
}

//写操作
bool my_http_conn::write()
{
    int temp = 0;
    // int bytes_have_send = 0;
    // int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0)
    {
        epoll_modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_model);
        init();
        return true;
    }

    // while(1)
    // {
    //     temp = writev(m_sockfd, m_iv, m_iv_count);
    //     if(temp <= -1)
    //     {
    //         //如果TCP写缓冲没有空闲 则得到下一轮EPOLLOUT事件
    //         //虽然在此期间 服务器无法立即接收到同一客户的下一个请求
    //         //但是这可以保证连接的完整性
    //         if(errno == EAGAIN)
    //         {
    //             epoll_modfd(m_epollfd, m_sockfd, EPOLLOUT);
    //             return true;
    //         }

    //         unmap();
    //         return false;
    //     }
    //     bytes_to_send -= temp;
    //     bytes_have_send += temp;
    //     if(bytes_to_send <= bytes_have_send)//感觉是bug 为什么这样?
    //     {
    //         //发送HTTP响应成功 根据HTTP请求中的Connection字段决定是否立即关闭连接
    //         unmap();
    //         if(m_linger)
    //         {
    //             init();
    //             epoll_modfd(m_epollfd, m_sockfd, EPOLLIN);
    //             return true;
    //         }
    //         else
    //         {
    //             epoll_modfd(m_epollfd, m_sockfd, EPOLLIN);
    //             return false;
    //         }

    //     }
    // }

    //解决大文件传输问题
    //因为之前是俩个iv重复传入
    //会导致头部信息重复输入
    while (true)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp < 0)
        {
            //printf("temp < 0 errno = %d\n", errno);
            if (errno == EAGAIN)
            {
                //如果TCP写缓冲没有空闲 则得到下一轮EPOLLOUT事件
                //虽然在此期间 服务器无法立即接收到同一客户的下一个请求
                //但是这可以保证连接的完整性
                epoll_modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_model);
                return true;
            }
            //printf("unmap()\n");
            unmap();
            return false;
        }

        //已经写入的temp字节数
        bytes_have_send += temp;

        //要发送的temp字节数文件
        bytes_to_send -= temp;
       // printf("have: %d  to  %d\n", bytes_have_send, bytes_to_send);
        //如果可以发送的字节大于报头 证明报头发送完毕 但文件还未发送完毕
        /*这行代码：因为m_write_idx表示为待发送文件的定位点，m_iv[0]指向m_write_buf，
        所以bytes_have_send（已发送的数据量） - m_write_idx（已发送完的报头中的数据量）
        就等于剩余发送文件映射区的起始位置*/
        // m_file_address 是文件位置
        // bytes_have_send 是已经发送的字节数
        // m_write_idx 是刚才iv[0]的字节数
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
            //头部已经完成发送
            //printf("headers overs\n");
        }
        //否则继续发送报头 修改m_iv指向写缓冲区的位置以及待发送的长度以便下次接着发
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
            //头部还有剩余
            //printf("headers ever\n");
        }

        if (bytes_to_send <= 0)
        {
            //发送完毕，恢复默认值以便下次继续传输文件
            //printf("overover\n");
            unmap();
            epoll_modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_model);
            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}
//根据服务器端处理HTTP请求结果 决定返回给客户端的内容
//填充返回的HTTP响应
bool my_http_conn::process_write(HTTP_RETURN ret)
{

    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        //服务器内部错误
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
        {
            return false;
        }
        break;
    }
    case BAD_REQUEST:
    {
        //输入错误
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        if (!add_content(error_400_form))
        {
            return false;
        }
        break;
    }
    case NO_RESOURCE:
    {
        //没有请求的资源
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
        {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        //没有足够的权限
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
        {
            return false;
        }
        break;
    }
    case FILE_REQUEST:
    {
        //文件请求成功
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            //还需传入的数据字节
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
            {
                return false;
            }
        }
        break;
    }
    default:
    {
        return false;
    }
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

//通过一组函数被process_write()调用来填充HTTP请求
bool my_http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    // VA_LIST 是在C语言中解决变参问题的一组宏
    /*
    VA_LIST的用法：
       （1）首先在函数里定义一具VA_LIST型的变量，这个变量是指向参数的指针；
       （2）然后用VA_START宏初始化变量刚定义的VA_LIST变量；
       （3）然后用VA_ARG返回可变的参数，VA_ARG的第二个参数是你要返回的参数的类型（如果函数有多个可变参数的，依次调用VA_ARG获取各个参数）；
       （4）最后用VA_END宏结束可变参数的获取。
使用VA_LIST应该注意的问题：
   （1）可变参数的类型和个数完全由程序代码控制,它并不能智能地识别不同参数的个数和类型；
   （2）如果我们不需要一一详解每个参数，只需要将可变列表拷贝至某个缓冲，可用vsprintf函数；
   （3）因为编译器对可变参数的函数的原型检查不够严格,对编程查错不利.不利于我们写出高质量的代码；

    */
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx,
                        WRITE_BUFFER_SIZE - 1 - m_write_idx,
                        format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool my_http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool my_http_conn::add_headers(int content_length)
{
    return                                    // add_file_type(get_mime_type(m_url)) &&
        add_content_length(content_length) && //文件大小
        add_linger() &&                       //是否长连接
        add_blank_line();                     //空行
}
bool my_http_conn::add_file_type(const char *content_filetype)
{
    return add_response("Content-Type:%s\r\n", content_filetype);
}
bool my_http_conn::add_content_length(int content_length)
{
    return add_response("Content-Length:%d\r\n", content_length);
}
//发送是否保持长连接
bool my_http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
//发送空行
bool my_http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool my_http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

//关闭mmap
//对内存映射区执行munmap操作
void my_http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
