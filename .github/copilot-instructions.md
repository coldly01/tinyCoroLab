# tinyCoroLab Copilot Instructions

## 项目概述

tinyCoroLab 是一个基于 **C++20 协程 + Linux io_uring** 的高性能异步协程库实验课程。核心目标是通过分阶段实验构建完整的协程调度系统。

## 架构层次

```
scheduler (单例，管理所有 context)
    └── context (工作线程，每个拥有一个 engine)
            └── engine (核心执行引擎，管理任务队列 + io_uring)
                    ├── task<T> (协程任务封装)
                    └── uring_proxy (io_uring 封装)
```

**关键数据流**：`submit_to_scheduler()` → dispatcher 分发 → context 的 engine → 任务执行/IO 等待

## 核心文件导航

| 组件 | 头文件 | 源文件 | 实验 |
|------|--------|--------|------|
| task | `include/coro/task.hpp` | - | lab1 |
| engine | `include/coro/engine.hpp` | `src/engine.cpp` | lab2a |
| context | `include/coro/context.hpp` | `src/context.cpp` | lab2b |
| scheduler | `include/coro/scheduler.hpp` | `src/scheduler.cpp` | lab2b |
| 同步组件 | `include/coro/comp/*.hpp` | `src/comp/*.cpp` | lab4-5 |

## 开发命令

```bash
# 在 build/ 目录下执行
make build-labXX     # 编译特定实验测试
make test-labXX      # 运行功能测试
make memtest-labXX   # 运行内存安全测试 (valgrind)
make benchtest-labXX # 运行性能测试 (lab4/5)
```

## 代码规范

### 特殊标记
- `[[CORO_TEST_USED(labXX)]]` - 测试依赖的函数，**不要修改声明**
- `// TODO[labXX]:` - 需要补充实现的位置

### 线程安全模式
- **engine**: 单线程内使用，无需原子操作
- **context/scheduler**: 多线程访问，使用 `std::atomic` 和 `memory_order_acq_rel`
- 使用 `std::atomic_ref` 包装普通变量实现原子操作（因 `atomic<T>` 不可放入 vector）

### 命名空间
- `coro` - 主命名空间
- `coro::detail` - 内部实现（engine, meta_info）
- `coro::net` - 网络 IO awaiter

## 关键设计模式

### eventfd 事件复用
engine 使用单个 eventfd 接收多种事件，通过位掩码区分：
```cpp
// 高20位: task事件, 中20位: io事件, 低24位: cqe完成事件
task_mask = 0xFFFFF00000000000
cqe_mask  = 0x0000000000FFFFFF
```

### 协程生命周期
```cpp
task<T> coro_func() {
    // 第1次作为 Task 调度
    co_await some_io();  // 挂起，IO完成后 handle 重新入队
    // 第2次作为 Task 调度
    co_return result;
}
```

### 优雅停止机制
- `m_stop_token` = 忙碌 context 数量
- 当降为 0 时自动触发 `stop_impl()` 通知所有 context 退出

## 常见陷阱

1. **默认参数**: 只在 `.hpp` 声明中写，`.cpp` 定义中不要重复
2. **静态成员变量**: 需要在头文件声明 `static`，在源文件定义初始化
3. **空回调检查**: 调用 `std::function` 前先检查 `if (callback)`
4. **debug 模式栈溢出**: `-O0` 下协程循环调用过多会栈溢出（GCC 限制）

## 配置文件

修改 `config/config.h.in`（不是 `config.h`），cmake 会生成实际配置。

## 回复语言

使用**中文**回复和添加代码注释。
