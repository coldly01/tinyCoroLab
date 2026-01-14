#include "coro/context.hpp"
#include "coro/scheduler.hpp"

//每个 context 对应一个工作线程：调度器可以创建多个 context，实现多线程并发
namespace coro
{

/// 构造函数：从全局计数器获取唯一 ID
context::context() noexcept
{
    m_id = ginfo.context_id.fetch_add(1, std::memory_order_relaxed);
}

/// 初始化当前线程的 context：绑定线程局部变量，初始化 engine
auto context::init() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    linfo.ctx=this;
    m_engine.init();
}

/// 清理当前线程的 context：解绑线程局部变量，清理 engine
auto context::deinit() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    linfo.ctx=nullptr;
    m_engine.deinit();
}

/// 启动工作线程：创建新线程执行 init -> run -> deinit 流程
/// @note 此函数立即返回，不会阻塞调用者
auto context::start() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    // 创建 jthread，lambda 捕获 this 指针以访问成员函数
    // jthread 会自动传入 stop_token 用于协作式停止
    m_job = make_unique<jthread>(
        [this](stop_token token)
        {
            this->init();         // 初始化当前线程的 context
            // 如果外部没有注入 stop_cb，那么自行为其添加逻辑
            if(!(this->m_stop_cb)){
                m_stop_cb=[&](){m_job->request_stop();}; // 请求线程停止
            }
            this->run(token);     // 主循环，token 用于检测是否需要停止
            this->deinit();       // 清理
        });
}

/// 通知工作线程停止：发送停止信号并唤醒可能阻塞的 engine
auto context::notify_stop() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    m_job->request_stop();  // 设置 stop_token，使 token.stop_requested() 返回 true
    m_engine.wake_up(1);    // 唤醒可能在等待 IO 的 engine
}

/// 提交协程任务到当前 context 的 engine
auto context::submit_task(std::coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    m_engine.submit_task(handle);
}


/// 增加等待任务计数（原子操作，线程安全）
auto context::register_wait(int register_cnt) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    m_num_wait_task.fetch_add(register_cnt, std::memory_order_acq_rel);
}

/// 减少等待任务计数（原子操作，线程安全）
auto context::unregister_wait(int register_cnt) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    m_num_wait_task.fetch_sub(register_cnt, std::memory_order_acq_rel);
}
// 驱动 engine 从任务队列取出任务并执行
auto context::process_work() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    auto num=m_engine.num_task_schedule();
    for(size_t i=0;i<num;i++){
        m_engine.exec_one_task();
    }

}
// 驱动 engine 执行 IO 任务
auto context::poll_work() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    m_engine.poll_submit();

}
// 判断是否没有 IO 任务以及引用计数是否为 0
auto context::empty_wait_work() noexcept -> bool
{
    // TODO[lab2b]: Add you codes
    return m_num_wait_task.load(std::memory_order_acquire)==0&&m_engine.empty_io();
}


/// 工作线程主循环：处理任务直到收到停止信号
auto context::run(stop_token token) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    // while(true){
    //     process_work();
    //     if(token.stop_requested()&&empty_wait_work()){
    //         if(m_engine.ready()){
    //             continue;
    //         }
    //         else{
    //             break;
    //         }
    // }
    // poll_work();
    // if(token.stop_requested()&&empty_wait_work()&&m_engine.ready()==false){
    //     break;
    // }
    // }

    // 原实现：（无法通过memtest）
    // while(!token.stop_requested()){
    //     process_work();
    //     if(empty_wait_work()){
    //         if(m_engine.ready()){
    //             continue;
    //         }
    //         else{
    //             m_stop_cb();  // 问题：没检查 m_stop_cb 是否为空，可能段错误
    //         }
    //     }
    //
    //     poll_work();
    // }
    // 问题：循环条件是 !token.stop_requested()，调用 m_stop_cb() 后如果 token 变了就直接退出
    //       但没有处理完剩余任务

    // 新实现：
    while (true)
    {
        // 1. 处理所有就绪的任务
        process_work();

        // 2. 检查是否可以停止（收到停止信号 + 没有待处理工作）
        if (token.stop_requested() && empty_wait_work())
        {
            // 如果还有任务，继续处理
            if (m_engine.ready())
            {
                continue;
            }
            // 没有任务了，可以退出
            break;
        }

        // 3. 没有收到停止信号时，检查是否空闲
        if (!token.stop_requested() && empty_wait_work() && !m_engine.ready())
        {
            // 空闲了，通知 scheduler（如果有回调的话）
            if (m_stop_cb)  // 关键：检查回调是否为空
            {
                m_stop_cb();
            }
        }

        // 4. 等待/处理 IO
        poll_work();
    }
}

auto context::set_stop_cb(stop_cb cb) noexcept -> void{
    m_stop_cb=cb;
}
}; // namespace coro