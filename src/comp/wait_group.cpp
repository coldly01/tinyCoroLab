#include "coro/comp/wait_group.hpp"
#include "coro/scheduler.hpp"

namespace coro
{
// TODO[lab4c] : Add codes if you need
auto wait_group::awaiter::await_suspend(std::coroutine_handle<> handle) noexcept -> bool
{
    m_await_coro = handle;
    m_ctx.register_wait();
    while (true)
    {
        if (m_wg.m_count.load(std::memory_order_acquire) == 0)
        {
            return false;
        }
        auto head = m_wg.m_state.load(std::memory_order_acquire);
        m_next    = static_cast<awaiter*>(head);
        if (m_wg.m_state.compare_exchange_weak(
                head, static_cast<awaiter_ptr>(this), std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            // m_ctx.register_wait(m_register_state);
            // m_register_state = false;
            return true;
        }
    }


}
/**
 * @brief 协程恢复后自动调用，执行清理工作
 * 
 * 与 resume() 的区别：
 * - resume()：协程恢复前，手动调用，把协程提交到 context
 * - await_resume()：协程恢复后，co_await 机制自动调用，执行收尾
 * 
 * 执行流程：done() → resume() → context.submit_task() → handle.resume() → await_resume()
 */
auto wait_group::awaiter::await_resume() noexcept -> void
{
    m_ctx.unregister_wait();  // 减少引用计数
}

/**
 * @brief 把挂起的协程重新提交到 context 执行（唤醒协程）
 * 
 * 由 done() 在计数归零时调用，遍历链表唤醒所有等待者
 * m_ctx 是协程原来所在的 context（线程），确保协程恢复时回到原线程
 */
auto wait_group::awaiter::resume() noexcept -> void
{
    m_ctx.submit_task(m_await_coro);
}

auto wait_group::add(int count) noexcept -> void
{
    m_count.fetch_add(count, std::memory_order_acq_rel);
}

auto wait_group::done() noexcept -> void
{
    if (m_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        auto head = static_cast<awaiter*>(m_state.exchange(nullptr, std::memory_order_acq_rel));
        while (head != nullptr)
        {   
            auto next=head->m_next;
            head->resume();
            head = next;
        }
    }
}

auto wait_group::wait() noexcept -> awaiter
{
    return awaiter{local_context(), *this};
}


}; // namespace coro
