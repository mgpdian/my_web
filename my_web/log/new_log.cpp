#include "new_log.h"

#include <time.h>

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <tuple>

const char* LogLevel::ToString(LogLevel::Level level) {
    switch (level) {
        //纯纯的懒狗
#define XX(name)         \
    case LogLevel::name: \
        return #name;    \
        break;

        XX(DEBUG);
        XX(INFO);
        XX(WARN);
        XX(ERROR);
        XX(FATAL);

#undef XX

        default:
            return "UNKNOW";
    }

    return "UNKNOW";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, name)         \
    if (str == #name) {         \
        return LogLevel::level; \
    }

    XX(DEBUG, debug);
    XX(INFO, info);
    XX(WARN, warn);
    XX(ERROR, error);
    XX(FATAL, fatal);

    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);

    return LogLevel::UNKNOW;

#undef XX
}

//定义各种输出地
//输出日志内容
class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }
};

//输出日志类型级别
class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }
};

//输出程序启动到现在的毫秒数
class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

//输出日志当前使用者名称
class NameFormatItem : public LogFormatter::FormatItem {
public:
    NameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLogger()->getName();
    }
};

//输出日志线程ID
class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

//输出时间
class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
        : m_format(format) {
        if (m_format.empty()) {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }

private:
    std::string m_format;
};

//文件名
class FilenameFormatItem : public LogFormatter::FormatItem {
public:
    FilenameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFile();
    }
};
//行号
class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLine();
    }
};

//换行
class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << std::endl;
    }
};

//作用是 输出字符串
class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string& str) : m_string(str) {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }

private:
    std::string m_string;
};

// tab号
class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event) override {
        os << "\t";
    }
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   const char* file, int32_t line, uint32_t elapse,
                   uint32_t thread_id, uint64_t time)
    : m_file(file),
      m_line(line),
      m_elapse(elapse),
      m_threadId(thread_id),
      m_time(time),
      m_logger(logger),
      m_level(level) {}


LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e){}

LogEventWrap::~LogEventWrap(){
    m_event->getLogger()->log(m_event->getLevel(), m_event);
    //在结束的时候 触发事件 日志写入
}

std::stringstream& LogEventWrap::getSS() {
    // std::cout << "dada";
    return m_event->getSS();
}



void LogAppender::setFormatter(LogFormatter::ptr val){
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    if(m_formatter){
        m_hasFormatter = true;
    }
    else{
        m_hasFormatter = false;
    }
}

LogFormatter::ptr LogAppender::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}


Logger::Logger(const std::string& name)
    : m_name(name), m_level(LogLevel::DEBUG)
{
    m_formatter.reset(new LogFormatter(
        "%d{%Y-%m-%d %H:%M:%S}%T%t%T[%p]%T[%c]%T%f:%l%T%T%m%n"
    ));
    //设置个人的加工器  如果指定的输出地没有自己的加工器 那就使用自己的
}


//增加 或 删除Appender
void Logger::addAppender(LogAppender::ptr appender){
    MutexType::Lock lock(m_mutex);

    //判断输出地有没有加工器
    if(!appender->getFormatter()){
        MutexType::Lock ll(appender->m_mutex);

        appender->m_formatter = m_formatter;
        //这里需要解释一下 为什么要使用友元类直接使用m_formatter
        //而不使用setFormatter 前提: 在子类没有设置日志格式的情况下,
        //我们想让父类的日志格式不在子类那里输出 因为这样不好看
        //我们在appender类那里定义了一个bool 来判断子类是否有自己的日志格式
        //在setFormatter那里设置bool 原来的本质实际上只要赋值就能让bool变成true
        //为了避免父类给子类的日志格式 影响到bool
        //我们使用友元类 直接对appender类进行赋值
    }

    m_appenders.emplace_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender){
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin(); it != m_appenders.end(); ++it){
        if(*it = appender){
            m_appenders.erase(it);
            break;
        }
    }
}


void Logger::clearAppenders(){
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}

void Logger::setFormatter(LogFormatter::ptr val) {
    MutexType::Lock lock(m_mutex);
    m_formatter = val;

    //防止有人直接通过父类的setFormatter修改m_formatter 注意
    //该m_formatter修改只会影响到没有自己的m_formatter的子类
    // 导致不按修改的格式输出的问题

    for (auto& i : m_appenders) {
        MutexType::Lock ll(i->m_mutex);
        if (!i->m_hasFormatter) {
            i->m_formatter = m_formatter;
        }
    }
}

