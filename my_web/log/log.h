/*
 * @Date: 2022-03-13 23:51:04
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 01:03:49
 * @FilePath: /data/my_web/log/log.h
 */
//日志类

#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"


using namespace std;



class Log
{
public:
    //懒汉模式的单例模型
    //创建唯一实例 
    static Log * get_instance()
    {
        static Log  instance;
        return &instance;
    }
    //执行写入
    static void* flush_log_thread(void *args)
    {
        //get_instance为创建唯一实例
        Log::get_instance() -> async_write_log();
    }
    //初始化日志
    bool init(const char* file_name, int close_log, 
    int log_buf_size = 8192, int split_line = 5000000, 
     int max_queue_size = 0);

    //将输出内容按照标准格式整理
    void write_log(int level, const char* format, ...);
    
    //刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();
    //队列写入日志
    void *async_write_log()
    {
        string single_log;
        //从阻塞队列中取出一个日志string 写入文件
        while(m_log_queue -> pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();   
        }
    }
    

private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines; //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count; //日志行数记录
    int m_today; //日志日期    按天分类
    FILE *m_fp; //文件log的指针
    char *m_buf;    //缓冲区

    block_queue<string> *m_log_queue; //阻塞队列
    bool m_is_async;  //是否同步标志位 同步异步
    my_locker m_mutex;
    int m_close_log; //关闭日志

};
/*
#### **可变参数宏__VA_ARGS__**

__VA_ARGS__是一个可变参数的宏，定义时宏定义中参数列表的最后一个参数为省略号，在实际使用时会发现有时会加##，有时又不加。

```
1//最简单的定义
2#define my_print1(...)  printf(__VA_ARGS__)
3
4//搭配va_list的format使用
5#define my_print2(format, ...) printf(format, __VA_ARGS__)  
6#define my_print3(format, ...) printf(format, ##__VA_ARGS__)
```

__VA_ARGS__宏前面加上##的作用在于，当可变参数的个数为0时，
这里printf参数列表中的的##会把前面多余的","去掉，否则会编译出错，
建议使用后面这种，使得程序更加健壮。

*/
// #define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
// #define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
// #define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
// #define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#define LOG_DEBUG(format, ...)   {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...)   {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...)   {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...)  {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif