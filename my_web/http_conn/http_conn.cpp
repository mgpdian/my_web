/*
 * @Date: 2022-03-11 00:06:04
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-11 07:55:03
 * @FilePath: /data/my_web/http_conn/http_conn.cpp
 */


//解析http类 http_conn


#include "http_conn.h"


//定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

//网站根目录
const char* doc_root = "/data/www/html";


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
void epoll_addfd(int epollfd, int fd, bool epoll_one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    //当socket接收到对方关闭连接时的请求之后触发，
    //有可能是TCP连接被对方关闭，
    //也有可能是对方关闭了写操作。
    if(epoll_one_shot)
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
void epoll_modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
//静态函数要在类外初始化
int my_http_conn::m_client_number = 0;
int my_http_conn::m_epollfd = -1;


    //初始化新接收的连接
void my_http_conn::init(int sockfd, const sockaddr_in& addr)
{
    m_sockfd = sockfd;
    //这是线程当前负责的客户端

    m_address = addr;
    //下俩行是为了避免TIME_WAIT 仅用于测试也就是端口复用
    //可能导致在TIME_WAIT的返回数据无了
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    epoll_addfd(m_epollfd, sockfd, true);
    //用户连接数加一
    ++m_client_number;

    init();
}
    //初始化连接
void my_http_conn::init()
{
    m_check_state = CHECK_STATE_LINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', MAX_FILENAME_LENGTH);
}
	//关闭客户端连接
void my_http_conn::close_conn(bool real_close )
{
    if(real_close && (m_sockfd != -1))
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
    if(read_ret == NO_REQUEST)
    {
        //数据没读完 重置EPOLLONESHOT继续读
        epoll_modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    //写入写缓冲
    bool write_ret = process_write(read_ret);

    if(!write_ret)
    {
        close_conn();
    }
    //切换状态
    epoll_modfd(m_epollfd, m_sockfd, EPOLLOUT);
    
}
	//读操作
    //循环读取客户数据 直到无数据可读或者对方关闭连接
bool my_http_conn::read()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while(true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - 1 - m_read_idx, 0);

        if(bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                //在某些套接字的函数操作不能立即完成时 会出现错误码EWOULDBLOOK或者EAGAIN
                break;
            }
            return false;
        }
        else if(bytes_read == 0)
        {
            return false;
        }
        
        m_read_idx += bytes_read;

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
       char * text = 0;
       while(((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
            || ((line_status = parse_line()) == LINE_OK))
            {
                text = get_line();
                m_start_line = m_check_idx;
                printf("got 1 http line: %s\n", text);


                switch(m_check_state)
                {
                    case CHECK_STATE_LINE:
                    {
                        ret = parse_request_line(text);
                        if( ret == BAD_REQUEST)
                        {
                            return BAD_REQUEST;
                        }
                        break;
                    }
                    case CHECK_STATE_HEADER:
                    {
                        ret = parse_headers(text);
                        if( ret == BAD_REQUEST)
                        {
                            return BAD_REQUEST;
                        }
                        if( ret == GET_REQUEST)
                        {
                            //do_request() 查询文件是否存在
                            //若存在则发送文件
                            return do_request();
                        }
                        break;
                    }
                    case CHECK_STATE_CONTENT:
                    {
                        ret = parse_content(text);
                        if( ret == GET_REQUEST)
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
    for(; m_check_idx <= m_read_idx; ++m_check_idx)
    {
        temp = m_read_buf[m_check_idx];
        if(temp == '\r')
        {
            if((m_check_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_check_idx + 1] == '\n')
            {
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if((m_check_idx > 1) && (m_read_buf[m_check_idx - 1] == '\r'))
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
my_http_conn::HTTP_RETURN my_http_conn::parse_request_line(char* text)
{
    m_url = strpbrk(text, " \t");
     //检索字符串 str1 中第一个匹配字符串 str2 中字符的字符，不包含空结束字符。
    //也就是说，依次检验字符串 str1 中的字符，
    //当被检验字符在字符串 str2 中也包含时，则停止检验，并返回该字符位置。
    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
     //请求行中最先含有空格和\t任一字符的位置并返回
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    
    char* method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else
    {
        return BAD_REQUEST;
    }
    //m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
    //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符
    //strspn检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。    
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    
    *m_version++ = '\0';
    //检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_version += strspn(m_version, " \t");

    if(strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    //检查url是否合法
     //这里的url是客户请求
     //也就是第一行的第二端
     if(strncasecmp(m_url, "http://", 7) == 0)
     {
         m_url += 7;
         //在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
         m_url = strchr(m_url, '/');
     }
     if(!m_url || m_url[0] != '/')
     {
         return BAD_REQUEST;
     }
     m_check_state = CHECK_STATE_HEADER;
     return NO_REQUEST;//让循环继续

}
//分析请求头
my_http_conn::HTTP_RETURN my_http_conn::parse_headers(char* text)
{
    //一开始就遇到'\0'表示请求头分析完毕
    if(text[0] == '\0')
    {
        //如果http有消息体 则需要读取m_content_length字节的消息体
        //将状态机变成CHECK_STATE_CONTENT状态
        if( m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经到了一个完整的HTTP请求
        return GET_REQUEST;
    }
    //开始处理头部字段
    //表示请求长连接
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        //索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    //处理content-length字段
    else if(strncasecmp(text, "Content-Length", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    //处理HOST头部
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else{
        printf("oop ! unknow header %s\n", text);
    }
    return NO_REQUEST;

}
//我们没有真正解析HTTP请求的信息体 
//只是判断它是否被完整的读入//
my_http_conn::HTTP_RETURN my_http_conn::parse_content(char* text)
{
    if(m_read_idx >= (m_content_length + m_check_idx))
    {
        text[m_content_length] = '\0';
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
    strcpy(m_real_file, doc_root);//doc_root是网站的根目录 在最上面就定义了
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, MAX_FILENAME_LENGTH - len - 1);

    if(stat(m_real_file, &m_file_stat) < 0)
    {
        //获取不到
        return NO_REQUEST;
    }
    if(!(m_file_stat.st_mode & S_IROTH))
    {
        //用户权限不足
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, 
    MAP_PRIVATE, fd, 0);

    close(fd);
    return FILE_REQUEST;
    

}
    
	//写操作
bool my_http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if(bytes_to_send == 0)
    {
        epoll_modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp <= -1)
        {
            //如果TCP写缓冲没有空闲 则得到下一轮EPOLLOUT事件
            //虽然在此期间 服务器无法立即接收到同一客户的下一个请求
            //但是这可以保证连接的完整性
            if(errno == EAGAIN)
            {
                epoll_modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }

            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if(bytes_to_send <= bytes_have_send)//感觉是bug 为什么这样?
        {
            //发送HTTP响应成功 根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger)
            {
                init();
                epoll_modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else
            {
                epoll_modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }

        }
    }
}
//根据服务器端处理HTTP请求结果 决定返回给客户端的内容
	//填充返回的HTTP响应
bool my_http_conn::process_write(HTTP_RETURN ret)
{
    
    switch((ret))
    {
        case INTERNAL_ERROR:
        {
            //服务器内部错误
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
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
            if(!add_content(error_400_form))
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
            if(!add_content(error_404_form))
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
            if(!add_content(error_403_form))
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            //文件请求成功
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else
            {
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
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
    return true;
}

	//通过一组函数被process_write()调用来填充HTTP请求
bool my_http_conn::add_response(const char* format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    //VA_LIST 是在C语言中解决变参问题的一组宏
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
   int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx,
                        format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list);
    return true;
}

bool my_http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);

}
bool my_http_conn::add_headers(int content_length)
{
    add_content_length(content_length);//文件大小
    add_linger();//是否长连接
    add_blank_line();//空行
}
bool my_http_conn::add_content_length(int content_length)
{
    return add_response("Content-Length: %d\r\n", content_length); 
}
    //发送是否保持长连接
bool my_http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");

}
    //发送空行
bool my_http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool my_http_conn::add_content(const char* content) 
{
    return add_response("%s", content);
}  

    //关闭mmap
    //对内存映射区执行munmap操作
void my_http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}