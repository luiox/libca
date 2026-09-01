#pragma once

#include "libca/core/datatype.hpp"

namespace ca::time {

/// @brief 时间相关工具函数。
///
/// `TimeUtil` 提供 runtime intrinsic 常用的两个时钟入口：Unix epoch 毫秒和
/// 单调纳秒计数。它不保存时区、日历或线程状态。
class TimeUtil
{
public:
    /// @brief 返回 Unix epoch 毫秒时间戳，语义对齐 Java `System.currentTimeMillis()`。
    /// @return 自 1970-01-01T00:00:00Z 起经过的毫秒数。
    static ca::i64 current_time_millis() noexcept;

    /// @brief 返回单调时钟纳秒计数，仅用于相对时间差，语义对齐 Java `System.nanoTime()`。
    /// @return 单调时钟的纳秒计数；绝对值没有跨进程或跨机器含义。
    static ca::i64 nano_time() noexcept;
};

}   // namespace ca::time
