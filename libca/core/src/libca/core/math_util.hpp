#pragma once

#include "datatype.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

/// @file math_util.hpp
/// @brief 轻量数学工具，给 runtime intrinsic 映射提供稳定入口。

namespace ca::core {

/// @brief Java/runtime 常用数学 intrinsic 的轻量入口。
///
/// `MathUtil` 只封装稳定、无状态的基础数学操作，不保存运行时上下文，也不处理
/// Java 异常或对象语义。整数除法相关函数按 Java `Math.floorDiv` /
/// `Math.floorMod` 的符号规则实现，便于翻译器直接映射。
class MathUtil
{
public:
    /// @brief 返回两个值中较小的一个。
    template<typename T>
    static constexpr T min(T lhs, T rhs) noexcept
    {
        return rhs < lhs ? rhs : lhs;
    }

    /// @brief 返回两个值中较大的一个。
    template<typename T>
    static constexpr T max(T lhs, T rhs) noexcept
    {
        return lhs < rhs ? rhs : lhs;
    }

    /// @brief 将 value 限制在 [low, high] 闭区间内。
    template<typename T>
    static constexpr T clamp(T value, T low, T high) noexcept
    {
        return value < low ? low : (high < value ? high : value);
    }

    /// @brief 返回算术类型的绝对值；无符号类型原样返回。
    /// @note 有符号整数最小值（如 INT_MIN）的数学绝对值不可表示：本函数按二进制补码
    ///       经无符号取反（well-defined，回绕后仍为最小值本身），**不触发有符号溢出 UB**，
    ///       但返回值仍为负——调用方需自行避免对最小值求绝对值。浮点取反无此问题。
    template<typename T>
    static constexpr T abs(T value) noexcept
    {
        static_assert(std::is_arithmetic<T>::value, "MathUtil::abs requires an arithmetic type");
        if constexpr (std::is_unsigned<T>::value) {
            return value;
        }
        else if constexpr (std::is_integral<T>::value) {
            // 经无符号取反规避 -min() 的有符号溢出 UB。
            using U = std::make_unsigned_t<T>;
            return value < static_cast<T>(0)
                       ? static_cast<T>(static_cast<U>(0) - static_cast<U>(value))
                       : value;
        }
        else {
            return value < static_cast<T>(0) ? -value : value;
        }
    }

    /// @brief 向负无穷取整，包装 `std::floor`。
    static float  floor(float value) noexcept { return std::floor(value); }
    static double floor(double value) noexcept { return std::floor(value); }

    /// @brief 向正无穷取整，包装 `std::ceil`。
    static float  ceil(float value) noexcept { return std::ceil(value); }
    static double ceil(double value) noexcept { return std::ceil(value); }

    /// @brief 四舍五入到浮点整数值，包装 `std::round`。
    static float  round(float value) noexcept { return std::round(value); }
    static double round(double value) noexcept { return std::round(value); }

    /// @brief 按 Java `Math.round(float)` 语义返回 i32。
    /// @note NaN 返回 0，超出 i32 范围时饱和到边界值。
    static ca::i32 round_to_i32(float value) noexcept
    {
        if (std::isnan(value))
            return 0;
        if (value <= static_cast<float>(std::numeric_limits<ca::i32>::min()))
            return std::numeric_limits<ca::i32>::min();
        if (value >= static_cast<float>(std::numeric_limits<ca::i32>::max()))
            return std::numeric_limits<ca::i32>::max();
        return static_cast<ca::i32>(std::floor(value + 0.5f));
    }

    /// @brief 按 Java `Math.round(double)` 语义返回 i64。
    /// @note NaN 返回 0，超出 i64 范围时饱和到边界值。
    static ca::i64 round_to_i64(double value) noexcept
    {
        if (std::isnan(value))
            return 0;
        if (value <= static_cast<double>(std::numeric_limits<ca::i64>::min()))
            return std::numeric_limits<ca::i64>::min();
        if (value >= static_cast<double>(std::numeric_limits<ca::i64>::max()))
            return std::numeric_limits<ca::i64>::max();
        return static_cast<ca::i64>(std::floor(value + 0.5));
    }

    /// @brief 有符号整数向负无穷除法，语义对齐 Java `Math.floorDiv`。
    /// @param lhs 被除数。
    /// @param rhs 除数；调用方必须避免传入 0。
    template<typename T>
    static constexpr T floor_div(T lhs, T rhs) noexcept
    {
        static_assert(std::is_integral<T>::value && std::is_signed<T>::value,
                      "MathUtil::floor_div requires a signed integer type");
        if (lhs == std::numeric_limits<T>::min() && rhs == static_cast<T>(-1))
            return std::numeric_limits<T>::min();

        const T quotient  = static_cast<T>(lhs / rhs);
        const T remainder = static_cast<T>(lhs % rhs);
        if (remainder != static_cast<T>(0) &&
            ((remainder < static_cast<T>(0)) != (rhs < static_cast<T>(0)))) {
            return static_cast<T>(quotient - static_cast<T>(1));
        }
        return quotient;
    }

    /// @brief 有符号整数 floor modulus，语义对齐 Java `Math.floorMod`。
    /// @param lhs 被除数。
    /// @param rhs 除数；调用方必须避免传入 0。
    template<typename T>
    static constexpr T floor_mod(T lhs, T rhs) noexcept
    {
        static_assert(std::is_integral<T>::value && std::is_signed<T>::value,
                      "MathUtil::floor_mod requires a signed integer type");
        if (lhs == std::numeric_limits<T>::min() && rhs == static_cast<T>(-1))
            return static_cast<T>(0);
        return static_cast<T>(lhs - floor_div(lhs, rhs) * rhs);
    }
};

}   // namespace ca::core

namespace ca {
using core::MathUtil;
}
