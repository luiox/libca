#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "libca/core/status.hpp"
#include "libca/thread/stop_token.hpp"

namespace ca::thread {

/// @brief 线程池关闭策略。
enum class ShutdownMode
{
    /// 拒绝新任务，并执行完所有已排队任务。
    Drain,
    /// 请求运行中任务协作停止，并取消尚未开始的任务。
    CancelPending
};

/// @brief ThreadPool 的创建参数。
struct ThreadPoolOptions
{
    /// worker 数量；0 表示使用 hardware_concurrency，仍为 0 时回退到 1。
    usize thread_count{0};
    /// 待执行任务队列容量，必须大于 0。
    usize queue_capacity{256};
};

/// @brief CancelPending 关闭时，尚未开始的任务通过 future 抛出的异常。
class TaskCancelled : public std::exception
{
public:
    /// @brief 返回稳定的取消原因文本。
    const char* what() const noexcept override;
};

namespace details {

struct PoolTask
{
    std::function<void()> run;
    std::function<void()> cancel;
};

template<typename Callable, bool WithToken = std::is_invocable_v<Callable&, StopToken>>
struct TaskResult;

template<typename Callable>
struct TaskResult<Callable, true>
{
    using type = std::invoke_result_t<Callable&, StopToken>;
};

template<typename Callable>
struct TaskResult<Callable, false>
{
    static_assert(std::is_invocable_v<Callable&>,
                  "ThreadPool task must accept no arguments or one StopToken");
    using type = std::invoke_result_t<Callable&>;
};

template<typename Callable>
using TaskResultT = typename TaskResult<Callable>::type;

template<typename Result>
struct PreparedTask
{
    PoolTask            task;
    std::future<Result> future;
};

template<typename Result, typename Callable>
void execute_task(const std::shared_ptr<std::promise<Result>>& promise,
                  const std::shared_ptr<Callable>& callable, const StopToken& token) noexcept
{
    try {
        if constexpr (std::is_void_v<Result>) {
            if constexpr (std::is_invocable_v<Callable&, StopToken>)
                std::invoke(*callable, token);
            else
                std::invoke(*callable);
            promise->set_value();
        }
        else {
            if constexpr (std::is_invocable_v<Callable&, StopToken>)
                promise->set_value(std::invoke(*callable, token));
            else
                promise->set_value(std::invoke(*callable));
        }
    }
    catch (...) {
        try {
            promise->set_exception(std::current_exception());
        }
        catch (...) {
        }
    }
}

template<typename Function, typename Callable = std::decay_t<Function>>
PreparedTask<TaskResultT<Callable>> prepare_task(Function&& function, const StopToken& token)
{
    using Result            = TaskResultT<Callable>;
    auto promise            = std::make_shared<std::promise<Result>>();
    auto callable           = std::make_shared<Callable>(std::forward<Function>(function));
    auto cancellation_error = std::make_exception_ptr(TaskCancelled{});

    PreparedTask<Result> prepared;
    prepared.future   = promise->get_future();
    prepared.task.run = [promise, callable, token]() noexcept {
        execute_task<Result>(promise, callable, token);
    };
    prepared.task.cancel = [promise, cancellation_error]() noexcept {
        try {
            promise->set_exception(cancellation_error);
        }
        catch (...) {
        }
    };
    return prepared;
}

inline ca::core::Status task_allocation_error(const std::bad_alloc& error)
{
    return ca::core::ErrStatus(ca::core::StatusCode::RESOURCE_EXHAUSTED,
                               std::string("task setup failed: ") + error.what());
}

inline ca::core::Status task_setup_error(const std::exception& error)
{
    return ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                               std::string("task setup failed: ") + error.what());
}

inline ca::core::Status task_setup_error()
{
    return ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                               "task setup failed with a non-standard exception");
}

}   // namespace details

/// @brief 具有有界背压、协作取消和显式关闭状态机的固定大小线程池。
///
/// submit() 在队列满时阻塞，try_submit() 立即返回，submit_for() 限时等待。任务异常被
/// 保存在各自 future 中，worker 会继续运行。析构函数执行 Drain shutdown 和 join 作为
/// 兜底；调用方应显式关闭并检查 join() 返回状态。
class ThreadPool
{
public:
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// @brief 转移线程池所有权。
    ThreadPool(ThreadPool&& other) noexcept;

    /// @brief 排空并等待当前线程池后接管新对象。
    ThreadPool& operator=(ThreadPool&& other) noexcept;

    /// @brief 执行 Drain shutdown 和 join。
    ~ThreadPool();

    /// @brief 创建固定大小线程池。
    /// @return 队列容量无效或 worker 创建失败时返回错误。
    static ca::core::StatusResult<ThreadPool> create(
        const ThreadPoolOptions& options = ThreadPoolOptions{});

