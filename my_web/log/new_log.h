#ifndef __LOG_H__
#define __LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdarg.h>
#include <map>

#include "./singleton.h"
#include "./util.h"
#include "./mutex.h"


//流式输出
//通过宏来创建事件
//通过LogEventWrap这个临时对象 在析构时 实现日志写入 
//这里巧妙的使用了局部变量的生命周期实现了打印
//宏是在if内部构造了局部变量 当if结束后 会调用析构函数 实现打印
//如果直接使用LogEvent的话 会因为智能指针 让他在主函数存在时 不会进行析构
#define MY_LOG_LEVEL(logger, level) \
    if (logger -> getLevel() <= level) \
        LogEventWrap(LogEvent::ptr(new LogEvent(logger, level,\
                __FILE__, __LINE__, 0, GetThreadId(), \
                time(0)))).getSS()

#define MY_LOG_DEBUG(logger) MY_LOG_LEVEL(logger, LogLevel::DEBUG)
#define MY_LOG_INFO(logger) MY_LOG_LEVEL(logger, LogLevel::INFO)
#define MY_LOG_WARN(logger) MY_LOG_LEVEL(logger, LogLevel::WARN)
#define MY_LOG_ERROR(logger) MY_LOG_LEVEL(logger, LogLevel::ERROR)
#define MY_LOG_FATAL(logger) MY_LOG_LEVEL(logger, LogLevel::FATAL)

//便捷调用单例管理类的日志管理类的 m_root日志类
#define MY_LOG_ROOT()  LoggerMgr::GetInstance()->getRoot()

#define MY_LOG_NAME(name)  LoggerMgr::GetInstance()->getLogger(name)


class Logger; //前向声明
class LoggerManager;

