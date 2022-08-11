//util 通用包
#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <cxxabi.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>



//返回当前线程的id
//为什么不用c++11的 std::this_thread::get_id() ?
//原因 使用std::this_thread::get_id() 会得到 140441394579264
//同时类型是std::thread::id  而且不能强制转换成uint32_t 需要你将其转换为指针 再转换成uint32_t
//但这样做 依旧有很大的误差 258967360 
//不如使用自带的
pid_t GetThreadId();

#endif