#pragma once

#include <atomic>

#ifdef ENABLE_MEMORY_ALLOC
    #include "coro/allocator/memory.hpp"
#endif
#include "coro/attribute.hpp"

namespace coro
{
class context;
}; // namespace coro

namespace coro::detail
{
using config::ctx_id;
using std::atomic;
class engine;

/**
 * @brief 存储线程局部变量（每个线程拥有独立的副本）
 *
 * 用于在任意位置快速访问当前线程的 context 和 engine，无需传参。
 */
struct CORO_ALIGN local_info//存的是指针
{
    context* ctx{nullptr}; ///< 指向当前线程的协程上下文
    engine*  egn{nullptr}; ///< 指向当前线程的调度引擎
    // TODO: Add more local var
};

/**
 * @brief 存储全局共享变量（所有线程共享同一份）
 *
 * 包含全局原子计数器，用于为新创建的 context/engine 实例分配唯一 ID。
 * 使用 atomic 保证多线程环境下的安全递增。
 */
struct global_info//存的是 ID 计数器，不是 ID 本身
{
    atomic<ctx_id>   context_id{0}; ///< 全局计数器，用于分配唯一的 context ID
    atomic<uint32_t> engine_id{0};  ///< 全局计数器，用于分配唯一的 engine ID
// TODO: Add more global var
#ifdef ENABLE_MEMORY_ALLOC
    coro::allocator::memory::memory_allocator<config::kMemoryAllocator>* mem_alloc; ///< 可选的内存分配器
#endif
};

inline thread_local local_info linfo;
inline global_info             ginfo;

// init global info
inline auto init_meta_info() noexcept -> void
{
    ginfo.context_id = 0;
    ginfo.engine_id  = 0;
#ifdef ENABLE_MEMORY_ALLOC
    ginfo.mem_alloc = nullptr;
#endif
}

// This function is used to distinguish whether you are currently in a worker thread
inline auto is_in_working_state() noexcept -> bool
{
    return linfo.ctx != nullptr;
}
}; // namespace coro::detail