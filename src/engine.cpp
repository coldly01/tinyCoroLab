#include "coro/engine.hpp"
#include "coro/io/io_info.hpp"
#include "coro/task.hpp"

namespace coro::detail
{
using std::memory_order_relaxed;

/// 初始化 engine：绑定线程局部变量，重置 IO 计数器，初始化 io_uring 代理
auto engine::init() noexcept -> void
{
    // TODO[lab2a]: Add you codes
    linfo.egn=this;
    m_num_io_running=0;
    m_num_io_wait_submit=0;
    m_upxy.init(config::kEntryLength);
}

/// 清理 engine：释放 io_uring 资源，重置计数器，清空任务队列
auto engine::deinit() noexcept -> void
{
    // TODO[lab2a]: Add you codes
    m_upxy.deinit();
    m_num_io_running=0;
    m_num_io_wait_submit=0;
    mpmc_queue<coroutine_handle<>> task_queue;  // 1. 创建一个空的临时队列
    m_task_queue.swap(task_queue);               // 2. 交换：m_task_queue 变空，task_queue 拿走原数据
                                                 // 3. 函数结束时，task_queue 析构，原数据被释放
}

/// 检查是否有待调度的任务
/// @return true 表示任务队列非空
auto engine::ready() noexcept -> bool
{
    // TODO[lab2a]: Add you codes
    return !m_task_queue.was_empty();
}

/// 获取一个空闲的 io_uring SQE（提交队列条目）
/// @return 指向可用 SQE 的指针，用于填充 IO 请求
auto engine::get_free_urs() noexcept -> ursptr
{
    // TODO[lab2a]: Add you codes
    return m_upxy.get_free_sqe();
}

/// 获取当前待调度任务的数量
auto engine::num_task_schedule() noexcept -> size_t
{
    // TODO[lab2a]: Add you codes
    return m_task_queue.was_size();
}

/// 从任务队列中取出一个协程句柄
/// @return 待执行的协程句柄
auto engine::schedule() noexcept -> coroutine_handle<>
{
    // TODO[lab2a]: Add you codes
    auto coro=m_task_queue.pop();
    assert(bool(coro));
    return coro;
}

/// 唤醒可能阻塞在 eventfd 上的 engine
/// @param val 写入 eventfd 的值
auto engine::wake_up(uint64_t val)noexcept->void{
    m_upxy.write_eventfd(val);
}

/// 提交一个协程任务到任务队列，并唤醒 engine
/// @param handle 协程句柄
auto engine::submit_task(coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab2a]: Add you codes
    assert(handle != nullptr && "engine get nullptr task handle");
    m_task_queue.push(handle);
    wake_up(task_flag);//自动生成的参数
}

/// 执行一个任务：从队列取出协程并恢复执行，若完成则清理
auto engine::exec_one_task() noexcept -> void
{
    auto coro = schedule();
    coro.resume();
    if (coro.done())
    {
        clean(coro);
    }
}

/// 处理一个已完成的 CQE（完成队列条目）
/// @param cqe 指向完成队列条目的指针
auto engine::handle_cqe_entry(urcptr cqe) noexcept -> void
{
    auto data = reinterpret_cast<io::detail::io_info*>(io_uring_cqe_get_data(cqe));
    data->cb(data, cqe->res);
}

/// 执行 IO 提交：将等待提交的 IO 请求批量提交到内核
auto engine::do_io_submit() noexcept -> void
{
    if(m_num_io_wait_submit>0){
        auto _ = m_upxy.submit();
        m_num_io_running+=m_num_io_wait_submit;
        m_num_io_wait_submit=0;
    }
}

/// IO 轮询主函数：提交 IO、等待 IO 完成、处理已完成的 IO
auto engine::poll_submit() noexcept -> void
{
    // TODO[lab2a]: Add you codes
    do_io_submit();
    auto cnt=m_upxy.wait_eventfd();
    if(!wake_by_cqe(cnt)){
        return;
    }
    //这句代码中的load是原子操作，但是m_num_io_running是size_t类型，engine只在单线程内，不需要原子操作保证线程安全
    // auto num = m_upxy.peek_batch_cqe(m_urc.data(), m_num_io_running.load(std::memory_order_acquire));
    auto num=m_upxy.peek_batch_cqe(m_urc.data(),m_num_io_running);
    if(num!=0){
        for(size_t i=0;i<num;i++){
            handle_cqe_entry(m_urc[i]);
        }
        m_upxy.cq_advance(num);
        //这句代码中的load是原子操作，但是m_num_io_running是size_t类型，engine只在单线程内，不需要原子操作保证线程安全
        // m_num_io_running.fetch_sub(num,std::memory_order_acq_rel);
        m_num_io_running-=num;
    }
}

/// 增加待提交 IO 计数（调用此函数后需调用 do_io_submit 实际提交）
auto engine::add_io_submit() noexcept -> void
{
    // TODO[lab2a]: Add you codes
    m_num_io_wait_submit++;
}

/// 检查是否没有正在进行或待提交的 IO
/// @return true 表示 IO 全部完成且无待提交
auto engine::empty_io() noexcept -> bool
{
    // TODO[lab2a]: Add you codes
    return m_num_io_wait_submit == 0&&m_num_io_running==0;
}
}; // namespace coro::detail
