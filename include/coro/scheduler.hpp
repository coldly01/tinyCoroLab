#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "config.h"
#ifdef ENABLE_MEMORY_ALLOC
    #include "coro/allocator/memory.hpp"
#endif
#include "coro/dispatcher.hpp"
#include "coro/detail/atomic_helper.hpp"

namespace coro
{

/**
 * @brief scheduler just control context to run and stop,
 * it also use dispatcher to decide which context can accept the task
 *
 */
class scheduler
{
    friend context;
    // 上文提到的引用计数
    using stop_token_type = std::atomic<int>;
    // 每个 context 对应的状态，只有 0 和 1 两个值，0 表示 context 已完成所有任务，1 表示 context 还在执行任务中
    using stop_flag_type =std::vector<detail::atomic_ref_wrapper<int>>;

public:
    [[CORO_TEST_USED(lab2b)]] inline static auto init(size_t ctx_cnt = std::thread::hardware_concurrency()) noexcept
        -> void
    {
        if (ctx_cnt == 0)
        {
            ctx_cnt = std::thread::hardware_concurrency();
        }
        get_instance()->init_impl(ctx_cnt);
    }

    /**
     * @brief loop work mode, auto wait all context finish job
     *
     */
    [[CORO_TEST_USED(lab2b)]] inline static auto loop() noexcept -> void { get_instance()->loop_impl(); }

    static inline auto submit(task<void>&& task) noexcept -> void
    {
        auto handle = task.handle();
        task.detach();
        submit(handle);
    }

    static inline auto submit(task<void>& task) noexcept -> void { submit(task.handle()); }

    [[CORO_TEST_USED(lab2b)]] inline static auto submit(std::coroutine_handle<> handle) noexcept -> void
    {
        get_instance()->submit_task_impl(handle);
    }

private:
    static auto get_instance() noexcept -> scheduler*
    {
        static scheduler sc;
        return &sc;
    }

    [[CORO_TEST_USED(lab2b)]] auto init_impl(size_t ctx_cnt) noexcept -> void;

    [[CORO_TEST_USED(lab2b)]] auto loop_impl() noexcept -> void;

    auto stop_impl() noexcept -> void;

    [[CORO_TEST_USED(lab2b)]] auto submit_task_impl(std::coroutine_handle<> handle) noexcept -> void;

    // TODO[lab2b]: Add more function if you need
    auto start_impl() noexcept -> void;

private:
    size_t                                              m_ctx_cnt{0};
    detail::ctx_container                               m_ctxs;
    detail::dispatcher<coro::config::kDispatchStrategy> m_dispatcher;
    // TODO[lab2b]: Add more member variables if you need

    /**
     * @brief 全局引用计数，表示当前"忙碌"的 context 数量
     *
     * - 初始值 = context 数量（所有 context 初始为忙碌）
     * - 提交任务时：如果目标 context 从空闲变忙碌，则 +1
     * - context 完成所有任务时：-1
     * - 当降为 0 时，触发 stop_impl() 通知所有 context 停止
     */
    stop_token_type m_stop_token;

    /**
     * @brief 每个 context 的状态标志数组
     *
     * m_ctx_stop_flag[i] 表示 context[i] 的状态：
     * - 1 = 忙碌（有任务在处理或待处理）
     * - 0 = 空闲（没有任务）
     *
     * 用于防止同一个 context 重复计入 m_stop_token：
     * - 提交任务时：fetch_or(1)，只有从 0→1 时才增加 m_stop_token
     * - 任务完成时：fetch_and(0)，只有从 1→0 时才减少 m_stop_token
     */
    stop_flag_type  m_ctx_stop_flag;

#ifdef ENABLE_MEMORY_ALLOC
    // Memory Allocator
    coro::allocator::memory::memory_allocator<coro::config::kMemoryAllocator> m_mem_alloc;
#endif
};

inline void submit_to_scheduler(task<void>&& task) noexcept
{
    scheduler::submit(std::move(task));
}

inline void submit_to_scheduler(task<void>& task) noexcept
{
    scheduler::submit(task.handle());
}

inline void submit_to_scheduler(std::coroutine_handle<> handle) noexcept
{
    scheduler::submit(handle);
}

}; // namespace coro
