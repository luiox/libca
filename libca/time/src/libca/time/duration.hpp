#pragma once

#include "libca/core/datatype.hpp"

#include <chrono>
#include <limits>
#include <optional>
#include <type_traits>

namespace ca::time {

namespace detail {

// 商的上界由调用方指定，余数始终小于 divisor。按二进制逐位处理，避免 left * right 溢出。
// chrono 的正 Period::den 不超过 INT64_MAX，余数加倍或相加均可由 u64 容纳。
struct TimeDivision {
    ca::u64 quotient;
    ca::u64 remainder;
    bool overflow;
};

constexpr TimeDivision divide_time_product(ca::u64 left, ca::u64 right, ca::u64 divisor, ca::u64 limit) noexcept {
    ca::u64 quotient = 0;
    ca::u64 remainder = 0;
    const ca::u64 left_quotient = left / divisor;
    const ca::u64 left_remainder = left % divisor;
    for (ca::usize shift = 64; shift > 0; --shift) {
        if (quotient > limit / 2) {
            return {0, 0, true};
        }
        quotient *= 2;
        remainder *= 2;
        if (remainder >= divisor) {
            remainder -= divisor;
            if (quotient == limit) {
                return {0, 0, true};
            }
            ++quotient;
        }
        if (((right >> (shift - 1)) & 1) != 0) {
            if (left_quotient > limit - quotient) {
                return {0, 0, true};
            }
            quotient += left_quotient;
            remainder += left_remainder;
            if (remainder >= divisor) {
                remainder -= divisor;
                if (quotient == limit) {
                    return {0, 0, true};
                }
                ++quotient;
            }
        }
    }
    return {quotient, remainder, false};
}

}  // namespace detail

/// @brief 纳秒精度时间间隔。
/// @note Duration 是纯值类型，不读取系统时钟；所有算术和单位转换均可 constexpr 求值。
/// @note checked_* 返回 std::optional，无法用 i64 纳秒表示时返回 nullopt。
/// @warning 未带 checked_ 的单位工厂和算术要求结果及中间乘法均能由 i64 表示。
///          unary - 要求值不为 INT64_MIN。外部输入可先走 checked_*。
/// @see doc:time-design-overflow
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

    /// @brief 从微秒构造，溢出返回 nullopt。
    static constexpr std::optional<Duration> checked_from_microseconds(ca::i64 value) noexcept {
        return checked_from_units(value, 1000);
    }
    /// @brief 从毫秒构造，溢出返回 nullopt。
    static constexpr std::optional<Duration> checked_from_milliseconds(ca::i64 value) noexcept {
        return checked_from_units(value, 1000000);
    }
    /// @brief 从秒构造，溢出返回 nullopt。
    static constexpr std::optional<Duration> checked_from_seconds(ca::i64 value) noexcept {
        return checked_from_units(value, 1000000000);
    }
    /// @brief 从分钟构造，溢出返回 nullopt。
    static constexpr std::optional<Duration> checked_from_minutes(ca::i64 value) noexcept {
        return checked_from_units(value, 60000000000LL);
    }
    /// @brief 从小时构造，溢出返回 nullopt。
    static constexpr std::optional<Duration> checked_from_hours(ca::i64 value) noexcept {
        return checked_from_units(value, 3600000000000LL);
    }

    /// @brief 从任意 std::chrono::duration 转换为纳秒精度 Duration。
    /// @pre duration_cast 的中间运算及结果须可表示。浮点 Rep 须有限、截断后的纳秒值须可由 i64 表示。
    template<typename Rep, typename Period>
    static constexpr Duration from_chrono(std::chrono::duration<Rep, Period> value) noexcept {
        return Duration(std::chrono::duration_cast<std::chrono::nanoseconds>(value).count());
    }

    /// @brief 从整数 chrono duration 转换，纳秒结果溢出返回 nullopt。
    /// @details 支持不超过 64 位的内置整数 Rep（含 unsigned）；亚纳秒部分向零截断。
    ///          不接收浮点 Rep。所有正 Period 均按整数比精确计算，乘法中间值不会溢出。
    template<typename Rep, typename Period,
             std::enable_if_t<std::is_integral_v<Rep> && (sizeof(Rep) <= sizeof(ca::u64)), ca::i32> = 0>
    static constexpr std::optional<Duration> checked_from_chrono(std::chrono::duration<Rep, Period> value) noexcept {
        bool negative = false;
        if constexpr (std::is_signed_v<Rep>) {
            negative = value.count() < 0;
        }
        const ca::u64 count = negative ? magnitude(static_cast<ca::i64>(value.count()))
                                       : static_cast<ca::u64>(value.count());
        const ca::u64 limit = negative ? MIN_MAGNITUDE : MAX_MAGNITUDE;
        const auto seconds = detail::divide_time_product(count, Period::num, Period::den, limit / 1000000000);
        if (seconds.overflow) {
            return std::nullopt;
        }
        const auto fraction = detail::divide_time_product(
            seconds.remainder, 1000000000, Period::den, 999999999);
        const ca::u64 whole = seconds.quotient * 1000000000;
        if (fraction.quotient > limit - whole) {
            return std::nullopt;
        }
        return Duration(signed_nanoseconds(whole + fraction.quotient, negative));
    }

