#include "libca/thread/thread_pool.hpp"

#include <algorithm>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "libca/thread/bounded_queue.hpp"
#include "libca/thread/thread.hpp"

namespace ca::thread {
namespace {

enum class PoolState
{
    Running,
    ShuttingDown,
    Joined
};

ca::core::Status empty_pool_error(const char* operation)
{
    return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                               std::string(operation) + " on a moved ThreadPool");
}

ca::core::Status closed_pool_error()
{
    return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                               "thread pool is shutting down");
}

}   // namespace

class ThreadPool::Impl
{
public:
    explicit Impl(BoundedQueue<details::PoolTask> queue_value)
        : queue(std::make_shared<BoundedQueue<details::PoolTask>>(std::move(queue_value)))
    {}

    ~Impl() { queue->close(); }

    static void worker_loop(const std::shared_ptr<BoundedQueue<details::PoolTask>>& queue)
    {
        for (;;) {
            auto next = queue->pop();
            if (next.is_err())
                return;
            auto task = std::move(next).unwrap();
            task.run();
        }
    }

    std::shared_ptr<BoundedQueue<details::PoolTask>> queue;
    StopSource                                       stop_source;
    std::vector<Thread>                              workers;
    mutable std::mutex                               state_mutex;
    std::mutex                                       join_mutex;
    PoolState                                        state{PoolState::Running};
    ShutdownMode                                     shutdown_mode{ShutdownMode::Drain};
    ca::core::Status                                 join_status;
};

const char* TaskCancelled::what() const noexcept
{
    return "thread pool task was cancelled before execution";
}

ThreadPool::ThreadPool(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

ThreadPool::ThreadPool(ThreadPool&& other) noexcept = default;

ThreadPool& ThreadPool::operator=(ThreadPool&& other) noexcept
{
    if (this != &other) {
        finish_noexcept();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

ThreadPool::~ThreadPool()
{
    finish_noexcept();
}

ca::core::StatusResult<ThreadPool> ThreadPool::create(const ThreadPoolOptions& options)
{
    if (options.queue_capacity == 0)
        return ca::core::Err(ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT,
                                                 "thread pool queue capacity must be nonzero"));

    auto queue = BoundedQueue<details::PoolTask>::create(options.queue_capacity);
    if (queue.is_err())
        return ca::core::Err(queue.unwrap_err());

    try {
        auto  impl  = std::make_unique<Impl>(std::move(queue).unwrap());
        usize count = options.thread_count;
        if (count == 0)
            count = static_cast<usize>(std::thread::hardware_concurrency());
        if (count == 0)
            count = 1;
        impl->workers.reserve(count);

        for (usize index = 0; index < count; ++index) {
            auto queue   = impl->queue;
            auto started = Thread::start([queue]() { Impl::worker_loop(queue); });
            if (started.is_err()) {
                impl->queue->close();
                return ca::core::Err(started.unwrap_err());
            }
            impl->workers.push_back(std::move(started).unwrap());
        }
        return ca::core::Ok(ThreadPool(std::move(impl)));
    }
    catch (const std::exception& error) {
        return ca::core::Err(
            ca::core::ErrStatus(ca::core::StatusCode::RESOURCE_EXHAUSTED,
                                std::string("thread pool creation failed: ") + error.what()));
    }
    catch (...) {
        return ca::core::Err(
            ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                "thread pool creation failed with a non-standard exception"));
    }
}

ca::core::Status ThreadPool::enqueue(details::PoolTask task)
{
    if (impl_ == nullptr)
        return empty_pool_error("submit");
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        if (impl_->state != PoolState::Running)
            return closed_pool_error();
    }
    return impl_->queue->push(std::move(task));
}

ca::core::StatusResult<bool> ThreadPool::try_enqueue(details::PoolTask task)
{
    if (impl_ == nullptr)
        return ca::core::Err(empty_pool_error("try_submit"));
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        if (impl_->state != PoolState::Running)
            return ca::core::Err(closed_pool_error());
    }
    return impl_->queue->try_push(std::move(task));
}

