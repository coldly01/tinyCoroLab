/**
 * @file mutex.hpp
 * @author JiahuiWang
 * @brief lab4d
 * @version 1.1
 * @date 2025-03-24
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <atomic>
#include <cassert>
#include <coroutine>
#include <type_traits>

#include "coro/comp/mutex_guard.hpp"
#include "coro/detail/types.hpp"

namespace coro
{
/**
 * @brief Welcome to tinycoro lab4d, in this part you will build the basic coroutine
 * synchronization component----mutex by modifing mutex.hpp and mutex.cpp.
 * Please ensure you have read the document of lab4d.
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
using detail::awaiter_ptr;
// TODO[lab4d]: This mutex is an example to make complie success,
// You should delete it and add your implementation, I don't care what you do,
// but keep the member function and construct function's declaration same with example.
class mutex
{
public:    
    struct mutex_awaiter
    {
        mutex_awaiter(context& ctx, mutex& mtx) noexcept : m_ctx(ctx), m_mtx(mtx) {}

        constexpr auto await_ready() noexcept -> bool { return false; }

        auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool;

        auto await_resume() noexcept -> void;

        // awaiter 将自身注册到 mutex 的 awaiter 链表中，注册成功即陷入 suspend 状态
        auto register_lock() noexcept -> bool;

        // 对于恢复协程过程的封装，声明为虚函数是因为后续需要
        virtual auto resume() noexcept -> void;

        context&                m_ctx; // 绑定的 context
        mutex&                  m_mtx; // 绑定的 mutex
        mutex_awaiter*          m_next{nullptr}; // mutex 协程链表的 next 指针
        std::coroutine_handle<> m_await_coro{nullptr}; // 待 resume 的协程
    };
    // 用于 mutex.lock_guard 的返回值，其逻辑与 mutex_awaiter 相同因此继承了 mutex_awaiter，
    // 但重写了 await_resume 用于返回 lock_guard
    struct mutex_guard_awaiter : public mutex_awaiter
    {
        using guard_type = detail::lock_guard<mutex>;
        using mutex_awaiter::mutex_awaiter;

        auto await_resume() noexcept -> guard_type
        {
            mutex_awaiter::await_resume(); // 保留原有的逻辑
            return guard_type(m_mtx);
        }
    };

public:
    mutex() noexcept : m_state(nolocked), m_resume_list_head(nullptr) {}
    ~mutex() noexcept { assert(m_state.load(std::memory_order_acquire) == mutex::nolocked); }

    auto try_lock() noexcept -> bool;

    auto lock() noexcept -> mutex_awaiter;

    auto unlock() noexcept -> void ;

    auto lock_guard() noexcept -> mutex_guard_awaiter;

private:
    friend mutex_awaiter;

    // 常量 1 表示当前锁处于未加锁状态，常量 0 表示锁处于加锁但没有协程在等待获取锁
    // 注意 0 和 1 不是随便设置的，原因稍后讲解
    inline static awaiter_ptr nolocked          = reinterpret_cast<awaiter_ptr>(1);
    inline static awaiter_ptr locked_no_waiting = 0; // nullptr
    // 与 event 类似，同样也是挂载 suspend awaiter 的链表头
    //m_state 是"状态 + 链表头"二合一：值为 0/1 表示状态，其他值就是链表头指针。
    std::atomic<awaiter_ptr>  m_state; // 收集新等待者
    awaiter_ptr               m_resume_list_head; // 用于存储待恢复的 suspend awaiter(已排序), 仅持锁者访问，无需原子操作
};

}; // namespace coro