    /// @brief 提交任务，队列满时阻塞形成背压。
    ///
    /// callable 可以无参或接受一个 StopToken。任务返回值和原始异常通过 future 传播。
    template<typename Function, typename Callable = std::decay_t<Function>,
             typename Result = details::TaskResultT<Callable>>
    ca::core::StatusResult<std::future<Result>> submit(Function&& function)
    {
        static_assert(std::is_invocable_v<Callable&> || std::is_invocable_v<Callable&, StopToken>,
                      "ThreadPool task must accept no arguments or one StopToken");
        try {
            auto prepared = details::prepare_task(std::forward<Function>(function), stop_token());
            auto status   = enqueue(std::move(prepared.task));
            if (status.is_err())
                return ca::core::Err(status);
            return ca::core::Ok(std::move(prepared.future));
        }
        catch (const std::bad_alloc& error) {
            return ca::core::Err(details::task_allocation_error(error));
        }
        catch (const std::exception& error) {
            return ca::core::Err(details::task_setup_error(error));
        }
        catch (...) {
            return ca::core::Err(details::task_setup_error());
        }
    }

    /// @brief 尝试立即提交任务。
    /// @return 提交成功返回 future，队列已满返回空 optional，关闭后返回错误。
    template<typename Function, typename Callable = std::decay_t<Function>,
             typename Result = details::TaskResultT<Callable>>
    ca::core::StatusResult<std::optional<std::future<Result>>> try_submit(Function&& function)
    {
        static_assert(std::is_invocable_v<Callable&> || std::is_invocable_v<Callable&, StopToken>,
                      "ThreadPool task must accept no arguments or one StopToken");
        try {
            auto prepared = details::prepare_task(std::forward<Function>(function), stop_token());
            auto enqueued = try_enqueue(std::move(prepared.task));
            if (enqueued.is_err())
                return ca::core::Err(enqueued.unwrap_err());
            if (!enqueued.unwrap())
                return ca::core::Ok(std::optional<std::future<Result>>{});
            return ca::core::Ok(std::optional<std::future<Result>>(std::move(prepared.future)));
        }
        catch (const std::bad_alloc& error) {
            return ca::core::Err(details::task_allocation_error(error));
        }
        catch (const std::exception& error) {
            return ca::core::Err(details::task_setup_error(error));
        }
        catch (...) {
            return ca::core::Err(details::task_setup_error());
        }
    }

    /// @brief 在指定时长内等待队列容量并提交任务。
    /// @return 提交成功返回 future，超时返回空 optional，关闭后返回错误。
    template<typename Function, typename Callable = std::decay_t<Function>,
             typename Result = details::TaskResultT<Callable>>
    ca::core::StatusResult<std::optional<std::future<Result>>>
    submit_for(Function&& function, std::chrono::milliseconds timeout)
    {
        static_assert(std::is_invocable_v<Callable&> || std::is_invocable_v<Callable&, StopToken>,
                      "ThreadPool task must accept no arguments or one StopToken");
        try {
            auto prepared = details::prepare_task(std::forward<Function>(function), stop_token());
            auto enqueued = enqueue_for(std::move(prepared.task), timeout);
            if (enqueued.is_err())
                return ca::core::Err(enqueued.unwrap_err());
            if (!enqueued.unwrap())
                return ca::core::Ok(std::optional<std::future<Result>>{});
            return ca::core::Ok(std::optional<std::future<Result>>(std::move(prepared.future)));
        }
        catch (const std::bad_alloc& error) {
            return ca::core::Err(details::task_allocation_error(error));
        }
        catch (const std::exception& error) {
            return ca::core::Err(details::task_setup_error(error));
        }
        catch (...) {
            return ca::core::Err(details::task_setup_error());
        }
    }

    /// @brief 开始关闭线程池。
    ///
    /// Drain 排空队列；CancelPending 请求运行中任务停止并取消待执行任务。Drain 可以升级
    /// 为 CancelPending，反向调用不会降级。该方法幂等且不等待 worker 结束。
    ca::core::Status shutdown(ShutdownMode mode = ShutdownMode::Drain);

    /// @brief 等待所有 worker 结束。
    ///
    /// 必须先调用 shutdown()，且不能从本池的 worker 内调用。该方法幂等，并返回 worker
    /// 基础设施的首个错误。
    ca::core::Status join();

    /// @brief 返回运行中任务观察的共享停止令牌。
    StopToken stop_token() const noexcept;

    /// @brief 返回 worker 数量；移动后的空对象返回 0。
    usize worker_count() const noexcept;

    /// @brief 返回当前排队但尚未开始的任务数量。
    usize pending_task_count() const noexcept;

    /// @brief 是否已经开始 shutdown。
    bool is_shutdown() const noexcept;

    /// @brief 是否已经完成 join。
    bool is_joined() const noexcept;

private:
    class Impl;

    explicit ThreadPool(std::unique_ptr<Impl> impl) noexcept;

    ca::core::Status             enqueue(details::PoolTask task);
    ca::core::StatusResult<bool> try_enqueue(details::PoolTask task);
    ca::core::StatusResult<bool> enqueue_for(details::PoolTask         task,
                                             std::chrono::milliseconds timeout);
    void                         finish_noexcept() noexcept;

    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::thread
