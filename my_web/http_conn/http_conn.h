/*
 * @Date: 2022-03-10 08:38:11
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-11 03:52:16
 * @FilePath: /data/my_web/http_conn/http_conn.h
 */
//http 对http行进行处理
#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <map>

//http状态分析类
class my_http_conn
{
public:
    //限定:文件名的最大长度
    static const int MAX_FILENAME_LENGTH = 200;
	//读缓冲区的大小
    static const int READ_BUFFER_SIZE = 2048;
	//写缓冲区的大小
    static const int WRITE_BUFFER_SIZE =1024;
//enum类:
//各种状态enum
		//HTTP请求方式:
        enum REQUEST_METHOD
        {
            GET = 0,
            POST,
            HEAD,
            PUT, 
            DELETE,
            TRACE,
            OPTIONS, 
            CONNECT, 
            PATCH
        };
		//有限状态机的状态
        //这是解析用户的请求时,状态机的状态
        enum CHECK_STATE
        {
            CHECK_STATE_LINE = 0, //分析请求行
            CHECK_STATE_HEADER, //分析请求头
            CHECK_STATE_CONTENT //分析内容(一般没用?)
        };

        //服务器处理HTTP请求的可能结果
        enum HTTP_RETURN
        {
            NO_REQUEST, //请求不完整 需要继续读取
            GET_REQUEST, //得到一个完整的请求
            BAD_REQUEST, //有错误输入
            NO_RESOURCE, //没有要求的资源
            FORBIDDEN_REQUEST, //没有足够的权限
            FILE_REQUEST, //文件请求成功
            INTERNAL_ERROR, //服务器内部错误
            CLOSED_CONNECTION //客户端关闭连接
        };        


		//服务器读取HTTP请求的每行的可能结果
        enum LINE_STATUS
        {
            LINE_OK = 0, //读取到一个完整的行
            LINE_BAD, //行出错
            LINE_OPEN //行数据不完整
        };		
public:
	//构造 析构
    my_http_conn(){}

    ~my_http_conn(){}
	
public:    
    //初始化新接收的连接
    void init(int sockfd, const sockaddr_in& addr);
	//关闭连接
    void close_conn(bool real_close = true);
	//处理客户请求()
    void process();
	//读操作
    bool read();
	//写操作
    bool write();

private:
	//初始化连接
    void init();
	//解析客户的HTTP请求
    HTTP_RETURN process_read();    
	//填充返回的HTTP响应
    bool process_write(HTTP_RETURN ret);


	//通过一组函数来被上面的process_read()调用来分析HTTP请求
   
    //得到当前要开始读取的位置
    char * get_line()
    {
        return m_read_buf + m_start_line;
    }
    LINE_STATUS parse_line();

	 HTTP_RETURN parse_request_line(char* text);
    HTTP_RETURN parse_headers(char* text);
    HTTP_RETURN parse_content(char* text);
    //当得到一个完整 正确的HTTP请求时 我们就分析目标文件的属性        
    //如果目标文件存在 对所有用户可读 且不是目录 则使用mmap将其映射到
    //内存地址m_file_address中 并告诉调用者获得文件成功
    //文件共享
    HTTP_RETURN do_request();//开始写
    

	//通过一组函数被process_write()调用来填充HTTP请求
    bool add_response(const char* format, ...);

    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    //发送是否保持长连接
    bool add_linger();
    //发送空行
    bool add_blank_line();
    bool add_content(const char* content);    

    //关闭mmap
    void unmap();
public:
	//静态变量

	//所有socket上的事件都被注册到同一个epoll内核事件上

	//因此要将epoll文件描述符设置为静态的
    static int m_epollfd;


	//统计用户数量
    static int m_client_number;
	

private:

	//该HTTP连接的socket和对方的socket地址
    int m_sockfd;
    sockaddr_in m_address;
	
    //HTTP协议版本号 仅支持HTTP/1.1
    char* m_version;
	//主机名
    char* m_host;

	//读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
	//标识读缓冲中已经读入的数据的最后一个字节的下一个位置
    
	//理解错误
    //应该是 读缓冲时的指针
    //也就是说是用户请求数据的总大小
    int m_read_idx;
    

	//当前正在解析的字符在读缓冲区的位置
    //m_check_idx在每次读行数据的时候会增长
    int m_check_idx;
	//当前正在解析的行起始位置
    int m_start_line;
	

	//写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
	//写缓冲区中待发送的字节数
    char m_write_idx;



	//状态机机当前的状态
    CHECK_STATE m_check_state;
	//请求方法
    REQUEST_METHOD m_method;
	

	//客户请求的目标文件的完整路径 内容等于 dor_root + m_url
    
	// doc_root是网站根目录
    char m_real_file[MAX_FILENAME_LENGTH];


	//客户请求的目标文件的文件名
    char* m_url;


	

	//HTTP请求的消息体长度
    //用户发送过来的数据消息长度 比如密码账号
    int m_content_length;
	//HTTP请求是否要求保持连接
    bool m_linger;


	//客户请求的目标文件被mmap到内存中的起始位置
    char* m_file_address;
	
	//目标文件的状态
    struct stat m_file_stat;
	//通过它我们可以判断文件是否存在 是否为目录,是否可读,并获取文件大小等信息



   //采用writev来执行写操作 

    //因此定义下面俩个成员

    //其中m_iv_count表示被写内存块的数量
    struct iovec m_iv[2]; //一个是请求行和头  一个是请求的内容
    int m_iv_count;

};


#endif