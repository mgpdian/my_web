/*
 * @Date: 2022-03-13 23:51:14
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 04:07:46
 * @FilePath: /data/my_web/log/log.cpp
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;


Log::Log()
{
    m_count = 0;

     
    m_is_async = false;
    m_buf = NULL;
    m_log_queue = NULL;

}

Log::~Log()
{
    if(m_fp != NULL)
    {
        fclose(m_fp);
        m_fp = NULL;
    }
    //释放
    if(m_buf != NULL)
    {
        delete m_buf;
        m_buf = NULL;
    }
    if(m_log_queue != NULL)
    {
        delete m_log_queue;
        m_log_queue = NULL;
    }
}

//队列初始化
//同步不需要设置队列长度 因为同步是一步步完成的
//异步需要设置队列长度
bool Log::init(const char* file_name, int close_log, int log_buf_size,
                int split_lines, int max_queue_size)
{
    //异步
    if(max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string> (max_queue_size);
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
        //设置子线程
    }

    m_close_log = close_log;//关闭日志参数
    m_log_buf_size = log_buf_size;//缓冲区大小
    m_buf = new char[m_log_buf_size];//
    memset(m_buf, '\0', m_log_buf_size);

    m_split_lines = split_lines; //日志最大行数

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    /*C 库函数 char *strrchr(const char *str, int c)
     在参数 str 所指向的字符串中搜索最
    后一次出现字符 c（一个无符号字符）的位置。*/
    const char * p  = strrchr(file_name, '/');
    char log_full_name[256] = {0};
        //相当于自定义日志名
    //若输入的文件名没有/，则直接将时间+文件名作为日志名
    
    if(p == NULL)
    {
        /*C 库函数 int snprintf(char *str, size_t size, const char *format, ...) 设将
        可变参数(...)按照 format 格式化成字符串，并将字符串复制到 str 中，size 为要写入的
        字符的最大数目，超过 size 会被截断。*/
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);

    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);

    }
    m_today = my_tm.tm_mday;
    //以天分类
     /*
    C 库函数 FILE *fopen(const char *filename, const char *mode) 使用给定的模式 mode
    打开 filename 所指向的文件。
    "r"	    打开一个用于读取的文件。该文件必须存在。
    "w"	    创建一个用于写入的空文件。如果文件名称与已存在的文件相同，则会删除已有文件的内容，文件被视为一个新的空文件。
    "a"	    追加到一个文件。写操作向文件末尾追加数据。如果文件不存在，则创建文件。
    "r+"	打开一个用于更新的文件，可读取也可写入。该文件必须存在。
    "w+"	创建一个用于读写的空文件。
    "a+"	打开一个用于读取和追加的文件
    */

    m_fp = fopen(log_full_name, "a"); //追加模式
    if(m_fp == NULL)
    {
        return false;
    }
    return true;
                
                
}
/*
超行、按天分文件逻辑，具体的，

- 日志写入前会判断当前day是否为创建日志的时间，行数是否超过最大行限制

- - 若为创建日志时间，写入日志，否则按当前时间创建新log，更新创建时间和行数
  - 若行数超过最大行限制，在当前日志的末尾加count/max_lines为后缀创建新log

将系统信息格式化后输出，具体为：格式化时间 + 格式化内容

*/
void Log::write_log(int level, const char * format, ...) 
{
    struct timeval now = {0, 0};
    /*头文件：#include <sys/time.h>    #include <unistd.h>

    定义函数：int gettimeofday (struct timeval * tv, struct timezone * tz);

    函数说明：gettimeofday()会把目前的时间有tv 所指的结构返回，当地时区的
    信息则放到tz 所指的结构中。
    */
    gettimeofday(&now, NULL);

    time_t t = now.tv_sec;

    //获取当前时间
    struct tm *sys_tm = localtime(&t); 
    //用t填充tm结构，返回tm结构的指针
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    switch(level)
    {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s,"[info]:");
            break;
        case 2:
            strcpy(s,"[warn]:");
            break;
        case 3:
            strcpy(s,"[erro]:");
            break;
        default:
            strcpy(s,"[info]:");
            break;
    }


    //写入log
    m_mutex.lock();
    m_count++;
    //如果今天的日期不等于记录中今天的日期，或者还没开始记录
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        //表示新的一天 或者是新的日志
        char new_log[256] = {0};
        //刷新m_fp并关闭
        fflush(m_fp);
        fclose(m_fp);

        char tail[16] = {0};
        //格式化日志名中的时间部分
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1990, my_tm.tm_mon + 1, my_tm.tm_mday);

         //如果是时间不是今天,则创建今天的日志，更新m_today和m_count
        if( m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0; 

        }
        else
        {
            //超过了最大行，在之前的日志名基础上加后缀, m_count/m_split_lines//把行数加起来
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log,"a");
    }
    
    m_mutex.unlock();

    //接收参数
    va_list valst;
    va_start(valst, format);
    string log_str;
    m_mutex.lock();

    //写入时间的具体格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);


    //写入具体内容
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);

    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();
    //判断是不是异步日志
    //判断阻塞队列是否满了
    //不满就放入
    if(m_is_async && !m_log_queue -> is_full())
    {
        m_log_queue->push(log_str);
    } 
    else{
        //C 库函数 int fputs(const char *str, FILE *stream) 把
        //字符串写入到指定的流 stream 中，但不包括空字符。
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}   


void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入缓存流
    fflush(m_fp);
    m_mutex.unlock();
}
