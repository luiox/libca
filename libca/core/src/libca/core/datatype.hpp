#pragma once
#include <cstdint>

namespace ca::core {
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using f32 = float;
    using f64 = double;
    using usize = size_t;

    inline namespace literals {
        constexpr i32 operator""_i32(unsigned long long v) noexcept { return static_cast<i32>(v); }
        constexpr u64 operator""_u64(unsigned long long v) noexcept { return v; }
    }
}

// Backward compat: put everything directly in namespace ca
namespace ca {
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using f32 = float;
    using f64 = double;
    using usize = size_t;
}