void Logger::setFormatter(const std::string& val) {
    LogFormatter::ptr new_val(new LogFormatter(val));
    if (new_val->isError()) {
        std::cout << "Logger setFormatter name=" << m_name << " value=" << val
                  << " invalide formatter" << std::endl;
        return;
    }
    // m_formatter.reset(new sylar::LogFormatter(val));
    // m_formatter = new_val;
    setFormatter(new_val);
}

LogFormatter::ptr Logger::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}


void Logger::log(LogLevel::Level level, LogEvent::ptr event)
{
    //检查是否 当前的日志器级别能否输出要求的日志类型
    if(level >= m_level){
        auto self = shared_from_this();
        MutexType::Lock lock(m_mutex);
        if (!m_appenders.empty()) {
            for (auto& i : m_appenders) {
                i->log(self, level,
                       event);  //发送给 日志输出类
                                //在日志输出类那里来检查该类是否能输出
            }
        } else if (m_root) {
            m_root->log(level, event);
        }
    
    }
}

//根据日志级别输出
void Logger::debug(LogEvent::ptr event) { log(LogLevel::DEBUG, event); }
void Logger::info(LogEvent::ptr event) { log(LogLevel::INFO, event); }
void Logger::warn(LogEvent::ptr event) { log(LogLevel::WARN, event); }
void Logger::error(LogEvent::ptr event) { log(LogLevel::ERROR, event); }
void Logger::fatal(LogEvent::ptr event) { log(LogLevel::FATAL, event); }


/// StdoutAppender
void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level,
                            LogEvent::ptr event){
        if (level >= m_level) {
            MutexType::Lock lock(m_mutex);

            std::string str = m_formatter->format(logger, level, event);
            std::cout << str;

        }
}



// FileLogAppender
FileLogAppender::FileLogAppender(const std::string& filename)
    : m_filename(filename) {
    reopen();  //打开文件
}

void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level,
                          LogEvent::ptr event) 
{
    if (level >= m_level) {
        uint64_t now = time(0);
        if(now != m_lastTime) //防止写到的时候 写的文件被删除
        {
            reopen();
            m_lastTime = now;
        }
        MutexType::Lock lock(m_mutex);

        if(!(m_filestream << m_formatter->format(logger, level, event)))
        {
            std::cout << "error" << std::endl;
        }
    }
}

bool FileLogAppender::reopen(){
    MutexType::Lock lock(m_mutex);
    if (m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename, std::ios::app);

    return !!m_filestream;  //双重!! 让文件从字符串变成真正的bool
                            //而不需要程序自己去判断
}


//解析传入的字符串?
LogFormatter::LogFormatter(const std::string& pattern) : m_pattern(pattern) {
    init();  //解析传入的m_pattern
}

//%t    %thread_id %m%n
std::string LogFormatter::format(std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, LogEvent::ptr event) {
        std::stringstream ss;
        for (auto& i : m_item) {
            i->format(ss, logger, level, event);  //每个都来一下吗  ???
        }
        return ss.str();  //从字节流转换为字符串
}

