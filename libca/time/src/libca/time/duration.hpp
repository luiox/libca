#pragma once

#include "libca/core/datatype.hpp"

#include <chrono>

namespace ca::time {

/// @brief 纳秒精度时间间隔。
/// @note Duration 是纯值类型，不读取系统时钟；所有算术和单位转换均可 constexpr 求值。
class Duration {
public:
    constexpr Duration() noexcept = default;

    /// @brief 从纳秒构造时间间隔。
    static constexpr Duration from_nanoseconds(ca::i64 value) noexcept { return Duration(value); }
    /// @brief 从微秒构造时间间隔。
    static constexpr Duration from_microseconds(ca::i64 value) noexcept { return Duration(value * 1000); }
    /// @brief 从毫秒构造时间间隔。
    static constexpr Duration from_milliseconds(ca::i64 value) noexcept { return Duration(value * 1000000); }
    /// @brief 从秒构造时间间隔。
    static constexpr Duration from_seconds(ca::i64 value) noexcept { return Duration(value * 1000000000); }
    /// @brief 从分钟构造时间间隔。
    static constexpr Duration from_minutes(ca::i64 value) noexcept { return from_seconds(value * 60); }
    /// @brief 从小时构造时间间隔。
    static constexpr Duration from_hours(ca::i64 value) noexcept { return from_minutes(value * 60); }

    /// @brief 从任意 std::chrono::duration 转换为纳秒精度 Duration。
    template<typename Rep, typename Period>
    static constexpr Duration from_chrono(std::chrono::duration<Rep, Period> value) noexcept
    {
        return Duration(std::chrono::duration_cast<std::chrono::nanoseconds>(value).count());
    }

    constexpr ca::i64 nanoseconds() const noexcept { return nanoseconds_; }
    constexpr ca::i64 microseconds() const noexcept { return nanoseconds_ / 1000; }
    constexpr ca::i64 milliseconds() const noexcept { return nanoseconds_ / 1000000; }
    constexpr ca::i64 seconds() const noexcept { return nanoseconds_ / 1000000000; }
    constexpr double as_seconds_f64() const noexcept { return static_cast<double>(nanoseconds_) / 1000000000.0; }

    constexpr bool is_zero() const noexcept { return nanoseconds_ == 0; }
    constexpr bool is_negative() const noexcept { return nanoseconds_ < 0; }

    constexpr std::chrono::nanoseconds to_chrono() const noexcept
    {
        return std::chrono::nanoseconds(nanoseconds_);
    }

    constexpr Duration operator+() const noexcept { return *this; }
    constexpr Duration operator-() const noexcept { return Duration(-nanoseconds_); }
    constexpr Duration operator+(Duration other) const noexcept { return Duration(nanoseconds_ + other.nanoseconds_); }
    constexpr Duration operator-(Duration other) const noexcept { return Duration(nanoseconds_ - other.nanoseconds_); }
    constexpr Duration& operator+=(Duration other) noexcept
    {
        nanoseconds_ += other.nanoseconds_;
        return *this;
    }
    constexpr Duration& operator-=(Duration other) noexcept
    {
        nanoseconds_ -= other.nanoseconds_;
        return *this;
    }

    constexpr bool operator==(Duration other) const noexcept { return nanoseconds_ == other.nanoseconds_; }
    constexpr bool operator!=(Duration other) const noexcept { return nanoseconds_ != other.nanoseconds_; }
    constexpr bool operator<(Duration other) const noexcept { return nanoseconds_ < other.nanoseconds_; }
    constexpr bool operator>(Duration other) const noexcept { return nanoseconds_ > other.nanoseconds_; }
    constexpr bool operator<=(Duration other) const noexcept { return nanoseconds_ <= other.nanoseconds_; }
    constexpr bool operator>=(Duration other) const noexcept { return nanoseconds_ >= other.nanoseconds_; }

private:
    explicit constexpr Duration(ca::i64 nanoseconds) noexcept : nanoseconds_(nanoseconds) {}

    ca::i64 nanoseconds_{0};
};

}  // namespace ca::time
