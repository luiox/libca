#pragma once

#include "libca/core/datatype.hpp"
#include "libca/time/duration.hpp"
#include "libca/time/time_util.hpp"

namespace ca::time {

/// @brief 单调秒表，构造即开始计时。
///
/// `Stopwatch` 基于 `TimeUtil::nano_time()`（steady_clock）测量相对耗时，
/// 只保存一个起点，不持有线程或系统资源。典型用法：
///
/// @code
/// Stopwatch watch;
/// do_work();
/// auto cost = watch.elapsed();
/// watch.restart();  // 重新从零计时
/// @endcode
///
/// @note elapsed() 的精度和稳定性取决于平台单调时钟；重复调用不产生额外开销。
class Stopwatch {
public:
    /// @brief 构造即启动：以当前单调时刻为计时起点。
    Stopwatch() noexcept : base_nanos_(TimeUtil::nano_time()), running_(true) {}

    /// @brief 返回从起点到当前时刻的累计耗时。
    /// @note 处于 reset 停表状态时恒返回 0。
    Duration elapsed() const noexcept
    {
        if (!running_)
            return Duration::from_nanoseconds(base_nanos_);
        return Duration::from_nanoseconds(TimeUtil::nano_time() - base_nanos_);
    }

    /// @brief 重新从零计时：起点重置为当前时刻并保持运行状态。
    void restart() noexcept
    {
        base_nanos_ = TimeUtil::nano_time();
        running_ = true;
    }

    /// @brief 停表并清零：elapsed() 恒返回 0，直到再次 restart()。
    void reset() noexcept
    {
        base_nanos_ = 0;
        running_ = false;
    }

    /// @brief 判断秒表是否处于计时中状态（reset 后为 false，restart 后为 true）。
    bool is_running() const noexcept { return running_; }

private:
    // running_ 为 true 时保存起点单调时刻，为 false 时保存冻结的耗时纳秒值。
    ca::i64 base_nanos_;
    bool running_;
};

}  // namespace ca::time
