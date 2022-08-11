#ifndef __FUNCTION_WRAPPER_HPP__
#define __FUNCTION_WRAPPER_HPP__


#include <memory>
//函数包装器
/*
 * 我们通过future来传递结果给其他线程
 * 但是std::packaged_task<> 的实例是不可拷贝的
 * 只能移动, 所以不能直接使用std::function<>来实现任务队列
 * 因为std::function<>需要存储可复制构造的函数对象
 * 我们要包装一个自定义函数, 用来处理只可移动的类型
 * 这就是一个带有函数操作符的类型擦除类
 * 只需要处理那些没有函数和无返回的函数，所以这是一个简单的虚函数调用。
 */
//对函数的一个封装类，可移动，不可拷贝
class function_wrapper
{
    
    struct impl_base{
        virtual void call() = 0;
        virtual ~impl_base() {};
        
    };

    std::unique_ptr<impl_base> impl;

    template<typename F>
    struct impl_type : impl_base
    {
        F f;
        impl_type(F&& f_): f(std::move(f_)){}

        void call() override
        {f();}

        
    };

public:
    template<typename F>
    function_wrapper(F&& f) : impl(new impl_type<F>(std::move(f))){}

    void operator()(){impl -> call();}

    

    function_wrapper() = default;

    function_wrapper(function_wrapper&& other) noexcept : 
            impl(std::move(other.impl)){}

    function_wrapper& operator=(function_wrapper&& other) noexcept
    {
        impl = std::move(other.impl);
        return *this;
    }
    
    function_wrapper(const function_wrapper&) = delete;
    function_wrapper(function_wrapper&) = delete;
    function_wrapper& operator=(const function_wrapper&) = delete;
};


#endif