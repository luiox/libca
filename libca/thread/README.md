# libca_thread

结构化并发基础设施：协作取消、作用域 join、有界背压队列与线程池。命名空间 `ca::thread`，构建目标 `libca_thread`。

> 设计与关闭协议见 `doc/thread-design.rst`；以下为快速示例。接口签名见头文件 Doxygen 注释。

## 启动可取消线程

```cpp
#include <libca/thread/thread.hpp>

using ca::thread::StopToken;
using ca::thread::Thread;

auto result = Thread::start([](StopToken token) {
    while (!token.stop_requested()) {
        // 执行一个可中断工作单元。
    }
});

if (result.is_err()) {
    handle_error(result.unwrap_err());
    return;
}

auto thread = std::move(result).unwrap();
thread.request_stop();
handle_status(thread.join());
```

`request_stop()` 只发送协作停止请求。线程函数必须检查令牌，或者使用 `token.wait_for()` 代替不可控的长时间休眠。

## 使用有界队列

```cpp
#include <libca/thread/bounded_queue.hpp>

auto result = ca::thread::BoundedQueue<Message>::create(256);
auto queue = std::move(result).unwrap();

queue.push(message); // 队列已满时等待消费者，产生背压。

auto item = queue.pop();
if (item.is_ok()) {
    consume(std::move(item).unwrap());
}

queue.close(); // 禁止新消息，消费者仍可排空已有消息。
```

不要把 `CANCELLED` 当成异常故障。它表示队列已经关闭并排空，是消费者退出循环的正常信号。

## 在线程池提交任务

```cpp
#include <libca/thread/thread_pool.hpp>

ca::thread::ThreadPoolOptions options;
options.thread_count = 4;
options.queue_capacity = 64;

auto result = ca::thread::ThreadPool::create(options);
auto pool = std::move(result).unwrap();

auto submitted = pool.submit([] {
    return build_index();
});
auto future = std::move(submitted).unwrap();

auto value = future.get();
pool.shutdown(ca::thread::ShutdownMode::Drain);
pool.join();
```

`submit()` 在队列满时等待，适合需要背压的生产者。不能等待时使用 `try_submit()`；需要限制等待时长时使用 `submit_for()`。

## 取消待执行任务

```cpp
auto submitted = pool.submit([](ca::thread::StopToken token) {
    while (!token.stop_requested()) {
        do_one_step();
    }
});

pool.shutdown(ca::thread::ShutdownMode::CancelPending);
pool.join();
```

`CancelPending` 会取消尚未开始的任务，并向正在执行的任务发出停止请求。被取消的待执行任务在调用 `future.get()` 时抛出 `ca::thread::TaskCancelled`。已经开始但忽略停止令牌的任务仍会继续运行，线程池不会强制终止它。

## 选择关闭模式

- 正常服务停机、批处理收尾：使用 `ShutdownMode::Drain`。
- 用户取消、错误恢复、无需继续积压任务：使用 `ShutdownMode::CancelPending`。
- 无论使用哪种模式，都应在 `shutdown()` 后调用 `join()` 并检查返回状态。
- `join()` 必须由线程池外部的生命周期控制线程调用，不能在池内任务中等待本池。
- 析构会执行排空关闭作为兜底，但不应依赖析构来观察 worker 基础设施错误。

## 定时执行任务

```cpp
#include "libca/thread/scheduled_executor.hpp"

ca::thread::ScheduledExecutorOptions options;
options.thread_count = 2;
options.task_capacity = 64;

auto created = ca::thread::ScheduledExecutor::create(options);
if (created.is_err()) {
    handle_error(created.unwrap_err());
    return;
}
auto executor = std::move(created).unwrap();
auto submitted = executor.schedule_after(std::chrono::milliseconds(25), [] {
    return 42;
});
if (submitted.is_err()) {
    handle_error(submitted.unwrap_err());
    return;
}
auto task = std::move(submitted).unwrap();
auto answer = task.future.get();
handle_status(executor.shutdown(ca::thread::ShutdownMode::Drain));
handle_status(executor.join());
```

`task_capacity` 约束所有尚未完成的任务，包含等待、运行和周期任务。容量用尽时提交返回
`RESOURCE_EXHAUSTED`。`schedule_at()` 接受 `steady_clock` 的绝对时间点，
`schedule_after()` 接受相对延迟。任务到期后由空闲 worker 执行。

周期任务可选择 `schedule_fixed_delay()` 或 `schedule_fixed_rate()`。固定延迟从上一次调用结束时起算，
固定频率沿提交时的时间轴续期并跳过已错过的时隙。同一周期任务的多次调用依次完成。

调用 `task.handle.cancel()` 可取消等待中的任务，并使其 future 抛出 `TaskCancelled`。
运行中的任务通过自己的 `StopToken` 观察取消请求。周期任务的 future 用于观察取消和回调异常。
`Drain` 等待已接受的一次性任务到期执行，并结束周期续期。`CancelPending` 取消等待中的任务并请求
运行中的任务停止。析构执行 `CancelPending` 和 join，生命周期控制线程可显式调用并检查状态。
