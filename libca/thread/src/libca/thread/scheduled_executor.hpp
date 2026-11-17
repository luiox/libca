#pragma once

#include "libca/thread/thread_pool.hpp"

#include <cstdint>

namespace ca::thread {

namespace details {
struct ScheduleState;
}

/// @brief 可复制的定时任务取消句柄，释放句柄不会取消任务。
/// @note 调度器销毁后句柄安全失效，取消协议见 doc:thread-design-scheduled-executor。
class ScheduledHandle {
public:
    /// @brief 构造空句柄。
    ScheduledHandle() noexcept = default;

    /// @brief 取消尚未开始的任务，或请求运行中任务的 StopToken。
    /// @return 首次成功请求取消返回 true，空句柄、已完成或已取消返回 false。
    /// @note 等待中的 future 以 TaskCancelled 完成；运行中的单次任务保留自然结果或异常。
    /// 周期任务等待本轮调用返回后以 TaskCancelled 完成，随后不再续期。
    bool cancel() const;

private:
    ScheduledHandle(std::weak_ptr<details::ScheduleState> state, std::uint64_t id) noexcept;
    std::weak_ptr<details::ScheduleState> state_;
    std::uint64_t                         id_{0};
    friend class ScheduledExecutor;
};

/// @brief 已接受任务的句柄和结果，future 只能移动。
template<typename Result>
struct ScheduledTask {
    /// 可复制的取消句柄。
    ScheduledHandle handle;
    /// 单次任务返回值或周期任务的终止结果，用户异常保留原始类型。
    std::future<Result> future;
};

/// @brief 固定 worker 数与定时任务总容量。
struct ScheduledExecutorOptions {
    /// worker 数，0 使用 hardware_concurrency，探测失败时回退为 1。
    usize thread_count{0};
    /// 未完成注册的上限，包含等待、运行和周期任务，必须大于 0。
    usize task_capacity{256};
};

/// @brief 基于 steady_clock 的有界固定 worker 定时执行器。
/// @note callable 接受零参数或一个任务独立的 StopToken，也支持 move-only 捕获。
/// 提交、句柄取消、shutdown 和 join 可并发。移动、赋值和析构需要拥有者同步。
/// 析构执行 CancelPending；从 worker 内析构时请求停止并分离线程，共享状态保持到 worker 退出。
/// 外部析构和 join 等待运行任务返回，支持取消的任务应及时观察 StopToken。
class ScheduledExecutor {
public:
    /// 所有截止时间使用的单调时钟。
    using Clock = std::chrono::steady_clock;

    ScheduledExecutor(const ScheduledExecutor&)            = delete;
    ScheduledExecutor& operator=(const ScheduledExecutor&) = delete;

    /// @brief 转移执行器所有权，句柄与已提交任务继续有效。
    ScheduledExecutor(ScheduledExecutor&& other) noexcept;
    /// @brief 取消并等待旧执行器，然后转移所有权。
    ScheduledExecutor& operator=(ScheduledExecutor&& other) noexcept;
    /// @brief 取消等待任务、请求运行任务停止并收尾 worker。
    ~ScheduledExecutor();

    /// @brief 创建执行器，参数无效或线程创建失败时返回 Status 错误。
    static ca::core::StatusResult<ScheduledExecutor> create(
        const ScheduledExecutorOptions& options = ScheduledExecutorOptions{});

    /// @brief 安排单次任务，到期后才允许 worker 开始，过去的时间点尽快执行。
    /// @return 满容量返回 RESOURCE_EXHAUSTED，关闭后返回 FAILED_PRECONDITION。
    template<typename Function, typename Callable = std::decay_t<Function>,
             typename Result = details::TaskResultT<Callable>>
    ca::core::StatusResult<ScheduledTask<Result>> schedule_at(Clock::time_point deadline, Function&& function) {
        try {
            StopSource source;
            auto       prepared = details::prepare_task(std::forward<Function>(function), source.token());
            auto       accepted = enqueue(std::move(prepared.task),
                                          {},
                                          std::move(source),
                                          deadline,
                                          std::chrono::milliseconds::zero(),
                                          RepeatMode::Once);
            if (accepted.is_err())
                return ca::core::Err(accepted.unwrap_err());
            return ca::core::Ok(ScheduledTask<Result>{std::move(accepted).unwrap(), std::move(prepared.future)});
        } catch (const std::bad_alloc& error) {
            return ca::core::Err(details::task_allocation_error(error));
        } catch (const std::exception& error) {
            return ca::core::Err(details::task_setup_error(error));
        } catch (...) {
            return ca::core::Err(details::task_setup_error());
        }
    }

    /// @brief 在指定延迟后安排单次任务，负延迟按零处理。
    /// @return 无法表示截止时间时返回 OUT_OF_RANGE，其余错误同 schedule_at。
    template<typename Function, typename Callable = std::decay_t<Function>,
             typename Result = details::TaskResultT<Callable>>
    ca::core::StatusResult<ScheduledTask<Result>> schedule_after(std::chrono::milliseconds delay, Function&& function) {
        auto deadline = deadline_after(delay);
        if (deadline.is_err())
            return ca::core::Err(deadline.unwrap_err());
        return schedule_at(deadline.unwrap(), std::forward<Function>(function));
    }

