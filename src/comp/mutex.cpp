#include "coro/comp/mutex.hpp"
#include "coro/scheduler.hpp"

namespace coro
{
// TODO[lab4d] : Add codes if you need
auto mutex::mutex_awaiter::await_suspend(std::coroutine_handle<> handle) noexcept -> bool
{
    m_await_coro = handle;
    m_ctx.register_wait();
    // 挂载自身到 mutex 的 awaiter 链表，挂载成功与否会决定协程是否陷入 suspend 状态
    //false	锁获取成功	不挂起，继续执行
    //true	锁获取失败	挂起，等待恢复
    return register_lock();
}


auto mutex::mutex_awaiter::await_resume() noexcept -> void
{
    m_ctx.unregister_wait();
}
//阻塞，失败就加入等待队列挂起协程
auto mutex::mutex_awaiter::register_lock() noexcept -> bool{
    while(true){
        auto state = m_mtx.m_state.load(std::memory_order_acquire);//如果底下的操作失败的话，在进入新循环时重新获取表头
        m_next     = nullptr;
        if (state == mutex::nolocked){//mutex锁空闲
            if (m_mtx.m_state.compare_exchange_weak(
                    state, mutex::locked_no_waiting, std::memory_order_acq_rel, std::memory_order_relaxed))
            {   // 原子地尝试把 m_state 从 1（空闲）改为 0（已锁），成功则获得锁，失败则重试。  
                // 成功时的内存序 std::memory_order_acq_rel
                 // 失败时的内存序 std::memory_order_relaxed 
                return false;
            }
        }
        else{/* 此时 mutex 已经被别的协程加锁，
             state 要么等于指向 awaiter 的指针，要么是 locked_no_waiting
             如果 state 是 locked_no_waiting，那么经过下述的操作当前的
             awaiter 的 next 指针则被置为 locked_no_waiting，而 locked_no_waiting
             值为 0，即 nullptr，这样正好方便了链表遍历*/
            m_next = reinterpret_cast<mutex_awaiter*>(state);
            if (m_mtx.m_state.compare_exchange_weak(
                    state, reinterpret_cast<awaiter_ptr>(this), std::memory_order_acq_rel, std::memory_order_relaxed))
            {
                // m_ctx.register_wait(m_register_state);
                // m_register_state = false;
                return true;
            }
        }
    }
}

auto mutex::mutex_awaiter::resume() noexcept -> void
{
    m_ctx.submit_task(m_await_coro);
    // m_ctx.unregister_wait();
    // m_register_state = true;
}
//非阻塞，尝试一次，失败立即返回 false
auto mutex::try_lock() noexcept -> bool
{
    auto target = nolocked;
    /*用 strong 而非 weak 的原因：
      try_lock() 只尝试一次，不在循环中
      strong 保证不会出现"伪失败"（spurious failure）
      而 register_lock() 在 while(true) 循环中，用 weak 更高效*/
    return m_state.compare_exchange_strong(target, locked_no_waiting, std::memory_order_acq_rel, memory_order_relaxed);
}

/*
使用方式：
co_await mtx.lock();  // 阻塞式获取锁
// 临界区代码...
mtx.unlock();

1、调用 lock() → 返回 mutex_awaiter 对象
2、co_await 触发 await_suspend()
3、await_suspend() 调用 register_lock()
4、register_lock() 要么获得锁返回 false（不挂起），要么加入等待队列返回 true（挂起）
*/ 

auto mutex::lock() noexcept -> mutex_awaiter
{
    return mutex_awaiter(local_context(), *this);
}


/*
如果m_resume_list_head为空了，
说明需要从m_state里取出所有等待者补进m_resume_list_head。
如果m_resume_list_head不为空，就不急从m_state里取等待者

好处：
批量处理：一次 exchange 把屋外所有人拉进来，而不是每次 unlock 都去抢 m_state
减少竞争：m_resume_list_head 只有持锁者访问，无需原子操作
公平性：反转保证先来的先服务
*/
auto mutex::unlock() noexcept -> void
{
    assert(m_state.load(std::memory_order_acquire) != nolocked && "unlock the mutex with unlock state");

    auto to_resume = reinterpret_cast<mutex_awaiter*>(m_resume_list_head);
    if (to_resume == nullptr)
    {
        auto target = locked_no_waiting;
        if (m_state.compare_exchange_strong(target, nolocked, std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            return;// m_resume_list_head里没有等待者，m_state里也没有等待者，直接解锁成功
        }

        auto head = m_state.exchange(locked_no_waiting, std::memory_order_acq_rel);//一次性取出m_state所有等待者，减少竞争
        assert(head != nolocked && head != locked_no_waiting);

        auto awaiter = reinterpret_cast<mutex_awaiter*>(head);
        do
        {//链表反转（头插法顺序 → FIFO 顺序）
            auto temp       = awaiter->m_next;
            awaiter->m_next = to_resume;
            to_resume       = awaiter;
            awaiter         = temp;
        } while (awaiter != nullptr);
    }

    assert(to_resume != nullptr && "unexpected to_resume value: nullptr");
    m_resume_list_head = to_resume->m_next;
    to_resume->resume();
}

auto mutex::lock_guard() noexcept -> mutex_guard_awaiter
{
    return mutex_guard_awaiter(local_context(), *this);
}

}; // namespace coro