//单例
#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include <memory>

//N 是 当前类的序号
//N来区分不同对象
//这是单例模式的模板类
//之所以不在日志器里面实现单例模式 因为分布式, 多线程等 都可能会导致 每一个线程一个单例
//使用模板用N 来区分他们

template<class T, class X = void, int N = 0>
class Singleton{
public:
    static T* GetInstance(){
        static T v;
        return &v;//这里利用了c++11的 局部静态安全
    }
};

//智能指针 管理单例
template<class T, class X = void, int N = 0>
class SingletonPtr{
public:
    static std::shared_ptr<T> GetInstance(){
        static std::shared_ptr<T> v(new T);
        return v;
    }


};



#endif