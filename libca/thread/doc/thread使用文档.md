# libca_thread 使用文档

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

`request_stop()` 只发送协作停止请求。线程函数必须检查令牌，或者使用
`token.wait_for()` 代替不可控的长时间休眠。

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

不要把 `CANCELLED` 当成异常故障。它表示队列已经关闭并排空，是消费者退出循环的正常
信号。

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

`submit()` 在队列满时等待，适合需要背压的生产者。不能等待时使用 `try_submit()`；需要
限制等待时长时使用 `submit_for()`。

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

`CancelPending` 会取消尚未开始的任务，并向正在执行的任务发出停止请求。被取消的待执行
任务在调用 `future.get()` 时抛出 `ca::thread::TaskCancelled`。已经开始但忽略停止令牌的
任务仍会继续运行，线程池不会强制终止它。

## 选择关闭模式

- 正常服务停机、批处理收尾：使用 `ShutdownMode::Drain`。
- 用户取消、错误恢复、无需继续积压任务：使用 `ShutdownMode::CancelPending`。
- 无论使用哪种模式，都应在 `shutdown()` 后调用 `join()` 并检查返回状态。
- 析构会执行排空关闭作为兜底，但不应依赖析构来观察 worker 基础设施错误。
