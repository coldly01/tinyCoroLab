/**
 * @file condition_variable.hpp
 * @author JiahuiWang
 * @brief lab5b
 * @version 1.1
 * @date 2025-03-24
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <functional>

#include "coro/attribute.hpp"
#include "coro/comp/mutex.hpp"
#include "coro/spinlock.hpp"

namespace coro
{
/**
 * @brief Welcome to tinycoro lab5b, in this part you will build the basic coroutine
 * synchronization component����condition_variable by modifing condition_variable.hpp
 * and condition_variable.cpp. Please ensure you have read the document of lab5b.
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

using cond_type = std::function<bool()>;// 条件谓词的类型，入参为空且返回值为 bool

class condition_variable;
using cond_var = condition_variable;

// TODO[lab5b]: This condition_variable is an example to make complie success,
// You should delete it and add your implementation, I don't care what you do,
// but keep the member function and construct function's declaration same with example.
class condition_variable final
{
public:
    // 注意这里是对 mutex_awaiter 进行了复用，减少冗余代码
    struct cv_awaiter: public mutex::mutex_awaiter{
        friend condition_variable;

        // 不支持条件谓词的构造函数
        cv_awaiter(context& ctx, mutex& mtx, cond_var& cv) noexcept
            : mutex_awaiter(ctx, mtx), // 委托构造
              m_cv(cv),
              m_suspend_state(false)
        {
        }

        // 支持条件谓词的构造函数
        cv_awaiter(context& ctx, mutex& mtx, cond_var& cv, cond_type& cond) noexcept
            : mutex_awaiter(ctx, mtx), // 委托构造
              m_cv(cv),
              m_cond(cond),
              m_suspend_state(false)
        {
        }

        auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool;

        auto await_resume() noexcept -> void;

    protected:
        // 实现了 cv.wait 的核心逻辑，这个命名可能有些误导，后续会修复
        auto register_lock() noexcept -> bool;

        // 将自身挂载到 cv 的 suspend awaiter 链表中
        auto register_cv() noexcept -> void;

        // 唤醒 awaiter
        auto wake_up() noexcept -> void;

        // 尝试恢复 awaiter 运行，该函数会被 wake_up 调用
        // 注意这是个虚函数
        auto resume() noexcept -> void override;

        cond_type m_cond; // 条件谓词
        cond_var& m_cv; // 该 awaiter 所依赖的 condition_variable
        //m_suspend_state 是一个布尔标志，用于追踪当前 awaiter 是否已经调用过 register_wait()。
        // 用来确保 context 的引用计数正常
        bool      m_suspend_state; 
        // 这里还需要很多额外的成员变量比如 awaiter 依赖的 context 和 mutex，
        // 但通过继承 mutex_awaiter 就不需要再额外定义
    };
public:
    condition_variable() noexcept  = default;
    ~condition_variable() noexcept;

    CORO_NO_COPY_MOVE(condition_variable);

    auto wait(mutex& mtx) noexcept -> cv_awaiter;

    auto wait(mutex& mtx, cond_type&& cond) noexcept -> cv_awaiter;

    auto wait(mutex& mtx, cond_type& cond) noexcept -> cv_awaiter;

    auto notify_one() noexcept -> void ;

    auto notify_all() noexcept -> void ;
private:
    detail::spinlock m_lock;// 优化后的自旋锁，也可改用 std::mutex
    alignas(config::kCacheLineSize) cv_awaiter* m_head{nullptr};// suspend awaiter 链表头指针
    alignas(config::kCacheLineSize) cv_awaiter* m_tail{nullptr};// suspend awaiter 链表尾指针
};

}; // namespace coro
