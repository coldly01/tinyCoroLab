/**
 * @file event.hpp
 * @author JiahuiWang
 * @brief lab4a
 * @version 1.1
 * @date 2025-03-24
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <atomic>
#include <coroutine>

#include "coro/attribute.hpp"
#include "coro/concepts/awaitable.hpp"
#include "coro/context.hpp"
#include "coro/detail/container.hpp"
#include "coro/detail/types.hpp"

namespace coro
{
/**
 * @brief Welcome to tinycoro lab4a, in this part you will build the basic coroutine
 * synchronization component - event by modifing event.hpp and event.cpp. Please ensure
 * you have read the document of lab4a.
 *
 * @warning You should carefully consider whether each implementation should be thread-safe.
 *
 * You should follow the rules below in this part:
 *
 * @note The location marked by todo is where you must add code, but you can also add code anywhere
 * you want, such as function and class definitions, even member variables.
 *
 * @note lab4 and lab5 are free designed lab, leave the interfaces that the test case will use,
 * and then, enjoy yourself!
 */
class context;

namespace detail
{
// TODO[lab4a]: Add code that you don't want to use externally in namespace detail
class event_base{
public:
    struct awaiter_base{
        awaiter_base(context& ctx,event_base& ev):m_ctx(ctx),m_ev(ev){}
        inline auto next() noexcept ->awaiter_base* {return m_next;}
        auto await_ready() noexcept -> bool;
        auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool;
        auto await_resume() noexcept ->void;

        context& m_ctx;// 绑定的 context
        event_base& m_ev;// 绑定的 event
        awaiter_base* m_next{nullptr};// 链表的 next 指针
        std::coroutine_handle<> m_await_coro{nullptr};// 待 resume 的协程句柄
    };
    event_base(bool initial_set=false) noexcept : m_state((initial_set)?this:nullptr) {}
    ~event_base() noexcept = default;

    event_base(const event_base&) = delete;
    event_base(event_base&&) = delete;
    event_base& operator=(const event_base&) =delete;
    event_base& operator=(event_base&&) =delete;
    // 判断 event 是否被 set，根据 m_state 是否等于 this
    inline auto is_set() const noexcept ->bool{return m_state.load(std::memory_order_acquire)==this;}
    auto set_state() noexcept -> void;
    auto resume_all_awaiters(awaiter_ptr waiter) noexcept -> void;// 唤醒所有 suspend awaiter
    auto register_awaiters(awaiter_base* waiter) noexcept -> bool;// 挂载 suspend awaiter


private:
    //event 挂载 awaiter 的形式是链表，event_base 中的m_state表示链表头
    std::atomic<awaiter_ptr> m_state{nullptr};//用来挂载 suspend awaiter:当m_state等于 event 的 this 指针时表示 event 此时已被 set。
};
}; // namespace detail

// TODO[lab4a]: This event is an example to make complie success,
// You should delete it and add your implementation, I don't care what you do,
// but keep the function set() and wait()'s declaration same with example.
/**
 * @brief 带返回值的 event，可以在协程间传递数据
 * 
 * 使用流程：
 * ```cpp
 * event<int> ev;
 * 
 * // 消费者协程
 * int val = co_await ev.wait();  // 挂起等待，恢复后获得 42
 * 
 * // 生产者协程
 * ev.set(42);  // 存入值并唤醒所有等待者
 * ```
 * 
 * co_await ev.wait() 的执行流程：
 * 1. wait() 返回 awaiter 对象
 * 2. co_await 依次调用 awaiter 的三个方法：
 *    - await_ready()   → 检查 event 是否已 set
 *    - await_suspend() → 若未 set，挂起协程并挂载到链表
 *    - await_resume()  → 恢复时调用，返回 result() 即 set 存入的值
 * 3. co_await 表达式的值 = await_resume() 的返回值
 */
template<typename return_type = void>
class event : public detail::event_base, public detail::container<return_type>
{
public:
    using event_base::event_base;
    /**
     * @brief awaiter 等待器，供 co_await 使用
     */
    struct [[CORO_AWAIT_HINT]] awaiter : public detail::event_base::awaiter_base
    {
        using awaiter_base::awaiter_base;  // 继承基类的所有构造函数
        /**
         * @brief 协程恢复时调用，返回 set() 存入的值
         * @return co_await 表达式的值
         */
        auto await_resume() noexcept -> decltype(auto)
        {
            detail::event_base::awaiter_base::await_resume();  // 调用基类，执行 unregister_wait()
            return static_cast<event&>(m_ev).result();  // 从 container 取出值并返回
        }
    };
    /**
     * @brief 返回 awaiter 供 co_await 使用
     */
    auto wait() noexcept -> awaiter { return awaiter(local_context(), *this); }
    //local_context() 返回当前线程的 context，确保协程恢复时回到原来的线程执行

    /**
     * @brief 存入值并唤醒所有等待的协程
     * @param value 要传递给等待者的值
     */
    template<typename value_type>
    auto set(value_type&& value) noexcept -> void
    {
        this->return_value(std::forward<value_type>(value));  // 存入 container
        set_state();  // 设置状态并唤醒所有等待者
    }
};

template<>
class event<void> : public detail::event_base
{
public:
    using event_base::event_base;
    struct [[CORO_AWAIT_HINT]] awaiter : public detail::event_base::awaiter_base
    {
        using awaiter_base::awaiter_base;
    };
    auto wait() noexcept -> awaiter { return awaiter(local_context(), *this); } // return awaitable
    auto set() noexcept -> void {set_state(); }
};

/**
 * @brief RAII for event
 *
 */
class event_guard
{
    using guard_type = event<>;

public:
    event_guard(guard_type& ev) noexcept : m_ev(ev) {}
    ~event_guard() noexcept { m_ev.set(); }

private:
    guard_type& m_ev;
};

}; // namespace coro