//初始化
//三种情况需要特殊处理 %xxx %xxx{xxx} %%(转义%)
void LogFormatter::init() {
    // str , format , type //
    std::vector<std::tuple<std::string, std::string, int> > vec;
    std::string nstr;
    for (size_t i = 0; i < m_pattern.size(); ++i) {
        if (m_pattern[i] != '%')  //若不是百分号 就直接放入字符串中
        {
            nstr.append(1, m_pattern[i]);
            //++i; //这里应该加1 吧?
            continue;
        }

        if ((i + 1) < m_pattern.size()) {
            if (m_pattern[i + 1] == '%') {
                nstr.append(1, '%');

                continue;
            }
        }

        size_t n = i + 1;
        int fmt_status = 0;  //判断是否要结束 比如 {{ }} 如果不标记的话
                             //可能遇到第一个}就会退出
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;

        while (n < m_pattern.size()) {
            if (!fmt_status && (!isalpha(m_pattern[n])) &&
                m_pattern[n] != '{' &&
                m_pattern[n] != '}')  //如果遇到空格 表示%特殊字符结束 break;
                                      //isalpha 为判断是否为字母
            {
                str = m_pattern.substr(
                    i + 1,
                    n - i - 1);  //这里是考虑 没有括号的情况 中的数字 %112  ?
                break;
            }

            if (fmt_status == 0)  //现在还没有 遇到{}
            {
                if (m_pattern[n] == '{')
                //这是遇到的第一个{  我们先将{}前的字符串加入到str1中
                {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1;  //解析格式
                    fmt_begin = n;   //记录
                    ++n;

                    continue;
                }
            }

            if (fmt_status == 1)  //遇到了一个 {
            {
                if (m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 0;  //之前为2 现在修改为2 可以重复使用
                    ++n;
                    break;
                }
            }
            ++n;

            if (n == m_pattern.size()) {
                if (str.empty()) {
                    str = m_pattern.substr(i + 1);  //拿到剩下为%xxx的格式
                }
            }
        }

        if (fmt_status == 0)  //没有遇到{} 的情况下完成了%xxx
        {
            if (!nstr.empty()) {
                vec.push_back(std::make_tuple(nstr, std::string(),
                                              0));  //输出前面的正常字符串
                nstr.clear();
            }
            // str = m_pattern.substr(i + 1, n - i - 1); 在之前已经截取了
            vec.push_back(std::make_tuple(str, fmt, 1));  //%xxx
            i = n - 1;

            //这样的话 之前 fmt_status == 2 的代码 也会在这里被使用
            //减低了代码重复度
        } else if (fmt_status == 1)  //只有{ 没有} 报错
        {
            std::cout << "pattern parse error " << m_pattern << " - "
                      << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.push_back(
                std::make_tuple("<<pattern_error>>", fmt, 0));  //报错信息
            // i = n;
        }
    }

    //避免字符串结束时 没有把nstr内容输出出来
    if (!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, "", 0));
    }

    static std::map<std::string,
                    std::function<FormatItem::ptr(const std::string& str)> >
        s_format_items = {
#define XX(str, C)                                                             \
    {                                                                          \
#str,                                                                  \
            [](const std::string& fmt) { return FormatItem::ptr(new C(fmt)); } \
    }

            XX(m, MessageFormatItem),     // m:消息
            XX(p, LevelFormatItem),       // p:日志级别
            XX(r, ElapseFormatItem),      // r:累计毫秒数
            XX(c, NameFormatItem),        // c:日志名称
            XX(t, ThreadIdFormatItem),    // t:线程id
            XX(n, NewLineFormatItem),     // n:换行
            XX(d, DateTimeFormatItem),    // d:时间
            XX(f, FilenameFormatItem),    // f:文件名
            XX(l, LineFormatItem),        // l:行号
            XX(T, TabFormatItem),         // T: tab缩进符
            
#undef XX
        };

    for (auto& i : vec) {
        if (std::get<2>(i) == 0) {
            m_item.push_back(
                FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        } else {
            auto it = s_format_items.find(std::get<0>(i));

            if (it == s_format_items.end()) {
                m_item.push_back(FormatItem::ptr(new StringFormatItem(
                    "<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            } else {
                m_item.push_back(it->second(std::get<1>(i)));
            }
        }

        // std::cout <<  "{" << std::get<0>(i) << "} - {" << std::get<1>(i) <<
        // "} - {" << std::get<2>(i) << "}" << std::endl;
    }

    // std::cout << m_item.size() << std::endl;
}

LoggerManager::LoggerManager(){
    m_root.reset(new Logger);

    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));

    m_loggers[m_root->m_name] = m_root;

    init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()){
        return it->second;
    }

    Logger::ptr logger(new Logger(name)); // LoggerManager 是 logger的友元类
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}

struct LogAppenderDefine {
    int type = 0;  // 1 FIle, 2 Stdout
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine& oth) const {
        return type == oth.type && level == oth.level &&
               formatter == oth.formatter && file == oth.file;
    }
};


void LoggerManager::init() {}


std::string getLogFileName(){
        std::string format = "%Y-%m-%d";
        struct tm tm;

        time_t time_tmp = time(0);
        localtime_r(&time_tmp, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), format.c_str(), &tm);

        return buf;
    }

    void setFileLogAppender(){
        Logger::ptr g_logger = MY_LOG_ROOT();
        std::string LogFileName = getLogFileName();
        LogFileName = "./" + LogFileName + "_log.txt";
        FileLogAppender::ptr file_appender(new FileLogAppender(LogFileName));
        file_appender -> setLevel(LogLevel::INFO);
    
        g_logger->addAppender(file_appender);

    }
