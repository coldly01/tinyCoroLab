/**
 * @file wait_group.hpp
 * @author JiahuiWang
 * @brief lab4c
 * @version 1.1
 * @date 2025-03-24
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <atomic>
#include <coroutine>

#include "coro/detail/types.hpp"

namespace coro
{
/**
 * @brief Welcome to tinycoro lab4c, in this part you will build the basic coroutine
 * synchronization component��wait_group by modifing wait_group.hpp and wait_group.cpp.
 * Please ensure you have read the document of lab4c.
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

// TODO[lab4c]: This wait_group is an example to make complie success,
// You should delete it and add your implementation, I don't care what you do,
// but keep the member function and construct function's declaration same with example.
class wait_group
{
public:
    struct awaiter{
        awaiter(context& ctx, wait_group& wg) noexcept : m_ctx(ctx), m_wg(wg) {}
        constexpr auto await_ready() noexcept -> bool { return false; }
        auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool;
        auto await_resume() noexcept -> void;
        auto resume() noexcept -> void;


        context&                m_ctx;
        wait_group&             m_wg;
        awaiter*                m_next{nullptr};
        std::coroutine_handle<> m_await_coro{nullptr};

    };
    explicit wait_group(int count = 0) noexcept: m_count(count) {}

    auto add(int count) noexcept -> void;

    auto done() noexcept -> void;

    auto wait() noexcept -> awaiter;
private:
    friend awaiter;
    std::atomic<int32_t>     m_count;
    std::atomic<awaiter_ptr> m_state;
};

}; // namespace coro
