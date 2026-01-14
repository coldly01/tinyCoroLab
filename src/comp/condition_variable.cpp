#include "coro/comp/condition_variable.hpp"
#include "coro/scheduler.hpp"

namespace coro
{
/*
 * condition_variable 使用流程：
 * 
 * co_await mtx.lock();              // 1. 先获取锁
 * co_await cv.wait(mtx, cond);      // 2. 等待条件（会自动释放锁 → 挂起 → 被唤醒后重新获取锁）
 * // 3. 恢复后仍持有锁
 * mtx.unlock();
 * 
 * wait 内部执行流程：
 * 
 * 协程 A：正在运行，持有 mutex 锁
 *     │
 *     ▼
 * 执行 co_await cv.wait(mtx)
 *     │
 *     ▼
 * await_suspend() 被调用（协程仍在运行！）
 *     │
 *     ▼
 * 条件不满足 → "我马上要挂起了"
 *     │
 *     ▼
 * 释放锁 m_mtx.unlock()  ← 挂起前必须释放，否则别人永远拿不到锁
 *     │
 *     ▼
 * return true → 协程正式挂起
 *     │
 *     ▼
 * 被 notify 唤醒 → wake_up() 调用 mutex_awaiter::register_lock() 重新获取锁
 *     │
 *     ▼
 * await_resume() 返回，协程继续执行（仍持有锁）
 * 
 * 核心语义：wait 时自动释放锁，唤醒后自动重新获取锁
 */

// TODO[lab5b] : Add codes if you need
auto condition_variable::cv_awaiter::await_suspend(std::coroutine_handle<> handle) noexcept -> bool
{
    m_await_coro = handle; // 保存当前协程句柄便于之后恢复
    return register_lock();
}
// register_lock 返回 true 即表示协程将自身注册到了 suspend awaiter 链表中，即陷入 suspend 状态
// 否则协程继续运行

auto condition_variable::cv_awaiter::await_resume() noexcept -> void
{
    m_ctx.unregister_wait(m_suspend_state);// 只有 m_suspend_state=true 时才减少计数
}

auto condition_variable::cv_awaiter::register_lock() noexcept -> bool
{
    if (m_cond && m_cond())
    {
        // 如果条件谓词存在且条件谓词返回 true，那么协程继续运行，
        // 这一点与 C++ 中的 std::condition_variable 有所不同，
        // std::condition_variable 在调用 wait 时不会考虑条件谓词，只有被 notify 时才会
        return false;
    }

    // condition_variable 内部必须维持一个变量来表示 awaiter 是否已增加引用计数，
    // 因为协程因 condition_variable 陷入阻塞态且被 notify 时如果条件谓词返回 false，
    // 协程将再次陷入 suspend 状态，如果这里只是调用 m_ctx.register_wait()，那么
    // context 的引用计数便会多次增加，而协程恢复后只会对引用计数减 1，这导致 context
    // 将因引用计数永远无法终止
    // 这个问题的解决方案很多，读者只需要明白其核心问题即可
    m_ctx.register_wait(!m_suspend_state);// 只有 m_suspend_state=false 时才真正增加计数
    m_suspend_state = true;

    // 此时协程确定要陷入 suspend 状态，那么需要将自身挂载到 condition_variable
    // 的 suspend awaiter 链表中
    register_cv();

    // std::condition_variable 在调用 register_lock当前函数时其必须持有锁，tinyCoro 同理，
    // 因为协程即将要陷入 suspend 状态，所以释放锁
    m_mtx.unlock();
    return true;
}
auto condition_variable::cv_awaiter::register_cv() noexcept -> void
{
    m_next = nullptr;

    m_cv.m_lock.lock(); // 用自旋锁保护临界区

    // 将当前 awaiter 挂载到链表尾部
    if (m_cv.m_tail == nullptr)
    {
        m_cv.m_head = m_cv.m_tail = this;
    }
    else
    {
        m_cv.m_tail->m_next = this;
        m_cv.m_tail         = this;
    }
    m_cv.m_lock.unlock();
}
// 尝试唤醒并不意味着协程会立刻恢复运行
auto condition_variable::cv_awaiter::wake_up() noexcept -> void
{
    // 这里调用 mutex_awaiter::register_lock 是想让 awaiter 尝试获取锁，
    // 如果该函数返回 false，即协程获取到了锁，那么立刻尝试恢复该协程运行
    // 如果该函数返回 true，即协程因对已上锁的锁加锁陷入了 suspend 状态，
    // 那么什么也不做，因为该协程一定会通过 mutex.unlock 被唤醒，即将
    // 唤醒协程的义务转移给了 mutex
    if (!mutex_awaiter::register_lock())
    {
        resume();
    }
    // 读者应该确保理解上述提到的协程的状态转移过程
}
//在 wake_up() 中，当协程成功获取到锁后调用 resume()
auto condition_variable::cv_awaiter::resume() noexcept -> void
{
    if (m_cond && !m_cond())
    {
        // 存在条件谓词且条件谓词返回 false，那么当前 awaiter 需要再次将自身注册到
        // condition_variable 的 suspend awaiter 链表中并陷入 suspend 状态
        m_ctx.register_wait(!m_suspend_state);
        m_suspend_state = true;

        register_cv();
        m_mtx.unlock();
        return;
    }
    // 不存在条件谓词或条件谓词返回 true，那么协程正式恢复运行，
    // 这里直接复用 mutex_awaiter 的方法即可
    mutex_awaiter::resume(); 
}
condition_variable::~condition_variable() noexcept
{
    assert(m_head == nullptr && m_tail == nullptr && "exist sleep awaiter when cv destruct");
}

auto condition_variable::wait(mutex& mtx) noexcept -> cv_awaiter
{
    return cv_awaiter(local_context(), mtx, *this);
}

auto condition_variable::wait(mutex& mtx, cond_type&& cond) noexcept -> cv_awaiter
{
    return cv_awaiter(local_context(), mtx, *this, cond);
}

auto condition_variable::wait(mutex& mtx, cond_type& cond) noexcept -> cv_awaiter
{
    return cv_awaiter(local_context(), mtx, *this, cond);
}

/*
生产者协程执行 cv.notify_one()
    │
    ▼
m_lock.lock()      ← 生产者获取 cv 内部自旋锁
    │
    ▼
取出队头 cur
    │
    ▼
m_lock.unlock()    ← 生产者释放 cv 内部自旋锁
    │
    ▼
cur->wake_up()     ← 生产者调用，尝试唤醒等待者
*/
auto condition_variable::notify_one() noexcept -> void
{
    m_lock.lock(); // 自旋锁保护临界区
    auto cur = m_head;
    if (cur != nullptr)
    {
        // 将链表头作为待唤醒的 awaiter 并更新链表头
        m_head = reinterpret_cast<cv_awaiter*>(m_head->m_next);
        if (m_head == nullptr)
        {
            m_tail = nullptr;
        }
        m_lock.unlock();
        cur->wake_up(); // 尝试唤醒协程
    }
    else
    {
        m_lock.unlock();
    }
}
auto condition_variable::notify_all() noexcept -> void
{
    cv_awaiter* nxt{nullptr};

    m_lock.lock(); // 自旋锁保护临界区
    // 取出全部链表
    auto cur_head = m_head;
    m_head = m_tail = nullptr;
    m_lock.unlock();

    // 按 FIFO 顺序唤醒
    while (cur_head != nullptr)
    {
        nxt = reinterpret_cast<cv_awaiter*>(cur_head->m_next);
        cur_head->wake_up();
        cur_head = nxt;
    }
}

} // namespace coro


/*
只需要执行以下指令，不用执行make benchtest-lab5b：
/home/coldly/mycode/tinyCoroLab/build/benchtests && ./lab5b_bench --benchmark_filter=coro_cv 2>&1
你的 condition_variable 实现完全正确！ ✅

卡住的原因是 benchmark 文件中的 其他测试 有问题：

threadpool_stl_cv_notifyall - 死锁
coro_stl_cv_notifyall - 在协程中使用 std::condition_variable 导致 glibc 断言失败
这些都是 benchmark 对比测试的问题，不是你的 cv 实现问题。
Lab5b 测试结果
测试	结果
test-lab5b	✅ 24/24 通过
coro_cv_notifyall	✅ 4.73 ms
coro_cv_notifyone	✅ 24.6 ms
*/