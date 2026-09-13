#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace ca::thread {

namespace details {

class TimerState;
struct TimerEntry;

}  // namespace details

/// @brief 定时器句柄，用于取消尚未触发的定时任务。
///
/// 句柄可复制，副本间共享取消状态。cancel() 线程安全且不阻塞：不会等待正在执行的回调，
/// 也不会中断已经开始执行的回调。TimerManager 销毁后句柄退化为安全的空操作。
class TimerHandle
{
public:
    /// @brief 构造无效句柄，cancel() 为空操作。
    TimerHandle() noexcept = default;

    /// @brief 是否为 schedule_once()/schedule_repeating() 返回的有效句柄。
    explicit operator bool() const noexcept;

    /// @brief 取消对应定时器，线程安全且不阻塞。
    ///
    /// 只把任务从调度队列移除后立即返回，不等待回调结束。若回调已经开始执行，本次触发
    /// 照常完成（cancel 不中断执行），重复定时器随后不再续期。允许在回调内部取消自身
    /// 或安排新定时器，均不会死锁。可重复调用，幂等。
    void cancel() const noexcept;

private:
    friend class TimerManager;

    std::weak_ptr<details::TimerState>   state_;
    std::shared_ptr<details::TimerEntry> entry_;
};

/// @brief 基于 steady_clock 的定时器管理器，自带单个调度线程。
///
/// 回调在调度线程上串行执行。重复定时器的续期方式：callback 返回 true 才按 period 续期，
/// 返回 false 或抛出异常后停止；一次性任务用 schedule_once()。析构函数取消所有未到期
/// 任务、等待正在执行的回调返回后 join 调度线程；析构与其它线程的 schedule/cancel/
/// next_expiry 并发是安全的，但回调捕获的外部对象需由调用方保证存活期。
class TimerManager
{
public:
    /// @brief 创建管理器并启动调度线程。
    /// @throw 线程创建失败时抛出 std::system_error。
    TimerManager();

    /// @brief 停止调度：取消所有未到期任务，等待正在执行的回调返回后 join 调度线程。
    ~TimerManager();

    TimerManager(const TimerManager&)            = delete;
    TimerManager& operator=(const TimerManager&) = delete;

    /// @brief 安排一次性任务，到期后在调度线程执行 callback。
    /// @param delay 相对当前的延迟，负值按 0 处理（尽快触发）。
    /// @param callback 回调，不能为空。
    /// @return 取消句柄；callback 为空或管理器正在析构时返回无效句柄。
    TimerHandle schedule_once(std::chrono::milliseconds delay, std::function<void()> callback);

    /// @brief 安排重复任务，callback 返回 true 才续期，返回 false 或抛异常后停止。
    ///
    /// 以上一次回调结束时刻为基准加 period 得到下次到期时间（固定延迟语义，非固定频率，
    /// 长回调会顺延后续触发）。回调在调度线程串行执行，回调内可 cancel 自身或安排新任务。
    /// @param period 触发周期，必须大于 0。
    /// @param callback 回调，返回 true 表示继续按 period 续期。
    /// @return 取消句柄；callback 为空、period 非正或管理器正在析构时返回无效句柄。
    TimerHandle schedule_repeating(std::chrono::milliseconds period, std::function<bool()> callback);

    /// @brief 返回最近一个未取消任务的到期时间点，供 event loop 集成计算等待时长。
    /// @return 无任务时返回空 optional。返回值基于 steady_clock，可能早于当前时刻
    ///         （已到期、待调度线程触发的任务）。
    std::optional<std::chrono::steady_clock::time_point> next_expiry() const;

private:
    TimerHandle add_timer(std::chrono::milliseconds delay, std::function<bool()> callback, bool repeating);

    std::shared_ptr<details::TimerState> state_;
    std::thread                          worker_;
};

}  // namespace ca::thread
