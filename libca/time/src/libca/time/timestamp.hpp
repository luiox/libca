#pragma once

#include "libca/core/datatype.hpp"
#include "libca/time/duration.hpp"

#include <chrono>
#include <optional>
#include <type_traits>

namespace ca::time {

/// @brief Unix epoch 纳秒时间戳。
/// @note Timestamp 是纯 Unix epoch 纳秒值；除 now/is_past/is_future 外，转换与算术均可 constexpr 求值。
/// @warning 未带 checked_ 的单位工厂和算术要求结果及中间乘法均能由 i64 表示。
/// @see doc:time-design-overflow
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

    /// @brief 从 Unix epoch 毫秒构造，溢出返回 nullopt。
    static constexpr std::optional<Timestamp> checked_from_unix_milliseconds(ca::i64 value) noexcept {
        const auto duration = Duration::checked_from_milliseconds(value);
        if (!duration) {
            return std::nullopt;
        }
        return Timestamp(duration->nanoseconds());
    }

    /// @brief 从 Unix epoch 秒构造，溢出返回 nullopt。
    static constexpr std::optional<Timestamp> checked_from_unix_seconds(ca::i64 value) noexcept {
        const auto duration = Duration::checked_from_seconds(value);
        if (!duration) {
            return std::nullopt;
        }
        return Timestamp(duration->nanoseconds());
    }

    /// @brief 从 std::chrono::system_clock::time_point 构造时间戳。
    /// @pre duration_cast 的中间运算及纳秒结果须可表示；浮点 Rep 须有限。
    template<typename DurationT>
    static constexpr Timestamp from_time_point(
        std::chrono::time_point<std::chrono::system_clock, DurationT> value) noexcept {
        const auto since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch());
        return Timestamp(since_epoch.count());
    }

    /// @brief 从整数 system_clock time_point 构造，超出 i64 纳秒范围返回 nullopt。
    /// @details Rep 限制、亚纳秒截断与 Duration::checked_from_chrono 一致。
    template<typename DurationT,
             std::enable_if_t<std::is_integral_v<typename DurationT::rep> &&
                                  (sizeof(typename DurationT::rep) <= sizeof(ca::u64)), ca::i32> = 0>
    static constexpr std::optional<Timestamp> checked_from_time_point(
        std::chrono::time_point<std::chrono::system_clock, DurationT> value) noexcept {
        const auto duration = Duration::checked_from_chrono(value.time_since_epoch());
        if (!duration) {
            return std::nullopt;
        }
        return Timestamp(duration->nanoseconds());
    }

    /// @brief Unix epoch 纳秒值。
    constexpr ca::i64 unix_nanoseconds() const noexcept { return nanoseconds_; }
    /// @brief Unix epoch 完整毫秒数，向零截断（兼容既有语义）。
    constexpr ca::i64 unix_milliseconds() const noexcept { return nanoseconds_ / 1000000; }
    /// @brief Unix epoch 完整秒数，向零截断（兼容既有语义）。
    constexpr ca::i64 unix_seconds() const noexcept { return nanoseconds_ / 1000000000; }
    /// @brief Unix epoch 毫秒索引，向负无穷取整。epoch 前不足一毫秒时返回 -1。
    /// @see doc:time-design-negative-epoch
    constexpr ca::i64 unix_milliseconds_floor() const noexcept {
        return floor_units(1000000);
    }
    /// @brief Unix epoch 秒索引，向负无穷取整。epoch 前不足一秒时返回 -1。
    /// @see doc:time-design-negative-epoch
    constexpr ca::i64 unix_seconds_floor() const noexcept {
        return floor_units(1000000000);
    }

    /// @brief 转换为 std::chrono::system_clock::time_point。
    /// @pre duration_cast 的中间运算及目标 system_clock::duration 的计数须可表示。
    constexpr std::chrono::system_clock::time_point to_time_point() const noexcept {
        const auto duration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::nanoseconds(nanoseconds_));
        return std::chrono::system_clock::time_point(duration);
    }

    bool is_past() const noexcept;
    bool is_future() const noexcept;

    /// @brief 加时间间隔，溢出返回 nullopt。
    constexpr std::optional<Timestamp> checked_add(Duration duration) const noexcept {
        const auto sum = Duration::from_nanoseconds(nanoseconds_).checked_add(duration);
        if (!sum) {
            return std::nullopt;
        }
        return Timestamp(sum->nanoseconds());
    }

    /// @brief 减时间间隔，溢出返回 nullopt。
    constexpr std::optional<Timestamp> checked_sub(Duration duration) const noexcept {
        const auto difference = Duration::from_nanoseconds(nanoseconds_).checked_sub(duration);
        if (!difference) {
            return std::nullopt;
        }
        return Timestamp(difference->nanoseconds());
    }

    /// @brief 计算当前时间戳减 other 的间隔，超出 i64 纳秒范围返回 nullopt。
    constexpr std::optional<Duration> checked_duration_since(Timestamp other) const noexcept {
        return Duration::from_nanoseconds(nanoseconds_).checked_sub(Duration::from_nanoseconds(other.nanoseconds_));
    }

    constexpr Timestamp operator+(Duration duration) const noexcept {
        return Timestamp(nanoseconds_ + duration.nanoseconds());
    }

    constexpr Timestamp operator-(Duration duration) const noexcept {
        return Timestamp(nanoseconds_ - duration.nanoseconds());
    }

    constexpr Duration operator-(Timestamp other) const noexcept {
        return Duration::from_nanoseconds(nanoseconds_ - other.nanoseconds_);
    }

    constexpr Timestamp& operator+=(Duration duration) noexcept {
        nanoseconds_ += duration.nanoseconds();
        return *this;
    }
    constexpr Timestamp& operator-=(Duration duration) noexcept {
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

    constexpr ca::i64 floor_units(ca::i64 scale) const noexcept {
        const ca::i64 truncated = nanoseconds_ / scale;
        return truncated - (nanoseconds_ < 0 && nanoseconds_ % scale != 0 ? 1 : 0);
    }

    ca::i64 nanoseconds_{0};
};

}  // namespace ca::time