ca::core::StatusResult<bool> ThreadPool::enqueue_for(details::PoolTask         task,
                                                     std::chrono::milliseconds timeout)
{
    if (impl_ == nullptr)
        return ca::core::Err(empty_pool_error("submit_for"));
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        if (impl_->state != PoolState::Running)
            return ca::core::Err(closed_pool_error());
    }
    return impl_->queue->push_for(std::move(task), timeout);
}

ca::core::Status ThreadPool::shutdown(ShutdownMode mode)
{
    if (impl_ == nullptr)
        return empty_pool_error("shutdown");

    std::deque<details::PoolTask> pending;
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        // shutdown 幂等：已 Joined 直接返回成功。
        if (impl_->state == PoolState::Joined)
            return ca::core::OkStatus();
        if (impl_->state == PoolState::Running) {
            impl_->state         = PoolState::ShuttingDown;
            impl_->shutdown_mode = mode;
            if (mode == ShutdownMode::CancelPending) {
                // CancelPending：请求共享停止令牌，把队列里尚未开始的任务全部取出，
                // 让在跑的任务可协作退出、待跑的任务直接以 TaskCancelled 完成 future。
                impl_->stop_source.request_stop();
                for (auto& worker : impl_->workers)
                    worker.request_stop();
                pending = impl_->queue->close_and_take();
            }
            else {
                // Drain：只关队列生产端，worker 继续消费已排队任务后自然退出。
                impl_->queue->close();
            }
        }
        else if (impl_->shutdown_mode == ShutdownMode::Drain &&
                 mode == ShutdownMode::CancelPending) {
            // 允许从 Drain 升级为 CancelPending（紧急中止），反向降级无效。
            impl_->shutdown_mode = ShutdownMode::CancelPending;
            impl_->stop_source.request_stop();
            for (auto& worker : impl_->workers)
                worker.request_stop();
            pending = impl_->queue->close_and_take();
        }
    }

    // 在锁外完成 future，避免 cancel 回调里持锁导致死锁。
    for (auto& task : pending)
        task.cancel();
    return ca::core::OkStatus();
}

ca::core::Status ThreadPool::join()
{
    if (impl_ == nullptr)
        return empty_pool_error("join");

    // join_mutex 串行化多个控制线程的 join 调用，避免并发 join 同一组 worker。
    std::lock_guard<std::mutex> join_lock(impl_->join_mutex);
    {
        std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
        if (impl_->state == PoolState::Running)
            return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                       "shutdown must be called before join");
        if (impl_->state == PoolState::Joined)
            return impl_->join_status;
    }

    // 禁止 worker 从池内任务中 join 自己的池，否则会死锁。
    for (const auto& worker : impl_->workers) {
        if (worker.joinable() && worker.id() == std::this_thread::get_id())
            return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                       "a worker cannot join its own thread pool");
    }

    ca::core::Status result = ca::core::OkStatus();
    for (auto& worker : impl_->workers) {
        auto status = worker.join();
        if (result.is_ok() && status.is_err())
            result = status;
    }
    {
        std::lock_guard<std::mutex> state_lock(impl_->state_mutex);
        impl_->join_status = result;
        impl_->state       = PoolState::Joined;
    }
    return result;
}

StopToken ThreadPool::stop_token() const noexcept
{
    return impl_ == nullptr ? StopToken{} : impl_->stop_source.token();
}

usize ThreadPool::worker_count() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->workers.size();
}

usize ThreadPool::pending_task_count() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->queue->size();
}

bool ThreadPool::is_shutdown() const noexcept
{
    if (impl_ == nullptr)
        return true;
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    return impl_->state != PoolState::Running;
}

bool ThreadPool::is_joined() const noexcept
{
    if (impl_ == nullptr)
        return false;
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    return impl_->state == PoolState::Joined;
}

void ThreadPool::finish_noexcept() noexcept
{
    if (impl_ == nullptr)
        return;
    try {
        shutdown(ShutdownMode::Drain);
        join();
    }
    catch (...) {
    }
}

}   // namespace ca::thread