//日志类型枚举
    class LogLevel{
    public:  
        enum Level
        {
            UNKNOW = 0,
            DEBUG = 1,
            INFO = 2,
            WARN = 3,
            ERROR = 4,
            FATAL = 5
        };


        static const char* ToString(LogLevel::Level level);
        static LogLevel::Level FromString(const std::string& str);

    };

    class LogEvent
    {
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        // LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
        //      const char* file, int32_t line, uint32_t elapse,
        //     uint32_t thread_id, uint32_t fiber_id, uint64_t time,
        //     const std::string& thread_name);

        LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                 const char* file, int32_t line, uint32_t elapse,
                 uint32_t thread_id, uint64_t time);

        const char* getFile() const {return m_file;}
        int32_t getLine() const {return m_line;}
        uint32_t getElapse() const {return m_elapse;}
        uint32_t getThreadId() const {return m_threadId;}
        //uint32_t getFiberId() const {return m_fiberId;}
        uint64_t getTime() const {return m_time;}
        //const std::string& getThreadName() const {return m_threadName;}
        const std::string getContent() const {return m_ss.str();}

        std::shared_ptr<Logger> getLogger() const {return m_logger;}
        LogLevel::Level getLevel() const {return m_level;}

        std::stringstream& getSS()  {return m_ss; }
        
        
        //可变传参
        //void format(const char* fmt, ...); 
        //void format(const char* fmt, va_list al);
    private:
        

        const char *m_file = nullptr; //文件名
        int32_t m_line = 0;           //行号
        uint32_t m_elapse = 0;        //程序启动到现在的毫秒数
        uint32_t m_threadId = 0;         //线程号
        //uint32_t m_fiberId = 0;       //协程号
        uint64_t m_time;              //时间戳
        //std::string m_threadName;   //线程名
        std::stringstream m_ss;        //内容

        std::shared_ptr<Logger> m_logger; //日志器
        LogLevel::Level m_level;
        
        
    };

    //释放LogEvent
    class LogEventWrap{
    public:
        LogEventWrap(LogEvent::ptr e);
        ~LogEventWrap();

        LogEvent::ptr getEvent() const {return m_event;}
        std::stringstream& getSS();
    private:
        LogEvent::ptr m_event;
    };


    //日志格式器
    class LogFormatter
    {
    public:
        typedef std::shared_ptr<LogFormatter> ptr;

        //解析传入的字符串
        LogFormatter(const std::string& pattern);


        //%t    %thread_id %m%n
        std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);


    public:
        //定义一个虚基类来 进行解析工作
        class FormatItem{
            public:
                typedef std::shared_ptr<FormatItem> ptr;

                virtual ~FormatItem(){}
                virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

        };

        void init();

        bool isError() const {return m_error;}

        const std::string getPattern() const {return m_pattern; }

    private:
        std::string m_pattern; //传入的字符串
        std::vector<FormatItem::ptr> m_item; //保存的是 可以加工字符串的不同类

        bool m_error = false;
    };


    //日志输出地
    class LogAppender
    {
    friend class Logger;
    public:
        typedef std::shared_ptr<LogAppender> ptr;

        typedef Spinlock MutexType;

        virtual ~LogAppender(){}

        //virtual std::string toYamlString() = 0;

        virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
        //这里的 std::shared_ptr<Logger> logger 和 Logger::ptr logger 没什么区别 因为都不会用到 
        //但是如果使用 Logger::ptr logger 的话 前向声明没有作用 需要在实际声明后才能使用
        //所以 这里使用std::shared_ptr<Logger> logger

        //定义日志的格式
        void setFormatter(LogFormatter::ptr val);
        LogFormatter::ptr getFormatter() ;


        //定义日志的输出级别 只有高于他的才能输出
        void setLevel(LogLevel::Level level) { m_level = level; }
        LogLevel::Level getlevel() const {return m_level;}
    protected:
        LogLevel::Level m_level = LogLevel::DEBUG; //日志类型

        bool m_hasFormatter = false; // 这个是 由于日志yaml输出 会因为父类设置的日志格式 而子类没有设置日志格式 导致父类的日志格式被子类输出 为了让输出好看一些 使用这个来判断子类是否有输出格式 如果没有就不输出
        
        MutexType m_mutex;
        LogFormatter::ptr m_formatter; //日志格式    
    };



    //日志器
    class Logger : public std::enable_shared_from_this<Logger>
    {
    friend class LoggerManager;
    public:
        typedef std::shared_ptr<Logger> ptr;

        typedef Spinlock MutexType;

        Logger(const std::string& name = "root");

        void log(LogLevel::Level level, LogEvent::ptr event);

        //根据日志级别输出 
        void debug(LogEvent::ptr event);
        void info(LogEvent::ptr event);
        void warn(LogEvent::ptr event);
        void error(LogEvent::ptr event);
        void fatal(LogEvent::ptr event);

        //增加 或 删除Appender
        void addAppender(LogAppender::ptr appender);
        void delAppender(LogAppender::ptr appender);
        void clearAppenders();

        //设置和返回 日志级别
        LogLevel::Level getLevel() const {return m_level;}
        void setLevel(LogLevel::Level val) { m_level = val; }

        const std::string& getName() const { return m_name;}

        void setFormatter(LogFormatter::ptr val);
        void setFormatter(const std::string& val);

        LogFormatter::ptr getFormatter();

        //std::string toYamlString();

    private:
        std::string m_name; //日志当前使用者名称
        LogLevel::Level m_level;//日志类型

        MutexType m_mutex;
        std::list<LogAppender::ptr> m_appenders; //Appender集合
        LogFormatter::ptr m_formatter;

         /// 主日志器
        Logger::ptr m_root;

    };

    //输出到控制台的Appender
    class StdoutLogAppender : public LogAppender{
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;

        void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
        //std::string toYamlString() override;
    };

    //定义输出到文件的Appender
    class FileLogAppender : public LogAppender{
        public:
            typedef std::shared_ptr<FileLogAppender> ptr;
            FileLogAppender(const std::string& filename);
            void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;

            //std::string toYamlString() override;


            //若文件已经打开  我们关闭 重新打开 文件打开成功返回true
            bool reopen();
        private:
            std::string m_filename;
            std::ofstream m_filestream;

            uint64_t m_lastTime = 0; 
    };   

    //日志管理类 方便日志的创建
    class LoggerManager{
    public:
        typedef Spinlock MutexType;

        LoggerManager();
        Logger::ptr getLogger(const std::string& name);

        void init();

        Logger::ptr getRoot() const { return m_root; }


        //将所有的日志器配置转成YAML String
        //std::string toYamlString();

    private:
        MutexType m_mutex;
        std::map<std::string, Logger::ptr> m_loggers;

         /// 主日志器
        Logger::ptr m_root;
    };


    //单例类 来管理日志管理类
    typedef Singleton<LoggerManager> LoggerMgr;


    std::string getLogFileName();

    void setFileLogAppender(); 

#endif