#include "coro/comp/event.hpp"
#include "coro/scheduler.hpp"

namespace coro::detail
{
// TODO[lab4a] : Add codes if you need
auto event_base::awaiter_base::await_ready() noexcept -> bool
{
    m_ctx.register_wait();// 增加引用计数
    return m_ev.is_set();//event 是否被设置决定协程是否陷入 suspend
}
auto event_base::awaiter_base::await_suspend(std::coroutine_handle<> handle) noexcept ->bool{
    m_await_coro=handle;
    // 将协程 awaiter 挂载到 event 中，这个过程期间有可能 event 被 set，因此返回 bool 表示是否挂载成功，挂载失败那么协程恢复运行
    //awaiter 会检查 event 是否被 set，如果已经 set 了那么协程继续运行，
    //如果没有 set，那么 awaiter 会保存当前协程的句柄，将自身挂载到 event 的数据结构里，
    //这个数据结构记录了所有挂载的 awaiter，然后当前协程陷入 suspend 状态。
    return m_ev.register_awaiters(this);
}
auto event_base::awaiter_base::await_resume() noexcept ->void{
    m_ctx.unregister_wait();    // 降低引用计数
}
auto event_base::set_state() noexcept ->void{
    auto flag=m_state.exchange(this,std::memory_order_acq_rel);
    if(flag!=this){// event 之前未被 set
        auto waiter=static_cast<awaiter_base*>(flag);
        resume_all_awaiters(waiter);// 取得列表头并恢复所有 suspend awaiter
    }
}
auto event_base::resume_all_awaiters(awaiter_ptr waiter) noexcept ->void{
    while(waiter!=nullptr){
        auto cur=static_cast<awaiter_base*> (waiter);//转换后才能访问 awaiter_base 特有的成员
        auto next = cur->m_next;
        cur->m_ctx.submit_task(cur->m_await_coro);// 提交协程到对应 context
        // 提交后协程可能立即执行完毕并析构 awaiter，此时 cur 已无效
        // waiter=cur->next();
        waiter = next;
    }
}
auto event_base::register_awaiters(awaiter_base* waiter) noexcept ->bool{
    const auto set_state =this;
    awaiter_ptr old_value = nullptr;
    // 利用 cas 操作确保挂载操作的原子性，处理并发竞争
    do{
        old_value = m_state.load(std::memory_order_acquire);
        if (old_value == set_state)
        {
            waiter->m_next = nullptr;
            return false;// event 已被 set，挂载失败
        }
        waiter->m_next = static_cast<awaiter_base*>(old_value);// 将当前链表头挂载到 awaiter 的 next 指针上，链表头插法
    }
    while(!m_state.compare_exchange_weak(old_value, waiter, std::memory_order_acquire));//waiter 成为新的链表头
    return true;// 挂载成功

}



};