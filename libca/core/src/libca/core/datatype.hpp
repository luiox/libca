#pragma once
#include <cstdint>

/// @file datatype.hpp
/// @brief 全库统一的定长/语义类型别名。命名空间 `ca`，是 libca 所有模块的类型入口。
///        禁止裸用 int/long/size_t，统一用这里的别名。

namespace ca {
    /// @name 有符号定长整数
    /// @{
    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    /// @}
    /// @name 无符号定长整数
    /// @{
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    /// @}
    using f32 = float;   ///< 单精度浮点
    using f64 = double;  ///< 双精度浮点
    /// 大小/索引语义类型。当前等同 size_t，仅承诺"大小语义"，不承诺具体位宽。
    using usize = size_t;
    /// 字节语义，当前等同 u8。区别于 std::byte 和"8 位无符号整数"的数值语义。
    using byte = u8;

    /// 字面量后缀：写 `42_i32` / `42_u64` 得到对应定长类型，避免裸字面量的类型歧义。
    inline namespace literals {
        constexpr i32 operator""_i32(unsigned long long v) noexcept { return static_cast<i32>(v); }
        constexpr u64 operator""_u64(unsigned long long v) noexcept { return v; }
    }

}