    /// @brief 纳秒值。
    constexpr ca::i64 nanoseconds() const noexcept { return nanoseconds_; }
    /// @brief 完整微秒数，向零截断。
    constexpr ca::i64 microseconds() const noexcept { return nanoseconds_ / 1000; }
    /// @brief 完整毫秒数，向零截断。
    constexpr ca::i64 milliseconds() const noexcept { return nanoseconds_ / 1000000; }
    /// @brief 完整秒数，向零截断。
    constexpr ca::i64 seconds() const noexcept { return nanoseconds_ / 1000000000; }
    /// @brief 浮点秒数。
    constexpr double as_seconds_f64() const noexcept { return static_cast<double>(nanoseconds_) / 1000000000.0; }

    constexpr bool is_zero() const noexcept { return nanoseconds_ == 0; }
    constexpr bool is_negative() const noexcept { return nanoseconds_ < 0; }

    constexpr std::chrono::nanoseconds to_chrono() const noexcept {
        return std::chrono::nanoseconds(nanoseconds_);
    }

    /// @brief 加法，溢出返回 nullopt。
    constexpr std::optional<Duration> checked_add(Duration other) const noexcept {
        if ((other.nanoseconds_ > 0 && nanoseconds_ > MAX_VALUE - other.nanoseconds_) ||
            (other.nanoseconds_ < 0 && nanoseconds_ < MIN_VALUE - other.nanoseconds_)) {
            return std::nullopt;
        }
        return Duration(nanoseconds_ + other.nanoseconds_);
    }

    /// @brief 减法，溢出返回 nullopt。
    constexpr std::optional<Duration> checked_sub(Duration other) const noexcept {
        if ((other.nanoseconds_ > 0 && nanoseconds_ < MIN_VALUE + other.nanoseconds_) ||
            (other.nanoseconds_ < 0 && nanoseconds_ > MAX_VALUE + other.nanoseconds_)) {
            return std::nullopt;
        }
        return Duration(nanoseconds_ - other.nanoseconds_);
    }

    /// @brief 乘整数倍，溢出返回 nullopt。
    constexpr std::optional<Duration> checked_mul(ca::i64 factor) const noexcept {
        const bool negative = (nanoseconds_ < 0) != (factor < 0);
        const ca::u64 count = magnitude(nanoseconds_);
        const ca::u64 multiplier = magnitude(factor);
        const ca::u64 limit = negative ? MIN_MAGNITUDE : MAX_MAGNITUDE;
        if (multiplier != 0 && count > limit / multiplier) {
            return std::nullopt;
        }
        return Duration(signed_nanoseconds(count * multiplier, negative));
    }

    /// @brief 取相反数，INT64_MIN 返回 nullopt。
    constexpr std::optional<Duration> checked_neg() const noexcept {
        return checked_mul(-1);
    }

    constexpr Duration operator+() const noexcept { return *this; }
    constexpr Duration operator-() const noexcept { return Duration(-nanoseconds_); }
    constexpr Duration operator+(Duration other) const noexcept { return Duration(nanoseconds_ + other.nanoseconds_); }
    constexpr Duration operator-(Duration other) const noexcept { return Duration(nanoseconds_ - other.nanoseconds_); }
    constexpr Duration& operator+=(Duration other) noexcept {
        nanoseconds_ += other.nanoseconds_;
        return *this;
    }
    constexpr Duration& operator-=(Duration other) noexcept {
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
    static constexpr ca::i64 MIN_VALUE = std::numeric_limits<ca::i64>::min();
    static constexpr ca::i64 MAX_VALUE = std::numeric_limits<ca::i64>::max();
    static constexpr ca::u64 MIN_MAGNITUDE = ca::u64{1} << 63;
    static constexpr ca::u64 MAX_MAGNITUDE = static_cast<ca::u64>(MAX_VALUE);

    explicit constexpr Duration(ca::i64 nanoseconds) noexcept : nanoseconds_(nanoseconds) {}

    static constexpr ca::u64 magnitude(ca::i64 value) noexcept {
        return value < 0 ? static_cast<ca::u64>(-(value + 1)) + 1 : static_cast<ca::u64>(value);
    }

    static constexpr ca::i64 signed_nanoseconds(ca::u64 value, bool negative) noexcept {
        if (negative && value == MIN_MAGNITUDE) {
            return MIN_VALUE;
        }
        return negative ? -static_cast<ca::i64>(value) : static_cast<ca::i64>(value);
    }

    static constexpr std::optional<Duration> checked_from_units(ca::i64 value, ca::i64 scale) noexcept {
        if (value < MIN_VALUE / scale || value > MAX_VALUE / scale) {
            return std::nullopt;
        }
        return Duration(value * scale);
    }

    ca::i64 nanoseconds_{0};
};

}  // namespace ca::time
