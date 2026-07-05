#pragma once

#include "libca/core/datatype.hpp"
#include "libca/time/duration.hpp"

#include <chrono>

namespace ca::time {

/// @brief Unix epoch 纳秒时间戳。
/// @note Timestamp 是纯 Unix epoch 纳秒值；除 now/is_past/is_future 外，转换与算术均可 constexpr 求值。
class Timestamp {
public:
    constexpr Timestamp() noexcept = default;

    /// @brief 返回当前系统时钟时间。
    static Timestamp now() noexcept;
    /// @brief 从 Unix epoch 纳秒构造时间戳。
    static constexpr Timestamp from_unix_nanoseconds(ca::i64 value) noexcept { return Timestamp(value); }
    /// @brief 从 Unix epoch 毫秒构造时间戳。
    static constexpr Timestamp from_unix_milliseconds(ca::i64 value) noexcept { return Timestamp(value * 1000000); }
    /// @brief 从 Unix epoch 秒构造时间戳。
    static constexpr Timestamp from_unix_seconds(ca::i64 value) noexcept { return Timestamp(value * 1000000000); }

    /// @brief 从 std::chrono::system_clock::time_point 构造时间戳。
    template<typename DurationT>
    static constexpr Timestamp from_time_point(
        std::chrono::time_point<std::chrono::system_clock, DurationT> value) noexcept
    {
        const auto since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch());
        return Timestamp(since_epoch.count());
    }

    constexpr ca::i64 unix_nanoseconds() const noexcept { return nanoseconds_; }
    constexpr ca::i64 unix_milliseconds() const noexcept { return nanoseconds_ / 1000000; }
    constexpr ca::i64 unix_seconds() const noexcept { return nanoseconds_ / 1000000000; }

    /// @brief 转换为 std::chrono::system_clock::time_point。
    constexpr std::chrono::system_clock::time_point to_time_point() const noexcept
    {
        const auto duration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::nanoseconds(nanoseconds_));
        return std::chrono::system_clock::time_point(duration);
    }

    bool is_past() const noexcept;
    bool is_future() const noexcept;

    constexpr Timestamp operator+(Duration duration) const noexcept
    {
        return Timestamp(nanoseconds_ + duration.nanoseconds());
    }

    constexpr Timestamp operator-(Duration duration) const noexcept
    {
        return Timestamp(nanoseconds_ - duration.nanoseconds());
    }

    constexpr Duration operator-(Timestamp other) const noexcept
    {
        return Duration::from_nanoseconds(nanoseconds_ - other.nanoseconds_);
    }

    constexpr Timestamp& operator+=(Duration duration) noexcept
    {
        nanoseconds_ += duration.nanoseconds();
        return *this;
    }
    constexpr Timestamp& operator-=(Duration duration) noexcept
    {
        nanoseconds_ -= duration.nanoseconds();
        return *this;
    }

    constexpr bool operator==(Timestamp other) const noexcept { return nanoseconds_ == other.nanoseconds_; }
    constexpr bool operator!=(Timestamp other) const noexcept { return nanoseconds_ != other.nanoseconds_; }
    constexpr bool operator<(Timestamp other) const noexcept { return nanoseconds_ < other.nanoseconds_; }
    constexpr bool operator>(Timestamp other) const noexcept { return nanoseconds_ > other.nanoseconds_; }
    constexpr bool operator<=(Timestamp other) const noexcept { return nanoseconds_ <= other.nanoseconds_; }
    constexpr bool operator>=(Timestamp other) const noexcept { return nanoseconds_ >= other.nanoseconds_; }

private:
    explicit constexpr Timestamp(ca::i64 nanoseconds) noexcept : nanoseconds_(nanoseconds) {}

    ca::i64 nanoseconds_{0};
};

}  // namespace ca::time
