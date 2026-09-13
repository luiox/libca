//
// @brief MurmurHash3 x86 32 位实现（Austin Appleby canonical 算法）
//

#include "murmur3.hpp"

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

namespace {

// 循环左移，shift 取 1..31
inline u32 rotl32(u32 value, i32 shift)
{
    return (value << shift) | (value >> (32 - shift));
}

}  // namespace

u32 murmur3_32(ByteSlice data, u32 seed)
{
    // canonical 乘法常量
    constexpr u32 kC1 = 0xcc9e2d51u;
    constexpr u32 kC2 = 0x1b873593u;

    const u8* bytes = data.data();
    const usize len = data.size();

    u32 h = seed;

    // 主循环：每 4 字节作为一个小端 32 位块混淆进 h
    const usize block_count = len / 4;
    for (usize i = 0; i < block_count; ++i) {
        const usize off = i * 4;
        u32 k = static_cast<u32>(bytes[off]) |
                (static_cast<u32>(bytes[off + 1]) << 8) |
                (static_cast<u32>(bytes[off + 2]) << 16) |
                (static_cast<u32>(bytes[off + 3]) << 24);

        k *= kC1;
        k = rotl32(k, 15);
        k *= kC2;

        h ^= k;
        h = rotl32(h, 13);
        h = h * 5u + 0xe6546b64u;
    }

    // 尾块：剩余 1~3 字节按小端低位对齐拼入 k，再走与主块相同的混淆链
    const usize tail_len = len - block_count * 4;
    if (tail_len != 0) {
        u32 k = 0;
        for (usize i = 0; i < tail_len; ++i)
            k |= static_cast<u32>(bytes[block_count * 4 + i]) << (8 * i);

        k *= kC1;
        k = rotl32(k, 15);
        k *= kC2;
        h ^= k;
    }

    h ^= static_cast<u32>(len);

    // fmix32 finalizer
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;

    return h;
}

}  // namespace ca::crypto
