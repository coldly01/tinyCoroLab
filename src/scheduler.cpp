#include "coro/scheduler.hpp"

namespace coro
{
auto scheduler::init_impl(size_t ctx_cnt) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    detail::init_meta_info();
    m_ctx_cnt = ctx_cnt;
    m_ctxs    = detail::ctx_container{};
    m_ctxs.reserve(m_ctx_cnt);
    for (int i = 0; i < m_ctx_cnt; i++)
    {
        m_ctxs.emplace_back(std::make_unique<context>());
    }
    m_dispatcher.init(m_ctx_cnt, &m_ctxs);

#ifdef ENABLE_MEMORY_ALLOC
    coro::allocator::memory::mem_alloc_config config;
    m_mem_alloc.init(config);
    ginfo.mem_alloc = &m_mem_alloc;
#endif
    m_stop_token=m_ctx_cnt;
    m_ctx_stop_flag=stop_flag_type(m_ctx_cnt, detail::atomic_ref_wrapper<int>{.val = 1});
}

auto scheduler::start_impl() noexcept -> void{
    for (int i = 0; i < m_ctx_cnt; i++)
    {
        m_ctxs[i]->set_stop_cb(
            [&, i]()
            {
                // context 将其关联的状态设置为 0 即已执行完所有任务，cnt 总是为 1
                auto cnt = std::atomic_ref(this->m_ctx_stop_flag[i].val).fetch_and(0, memory_order_acq_rel);
                // 将 scheduler 的引用计数减 1，如果引用计数降至 0，那么触发 scheduler 发送停止信号
                //当全局忙碌计数减到 0 时，说明当前这个 context 是最后一个忙碌的，该触发停止了
                if (this->m_stop_token.fetch_sub(cnt) == cnt)
                {
                    this->stop_impl();
                }
            });
        m_ctxs[i]->start();
    }
}

auto scheduler::loop_impl() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    start_impl();
    for(size_t i=0;i<m_ctx_cnt;i++){
        m_ctxs[i]->join();
    }
}

auto scheduler::stop_impl() noexcept -> void
{
    // TODO[lab2b]: example function
    // This is an example which just notify stop signal to each context,
    // if you don't need this, function just ignore or delete it
    for (int i = 0; i < m_ctx_cnt; i++)
    {
        m_ctxs[i]->notify_stop();
    }
}

auto scheduler::submit_task_impl(std::coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    assert(this->m_stop_token.load(std::memory_order_acquire) != 0 && "error! submit task after scheduler loop finish");
    size_t ctx_id = m_dispatcher.dispatch();
    //m_stop_token = 活跃 context 数量
    //当 m_stop_token 降为 0 时 → 所有 context 都空闲 → 触发 stop_impl()
    m_stop_token.fetch_add(
        1 - std::atomic_ref(m_ctx_stop_flag[ctx_id].val).fetch_or(1, memory_order_acq_rel), memory_order_acq_rel);
    m_ctxs[ctx_id]->submit_task(handle);
}
}; // namespace coro
