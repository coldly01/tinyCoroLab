#include <atomic>

#include "config.h"

namespace coro::detail
{
/**
 * @brief std::vector<std::atomic<T>> will cause compile error, so use atomic_ref_wrapper
 *
 * @tparam T
 */
template<typename T>
struct alignas(config::kCacheLineSize) atomic_ref_wrapper
{
    // alignas 是 std::atomic_ref 要求的地址对齐
    alignas(std::atomic_ref<T>::required_alignment) T val;
    // 这里为何不直接用 std::atomic<T>呢？
    // 因为我们要在容器里存储多个该类型，由于 std::atomic<T>禁用了
    // 拷贝构造和拷贝赋值，所以直接用 std::atomic<T>会导致容器无法初始化
};

}; // namespace coro::detail