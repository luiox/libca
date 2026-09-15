---
version: 1.2
update:
2026-09-13 - 增补 §6：TimerManager / EventBus / ObjectPool 三个新组件的设计取舍
2026-07-14 - 由 design.md 改名补 YAML 头，删除使用文档（并入 README），本文成为 thread 唯一设计文档
---

# libca_thread 设计文档

## 1. 目标与边界

`libca_thread` 为 C++17 项目提供结构化并发基础设施。它不是给
`std::thread`、`std::mutex` 重新命名，而是补足标准库在 C++20 以前缺少的协作取消、
线程作用域生命周期、有界背压、任务异常隔离和线程池关闭协议。

模块只依赖 `libca_core` 和 C++ 标准库。公开类型使用 `CamelCase`，方法和字段使用
`snake_case`。预期依赖方向为：

```text
libca_core <- libca_thread
libca_core <- libca_io <- libca_net
                    ^
                    +--- libca_process
```

本模块不提供强制终止线程的接口。C++ 无法安全地从外部杀死一个正在持锁或修改共享
状态的线程，所有停止行为都必须通过 `StopToken` 协作完成。

## 2. 公开类型

### 2.1 StopSource 与 StopToken

`StopSource` 和 `StopToken` 共享一个引用计数停止状态，语义参考 C++20
`std::stop_source` / `std::stop_token`：

```cpp
StopSource source;
StopToken token = source.token();

source.request_stop();
if (token.stop_requested()) {
    // 收尾并退出
}
```

- `StopSource` 和 `StopToken` 都可复制，复制后仍指向同一停止状态。
- `request_stop()` 只在第一次状态转换时返回 `true`，后续调用返回 `false`。
- 停止状态是单向的：`Running -> StopRequested`，不能重置。
- 默认构造的 `StopToken` 没有停止状态，`stop_possible()` 返回 `false`。
- `wait()` 和 `wait_for()` 允许阻塞代码等待停止请求，避免轮询。

停止请求只是信号，不代表线程已经结束，也不会中断任意系统调用。

### 2.2 Thread

`Thread` 是 move-only 的结构化线程，语义接近 `std::jthread`：

```cpp
auto started = Thread::start([](StopToken token) {
    while (!token.stop_requested()) {
        do_one_step();
    }
});

Thread worker = std::move(started).unwrap();
worker.request_stop();
Status status = worker.join();
```

`Thread::start()` 接受无参 callable，或接受一个 `StopToken` 的 callable。需要额外参数时，
调用方使用 lambda 捕获，避免接口内部复制未知参数。

生命周期契约：

1. `Thread` 析构时先 `request_stop()`，再等待线程结束。
2. 不公开 `detach()`，防止线程静默逃离拥有它的作用域。
3. `join()` 可重复调用；第一次执行原生 join，后续返回缓存的完成状态。
4. callable 抛出的异常不会越过线程入口，也不会触发 `std::terminate`。异常被捕获，
   `join()` 返回 `StatusCode::INTERNAL`，消息包含可获得的异常文本。
5. `request_stop()` 不等待线程退出；是否及时退出取决于 callable 是否观察令牌。

移动赋值给一个仍在运行的 `Thread` 时，旧线程会先请求停止并 join，再接管新线程。

### 2.3 BoundedQueue<T>

`BoundedQueue<T>` 是 move-only 的多生产者、多消费者有界队列。它将背压作为接口语义，
而不是在过载时无限扩张内存。

```cpp
auto created = BoundedQueue<Job>::create(128);
auto queue = std::move(created).unwrap();

queue.push(job);                       // 满时阻塞
queue.try_push(job);                   // 满时返回 false
queue.push_for(job, 50ms);             // 限时等待容量
auto next = queue.pop();               // 空时阻塞
```

队列状态为：

```text
Open --close()----------> ClosedDraining --queue empty--> ClosedEmpty
  +----close_and_take()-> ClosedEmpty
```

- `push()` 在队列满时等待，因此提供自然背压。
- `try_push()` 不等待；`push_for()` 最多等待指定时长。
- `close()` 禁止新元素进入并唤醒所有等待者，但已有元素仍可被消费者排空。
- `close_and_take()` 关闭队列并把所有待处理元素移动给调用方。
- 关闭且排空后，`pop()` 返回 `StatusCode::CANCELLED`。
- 限时操作只有在“仍开放但超时”时返回空 optional；关闭不是超时，而是状态错误。
- 容量必须大于 0，由 `create()` 校验并通过 `StatusResult` 报错。

### 2.4 ThreadPool

`ThreadPool` 由固定数量的 `Thread`、一个 `BoundedQueue` 和共享停止状态组成：

```cpp
ThreadPoolOptions options;
options.thread_count = 4;
options.queue_capacity = 64;

auto created = ThreadPool::create(options);
auto pool = std::move(created).unwrap();

auto submitted = pool.submit([](StopToken token) -> ResultType {
    return calculate(token);
});
auto future = std::move(submitted).unwrap();

pool.shutdown(ShutdownMode::Drain);
pool.join();
```

任务 callable 可以无参，也可以接受线程池共享的 `StopToken`。返回值和异常通过
`std::future` 传递：

- 正常返回值由 `future.get()` 取得。
- 任务抛出的异常保存在对应 future 中，worker 线程继续处理后续任务。
- `CancelPending` 丢弃的待执行任务在 future 中抛出 `TaskCancelled`。
- worker 基础设施异常由 `Thread::join()` 转为 `Status`，不与任务异常混在一起。

提交接口对应三种背压策略：

| 接口 | 队列满时行为 |
| --- | --- |
| `submit()` | 等待容量，形成背压 |
| `try_submit()` | 立即返回空 optional |
| `submit_for()` | 等待到截止时间，超时返回空 optional |

