#pragma once
#include <cstddef>
#include <cstdint>

namespace ca {
    // 整数语义类型
    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    // 浮点数语义类型
    using f32 = float;
    using f64 = double;
    // 明确的大小语义类型
    using usize = size_t;
    // 明确的字节语义，避免与std::byte那种不方便的类型混淆，也避免跟u8这种8位无符号整数混淆语义
    using byte = u8;

    inline namespace literals {
        constexpr i32 operator""_i32(unsigned long long v) noexcept { return static_cast<i32>(v); }
        constexpr u64 operator""_u64(unsigned long long v) noexcept { return v; }
    }

}