    /// @brief 按本轮完成时刻加 period 续期，首次延迟可为零或负数。
    /// @note period 必须为正；同一注册的调用严格不重叠。返回值被忽略。
    /// future<void> 在取消时抛出 TaskCancelled，在回调抛异常时保留原始异常并停止续期。
    template<typename Function>
    ca::core::StatusResult<ScheduledTask<void>> schedule_fixed_delay(std::chrono::milliseconds initial_delay,
                                                                     std::chrono::milliseconds period,
                                                                     Function&&                function) {
        return schedule_repeating(initial_delay, period, std::forward<Function>(function), RepeatMode::FixedDelay);
    }

    /// @brief 按首次截止时间加 period 的时间网格续期。
    /// @note 调用超时后跳过已错过的周期，下一次严格晚于本轮完成时刻，无追赶突发和调用重叠。
    /// 参数、取消、异常和 future 协议同 schedule_fixed_delay。
    template<typename Function>
    ca::core::StatusResult<ScheduledTask<void>> schedule_fixed_rate(std::chrono::milliseconds initial_delay,
                                                                    std::chrono::milliseconds period,
                                                                    Function&&                function) {
        return schedule_repeating(initial_delay, period, std::forward<Function>(function), RepeatMode::FixedRate);
    }

    /// @brief 拒绝新任务并开始关闭，可从回调内调用，允许 Drain 升级为 CancelPending。
    /// @note Drain 保留单次任务的原截止时间，取消等待中的周期注册，运行中的周期在本轮后停止。
    /// CancelPending 还取消等待中的单次任务并请求所有运行任务的 StopToken。
    ca::core::Status shutdown(ShutdownMode mode = ShutdownMode::CancelPending);
    /// @brief shutdown 后等待全部 worker 退出和所有 future 终结，可重复调用。
    /// @return 未关闭、移动后或从本执行器 worker 调用时返回 FAILED_PRECONDITION。
    ca::core::Status join();
    /// @brief 返回未完成注册数量，包含运行中的任务。
    usize task_count() const;
    /// @brief 返回固定 worker 数，移动后的空对象返回 0。
    usize worker_count() const noexcept;
    /// @brief 是否已开始关闭，移动后的空对象返回 true。
    bool is_shutdown() const;

private:
    enum class RepeatMode { Once, FixedDelay, FixedRate };
    class Impl;
    explicit ScheduledExecutor(std::unique_ptr<Impl> impl) noexcept;
    static ca::core::StatusResult<Clock::time_point> deadline_after(std::chrono::milliseconds delay);
    ca::core::StatusResult<ScheduledHandle> enqueue(details::PoolTask                       task,
                                                    std::function<void(std::exception_ptr)> fail, StopSource source,
                                                    Clock::time_point deadline, std::chrono::milliseconds period,
                                                    RepeatMode mode);
    void                                    finish_noexcept() noexcept;

    template<typename Function, typename Callable = std::decay_t<Function>>
    ca::core::StatusResult<ScheduledTask<void>> schedule_repeating(std::chrono::milliseconds delay,
                                                                   std::chrono::milliseconds period,
                                                                   Function&& function, RepeatMode mode) {
        auto deadline = deadline_after(delay);
        if (deadline.is_err())
            return ca::core::Err(deadline.unwrap_err());
        if (period <= std::chrono::milliseconds::zero())
            return ca::core::Err(
                ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT, "scheduled period must be positive"));
        auto first_interval = deadline_after(period);
        if (first_interval.is_err())
            return ca::core::Err(first_interval.unwrap_err());
        try {
            auto       promise  = std::make_shared<std::promise<void>>();
            auto       callable = std::make_shared<Callable>(std::forward<Function>(function));
            auto       future   = promise->get_future();
            StopSource source;
            auto       token = source.token();
            auto       fail  = [promise](std::exception_ptr error) noexcept {
                try {
                    promise->set_exception(error);
                } catch (...) {}
            };
            details::PoolTask task;
            task.run = [callable, token]() {
                if constexpr (std::is_invocable_v<Callable&, StopToken>)
                    std::invoke(*callable, token);
                else
                    std::invoke(*callable);
            };
            const auto cancelled = std::make_exception_ptr(TaskCancelled{});
            task.cancel          = [fail, cancelled]() noexcept { fail(cancelled); };
            auto accepted =
                enqueue(std::move(task), std::move(fail), std::move(source), deadline.unwrap(), period, mode);
            if (accepted.is_err())
                return ca::core::Err(accepted.unwrap_err());
            return ca::core::Ok(ScheduledTask<void>{std::move(accepted).unwrap(), std::move(future)});
        } catch (const std::bad_alloc& error) {
            return ca::core::Err(details::task_allocation_error(error));
        } catch (const std::exception& error) {
            return ca::core::Err(details::task_setup_error(error));
        } catch (...) {
            return ca::core::Err(details::task_setup_error());
        }
    }

    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::thread
