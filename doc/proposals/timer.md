# 提案：Timer（定时器）

## 背景

旧 `libca.core/src/thread/Timer.{hpp,cpp}` 是纯占位实现：

- `PassedTimer::start/stop/reset` 全部空实现
- `PassedTimer::passedMs/passedS/passedM` 永远 `return false`
- `CallbackTimer` 是空类，无任何方法
- `Timer.cpp` 只有 `#include "Timer.hpp"`

旧代码不可用，已随 `libca.core/` 删除。本提案记录未来在新 `libca/` 中实现定时器的设计意图。

## 建议落点

候选位置（任选其一，需结合 thread 模块职责边界再定）：

- `libca/thread`（与线程/任务调度强相关）
- `libca/time`（与时间点/时间间隔概念接近）

## 设计要点

1. **基于新库原语**：用 `ca::time::Duration` / `ca::time::Timestamp` 表达时间间隔，
   用 `ca::thread::Thread` + `ca::thread::StopToken` 实现可取消的后台任务，避免旧代码
   detached thread 无 shutdown 的问题。
2. **`ca::core::Status` 错误处理**：调度失败（线程创建失败、定时器已停止）通过
   `StatusResult<T>` 返回，不抛异常。
3. **`PassedTimer` 等价物**：简单的时间间隔测量器，记录起始 `Timestamp`，提供
   `elapsed()` / `passed(Duration)` 接口。纯查询，不启动线程。
4. **`CallbackTimer` 等价物**：定时触发回调。建议至少支持：
   - 一次性定时（after(Duration, callback)）
   - 周期性定时（every(Duration, callback)）
   - 取消（基于 `StopToken` 或返回的 `TimerHandle`）
5. **命名**：`ca::thread::IntervalMeter` / `ca::thread::Timer` / `ca::thread::Scheduler`
   等符合 snake_case + PascalCase 规范的命名，避免沿用旧 `PassedTimer/CallbackTimer`。

## 不在范围

- 本提案不规定精确 API 签名，留给具体设计阶段决定。
- 不限定放在 thread 还是 time，需要根据调度模型讨论。
