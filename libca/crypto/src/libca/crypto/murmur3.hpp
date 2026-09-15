#pragma once

#include "libca/core/bytes.hpp"

#include <string>

namespace ca::crypto {

/// @brief 计算 MurmurHash3 x86 32 位哈希（Austin Appleby canonical 算法，非加密哈希）。
/// @param data 输入字节视图。
/// @param seed 哈希种子，同一输入不同 seed 产生不同哈希。
/// @return 32 位哈希值。
ca::u32 murmur3_32(ca::core::ByteSlice data, ca::u32 seed = 0);

/// @brief 从原始内存计算 MurmurHash3 32 位哈希。
/// @param data 输入内存指针。
/// @param len 输入字节数。
/// @param seed 哈希种子。
/// @return 32 位哈希值。
inline ca::u32 murmur3_32(const void* data, ca::usize len, ca::u32 seed = 0)
{
    return murmur3_32(ca::core::ByteSlice(static_cast<const ca::u8*>(data), len), seed);
}

/// @brief 计算 std::string 内容（不含结尾 '\0'）的 MurmurHash3 32 位哈希。
/// @param text 输入字符串。
/// @param seed 哈希种子。
/// @return 32 位哈希值。
inline ca::u32 murmur3_32(const std::string& text, ca::u32 seed = 0)
{
    return murmur3_32(text.data(), text.size(), seed);
}

}  // namespace ca::crypto
