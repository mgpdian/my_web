#ifndef __THREAD_SAFE_QUEUE_LOCKABLE_AND_WAITING_HPP__
#define __THREAD_SAFE_QUEUE_LOCKABLE_AND_WAITING_HPP__
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

//尝试实现细颗粒队列
// thread_safe_queue_lockable_and_waiting

template <typename T>
class thread_safe_queue_lockable_and_waiting {
private:
    typedef std::mutex MutexType;

private:
    typedef struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    } Node;

    MutexType head_mutex;
    std::unique_ptr<node> head;

    mutable MutexType tail_mutex;
    node* tail = nullptr;

    std::condition_variable data_cond;

private:
    //上锁和等待的线程安全队列——wait_and_pop()

    //查看尾指针
    node* get_tail();

    //过程函数 作用退队头指针 配合wait_pop_head 完成退队
    std::unique_ptr<node> pop_head();

    //等待函数  等待队伍不为空
    std::unique_lock<MutexType> wait_for_data();

    //退队函数
    std::unique_ptr<node> wait_pop_head();

    //退队 并将退队的对象传给value
    std::unique_ptr<node> wait_pop_head(T& value);

    //可上锁和等待的线程安全队列——try_pop()和empty()

    //尝试退队 如果失败 不会等待直接返回空值 如果成功则调用过程函数pop_head
    std::unique_ptr<node> try_pop_head();

    //尝试退队 如果失败 不会等待直接返回空值 如果成功则调用过程函数pop_head
    //并给value赋值
    std::unique_ptr<node> try_pop_head(T& value);

public:
    thread_safe_queue_lockable_and_waiting()
        : head(new node), tail(head.get()) {}

    thread_safe_queue_lockable_and_waiting(
        const thread_safe_queue_lockable_and_waiting<T>& other) = delete;

    thread_safe_queue_lockable_and_waiting& operator=(
        const thread_safe_queue_lockable_and_waiting<T>& other) = delete;

    //可上锁和等待的线程安全队列——推入新节点
    void push(T new_value);

    //可上锁和等待的线程安全队列——wait_and_pop()
    //通过共享指针传递退队对象
    std::shared_ptr<T> wait_and_pop();
    //通过value引用实现退队
    void wait_and_pop(T& value);

    //可上锁和等待的线程安全队列——try_pop()和empty()

    //通过共享指针传递退队对象
    std::shared_ptr<T> try_pop();

    //通过value引用实现退队
    bool try_pop(T& value);

    void empty();
};

//可上锁和等待的线程安全队列——推入新节点
template <typename T>
void thread_safe_queue_lockable_and_waiting<T>::push(T new_value) {
    std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));

    std::unique_ptr<node> new_node(new node);

    node* const new_tail = new_node.get();

    {
        std::lock_guard<MutexType> tail_lock(tail_mutex);
        tail->data = new_data;

        tail->next = std::move(new_node);
        tail = new_tail;
    }

    data_cond.notify_one();
}

//可上锁和等待的线程安全队列——wait_and_pop()
template <typename T>
typename thread_safe_queue_lockable_and_waiting<T>::node*
thread_safe_queue_lockable_and_waiting<T>::get_tail() {
    std::lock_guard<MutexType> tail_lock(tail_mutex);
    return tail;
}

//过程函数 作用退队头指针 配合wait_pop_head 完成退队
template <typename T>
std::unique_ptr<typename thread_safe_queue_lockable_and_waiting<T>::node>
thread_safe_queue_lockable_and_waiting<T>::pop_head() {
    std::unique_ptr<node> old_head = std::move(head);
    head = std::move(old_head->next);
    return old_head;
}

//等待函数  等待队伍不为空
template <typename T>
std::unique_lock<std::mutex>
thread_safe_queue_lockable_and_waiting<T>::wait_for_data() {
    std::unique_lock<MutexType> head_lock(head_mutex);
    data_cond.wait(head_lock, [&] { return head.get() != get_tail(); });

    return std::move(head_lock);
}

//退队函数
template <typename T>
std::unique_ptr<typename thread_safe_queue_lockable_and_waiting<T>::node>
thread_safe_queue_lockable_and_waiting<T>::wait_pop_head() {
    std::unique_lock<MutexType> head_lock(wait_for_data());
    // 传递锁

    return pop_head();
}

//退队 并将退队的对象传给value
template <typename T>
std::unique_ptr<typename thread_safe_queue_lockable_and_waiting<T>::node>
thread_safe_queue_lockable_and_waiting<T>::wait_pop_head(T& value) {
    std::unique_lock<MutexType> head_lock(wait_for_data());  // 传递锁
    value = std::move(*head->data);
    return pop_head();
}

//可上锁和等待的线程安全队列——try_pop()和empty()

//尝试退队 如果失败 不会等待直接返回空值 如果成功则调用过程函数pop_head
template <typename T>
std::unique_ptr<typename thread_safe_queue_lockable_and_waiting<T>::node>
thread_safe_queue_lockable_and_waiting<T>::try_pop_head() {
    std::lock_guard<MutexType> head_lock(head_mutex);

    if (head.get() == get_tail()) {
        return nullptr;
    }
    return pop_head();
}

//尝试退队 如果失败 不会等待直接返回空值 如果成功则调用过程函数pop_head
//并给value赋值
template <typename T>
std::unique_ptr<typename thread_safe_queue_lockable_and_waiting<T>::node>
thread_safe_queue_lockable_and_waiting<T>::try_pop_head(T& value) {
    std::lock_guard<MutexType> head_lock(head_mutex);

    if (head.get() == get_tail()) {
        return nullptr;
    }

    value = std::move(*head->data);

    return pop_head();
}

//可上锁和等待的线程安全队列——wait_and_pop()
//通过共享指针传递退队对象
template <typename T>
std::shared_ptr<T> thread_safe_queue_lockable_and_waiting<T>::wait_and_pop() {
    std::unique_ptr<node> const old_head = wait_pop_head();
    return old_head->data;
}
//通过value引用实现退队
template <typename T>
void thread_safe_queue_lockable_and_waiting<T>::wait_and_pop(T& value) {
    //std::unique_ptr<node> const old_head =
        wait_pop_head(value);  //通过智能指针来实现自动释放
}

//可上锁和等待的线程安全队列——try_pop()和empty()

//通过共享指针传递退队对象
template <typename T>
std::shared_ptr<T> thread_safe_queue_lockable_and_waiting<T>::try_pop() {
    std::unique_ptr<node> old_head = try_pop_head();
    return old_head ? old_head->data : nullptr;
}

//通过value引用实现退队
template <typename T>
bool thread_safe_queue_lockable_and_waiting<T>::try_pop(T& value) {
    std::unique_ptr<node> const old_head = try_pop_head(value);
    return old_head != nullptr;
}

template <typename T>
void thread_safe_queue_lockable_and_waiting<T>::empty() {
    std::lock_guard<MutexType> head_lock(head_mutex);
    return (head.get() == get_tail());
}
#endif