## 3. 关闭状态机

线程池状态为：

```text
Running --shutdown(Drain)--------> ShuttingDownDrain --join()--> Joined
   |                                      |
   +--shutdown(CancelPending)--> ShuttingDownCancel <----------+
```

`ShutdownMode::Drain`：

- 立即拒绝新任务。
- 关闭队列的生产端。
- worker 执行完所有已排队任务后退出。
- 不请求任务停止，保证已接收任务正常完成。

`ShutdownMode::CancelPending`：

- 立即拒绝新任务。
- 请求共享 `StopToken` 停止，使正在运行且支持取消的任务可以提前退出。
- 从队列取出尚未开始的任务，并用 `TaskCancelled` 完成它们的 future。
- 已经运行的任务不能被强杀；`join()` 仍等待它们返回。

已经进入 `Drain` 的线程池允许升级为 `CancelPending`，反向降级无效。`shutdown()` 幂等。
`join()` 要求先调用 `shutdown()`，并且自身幂等。析构函数作为兜底执行
`shutdown(Drain)` 和 `join()`，但业务代码应显式关闭以处理返回状态。

`join()` 不能从本线程池的 worker 内调用，否则返回 `FAILED_PRECONDITION`。worker 不能
等待自身退出；从外部生命周期控制线程调用 `shutdown()` 和 `join()` 才能保持完整的
结构化等待语义。

## 4. 并发与错误模型

- `StopSource`、`StopToken`、`BoundedQueue` 的公开操作可被多个线程并发调用。
- `ThreadPool::submit*()` 和 `shutdown()` 可并发；关闭与提交竞争时，任务要么成功进入
  队列，要么得到 `FAILED_PRECONDITION`，不会处于未归属状态。
- `Thread` 的控制方法不面向多个控制线程并发调用；拥有者负责串行化 `join()` 和移动。
- 预期运行时失败使用 `ca::core::Status`：参数错误为 `INVALID_ARGUMENT`，关闭后提交为
  `FAILED_PRECONDITION`，队列关闭且排空为 `CANCELLED`，线程创建资源不足为
  `RESOURCE_EXHAUSTED`，线程入口异常为 `INTERNAL`。
- 用户任务异常保留其原始异常类型，通过 future 传播，不转换为 `Status`。

## 5. 测试策略

测试必须覆盖：

- 停止状态共享、幂等请求、等待与超时。
- `Thread` 析构停止并 join、显式 join、无 token callable、异常隔离和移动语义。
- 有界队列 FIFO、多生产者/消费者、满队列背压、三种 push/pop 方式、关闭排空和
  `close_and_take()`。
- 线程池返回值、void 任务、异常 future、并发提交、队列背压、`Drain`、
  `CancelPending`、运行中任务协作取消、关闭后拒绝提交、重复 shutdown/join。
- `thread_count == 0` 的自动回退和非法队列容量。

测试不得依赖长时间 sleep 判定正确性；同步点优先使用条件变量、promise/future 和
明确的截止时间。

## 6. Timer / EventBus / ObjectPool（2026-09 增补）

三个组件与既有的 ThreadPool/BoundedQueue 同属并发设施，均不依赖其它 libca 模块
（仅 core 定长类型），延续「回调在锁外执行、异常不逃出」的模块纪律。

### 6.1 TimerManager

- 单调度线程 + `(expiry, id)` 有序 set；条件变量谓词校验「队首是否变化」，新任务
  成为最近到期者时立即唤醒重算等待时长。`next_expiry()` 供将来 event loop 计算等待。
- 重复定时器取**固定延迟语义**（上次回调结束 + period），长回调顺延后续触发，
  换取实现简单与「回调不重叠」的强保证。
- 取消语义：句柄 cancel 只把任务移出队列后立即返回，不中断已开始的回调；回调内
  自取消 / 安排新任务均安全（回调执行期间不持有内部锁）。续期前复查 cancelled，
  取消与触发的线性化点在「出队」。
- 回调异常视为返回 false（重复任务停止续期），不逃出调度线程。
- 析构 = 取消全部未到期任务 + join 调度线程（等待进行中回调返回）。

### 6.2 EventBus

- 字符串事件名 → 监听器表；`emit` 在锁内拷贝快照、锁外逐个同步调用。由此得到
  三条保证：回调内 subscribe/unsubscribe/emit 其它事件不死锁；本轮快照固定
  （过程中注销者本轮仍被调用，文档明示）；单个监听器异常被隔离，其余照常执行。
- 句柄持 `weak_ptr<State>`：总线销毁后注销退化为空操作。空事件表及时清理，
  避免长期运行时监听表无限增长。
- 明确不做：负载路由（回调自带 lambda 捕获）、异步投递（需要时显式 post 到
  ThreadPool / TimerManager）。回调内同步 emit **同一事件**会无限递归，头注释禁止。

### 6.3 ObjectPool

- 借还式语义完全由 `shared_ptr` 自定义 deleter 表达：最后一次引用释放时对象回池，
  调用方无感知。
- 快路径 `try_lock`：抢到锁走池化路径；锁竞争、池空或已关闭时**直接构造新对象，
  永不阻塞**（ZLToolKit ResourcePool 同款取舍：高并发下池化退化为普通分配）。
- deleter 持 `weak_ptr<State>`：借出对象可以比池活得久，池析构后归还的对象直接
  销毁，不泄漏不悬垂。
- `on_recycle` 重置钩子抛异常时吞掉异常并销毁对象、不入池（重置失败的对象状态
  不可信）。`shutdown()` 后 obtain 直接构造、归还改为销毁。
- 有意不做空闲上限（idle 无界）：与「永不阻塞」配套的取舍，容量治理交给调用方
  （控制同时在借对象数）。后续如有真实需求再加 max_idle。
