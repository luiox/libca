#include "sha3.h"

#include <cstring>

namespace ca::crypto {

namespace {

// ---------------------------------------------------------------------------
// FIPS 202 规范常量表
//
// 以下两组表是 NIST FIPS 202 §3.2.2（ρ 旋转偏移）与 §3.2.5（ι 轮常数）中
// 公开规定的常量，属互操作性所必需的规范数据，不是任何具体实现的表达形式。
// ---------------------------------------------------------------------------

/// ρ 步旋转偏移（位），按 [x][y] 索引，与 m_state 的 x + 5*y 对应。
constexpr int kRhoOffsets[5][5] = {
    {  0, 36,  3, 41, 18},
    {  1, 44, 10, 45,  2},
    { 62,  6, 43, 15, 61},
    { 28, 55, 25, 21, 56},
    { 27, 20, 39,  8, 14},
};

/// π 步坐标置换（FIPS 202 §3.2.3）：B[y][2x+3y mod 5] = rot(A[x][y], r[x][y])。
/// 以一维目标索引表表达，避免运行期模运算。表项按源索引 src = x + 5*y 排列
/// （x 为列、变化最快，y 为行、每 5 项一行）。
constexpr int kPiDestination[25] = {
    // y = 0
     0, 10, 20,  5, 15,
    // y = 1
    16,  1, 11, 21,  6,
    // y = 2
     7, 17,  2, 12, 22,
    // y = 3
    23,  8, 18,  3, 13,
    // y = 4
    14, 24,  9, 19,  4,
};

/// ι 步的 24 轮常数（FIPS 202 §3.2.5）。
constexpr ca::u64 kRoundConstants[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

/// 循环左移；偏移为 0 时原样返回，避免 `x >> 64` 触发未定义行为。
constexpr ca::u64 rotateLeft(ca::u64 value, int bits) {
    return bits == 0 ? value
                     : static_cast<ca::u64>((value << bits) | (value >> (64 - bits)));
}

}  // namespace

// ---------------------------------------------------------------------------
// 构造与重置
// ---------------------------------------------------------------------------

SHA3::SHA3(Bits bits)
    : m_rate(0), m_digest_size(0), m_buffer_size(0), m_bits(bits) {
    // FIPS 202 §6.1：rate = 1600 - 2*capacity，capacity = 2 × digest_bits。
    switch (bits) {
    case Bits224: m_rate = 144; break;
    case Bits256: m_rate = 136; break;
    case Bits384: m_rate = 104; break;
    case Bits512: m_rate = 72;  break;
    }
    m_digest_size = static_cast<ca::usize>(bits) / 8;
    reset();
}

void SHA3::reset() {
    std::memset(m_state, 0, sizeof(m_state));
    std::memset(m_buffer, 0, sizeof(m_buffer));
    m_buffer_size = 0;
}

// ---------------------------------------------------------------------------
// Keccak-f[1600] 置换（FIPS 202 §3.3）
// ---------------------------------------------------------------------------

void SHA3::permute() {
    for (int round = 0; round < 24; ++round) {
        // θ：计算列奇偶校验并扩散
        ca::u64 parity[5];
        for (int x = 0; x < 5; ++x) {
            parity[x] = m_state[x] ^ m_state[x + 5] ^ m_state[x + 10] ^
                        m_state[x + 15] ^ m_state[x + 20];
        }
        for (int x = 0; x < 5; ++x) {
            const ca::u64 delta = parity[(x + 4) % 5] ^ rotateLeft(parity[(x + 1) % 5], 1);
            m_state[x] ^= delta;
            m_state[x + 5] ^= delta;
            m_state[x + 10] ^= delta;
            m_state[x + 15] ^= delta;
            m_state[x + 20] ^= delta;
        }

        // ρ + π：按偏移旋转后搬到 π 目标槽位
        ca::u64 shuffled[25];
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                const int source = x + 5 * y;
                shuffled[kPiDestination[source]] = rotateLeft(m_state[source], kRhoOffsets[x][y]);
            }
        }

        // χ：逐行非线性
        for (int y = 0; y < 5; ++y) {
            const int row = 5 * y;
            for (int x = 0; x < 5; ++x) {
                m_state[row + x] =
                    shuffled[row + x] ^ ((~shuffled[row + (x + 1) % 5]) & shuffled[row + (x + 2) % 5]);
            }
        }

        // ι：注入本轮常数
        m_state[0] ^= kRoundConstants[round];
    }
}

// ---------------------------------------------------------------------------
// 吸收
// ---------------------------------------------------------------------------

void SHA3::absorb_block(const ca::u8* block) {
    // Keccak 的 lane 按小端解释：第 0 字节权重最低。
    // 逐字节拼装而非按 u64 直接解引用，使调用方缓冲可处于任意对齐位置。
    // SHA-3 四个变体的 rate（144/136/104/72）均为 8 的倍数，故按 8 字节整块载入。
    for (ca::usize offset = 0; offset < m_rate; offset += 8) {
        ca::u64 lane = 0;
        for (int byte = 7; byte >= 0; --byte) {
            lane = (lane << 8) | block[offset + static_cast<ca::usize>(byte)];
        }
        m_state[offset / 8] ^= lane;
    }
    permute();
}

void SHA3::add(const void* data, ca::usize num_bytes) {
    if (data == nullptr || num_bytes == 0) return;
    const ca::u8* cursor = static_cast<const ca::u8*>(data);

    // 已有残余缓冲：先补满一块并吸收
    if (m_buffer_size > 0) {
        const ca::usize wanted = m_rate - m_buffer_size;
        const ca::usize taken = num_bytes < wanted ? num_bytes : wanted;
        std::memcpy(m_buffer + m_buffer_size, cursor, taken);
        m_buffer_size += taken;
        cursor += taken;
        num_bytes -= taken;

        if (m_buffer_size == m_rate) {
            absorb_block(m_buffer);
            m_buffer_size = 0;
        }
    }

    // 直接吸收后续的整块数据
    while (num_bytes >= m_rate) {
        absorb_block(cursor);
        cursor += m_rate;
        num_bytes -= m_rate;
    }

    // 剩余零头留待下次或填充时处理
    if (num_bytes > 0) {
        std::memcpy(m_buffer + m_buffer_size, cursor, num_bytes);
        m_buffer_size += num_bytes;
    }
}

// ---------------------------------------------------------------------------
// 填充与取摘要
// ---------------------------------------------------------------------------

void SHA3::squeeze_into(ca::u8* out) const {
    // pad10*1 with domain separator 0x06（FIPS 202 §6.1）：
    // 末块先置 0x06，补零至 rate-1，再置末字节 |= 0x80。
    ca::u8 block[144];
    std::memcpy(block, m_buffer, m_buffer_size);
    std::memset(block + m_buffer_size, 0, m_rate - m_buffer_size);
    block[m_buffer_size] = 0x06;
    block[m_rate - 1] = static_cast<ca::u8>(block[m_rate - 1] | 0x80);

    // 复制状态后在副本上做末次吸收与输出，保持本对象状态不变。
    SHA3 scratch(*this);
    scratch.absorb_block(block);

    // 摘要字节数（28/32/48/64）恒不超过 rate，故单次输出即可。
    for (ca::usize i = 0; i < m_digest_size; ++i) {
        out[i] = static_cast<ca::u8>(scratch.m_state[i / 8] >> (8 * (i % 8)));
    }
}

std::string SHA3::get_hash() {
    static const char kHexDigits[] = "0123456789abcdef";

    ca::u8 digest[64];
    squeeze_into(digest);

    std::string result;
    result.reserve(m_digest_size * 2);
    for (ca::usize i = 0; i < m_digest_size; ++i) {
        result += kHexDigits[digest[i] >> 4];
        result += kHexDigits[digest[i] & 0x0F];
    }
    return result;
}

std::string SHA3::operator()(const void* data, ca::usize num_bytes) {
    reset();
    add(data, num_bytes);
    return get_hash();
}

std::string SHA3::operator()(const std::string& text) {
    reset();
    add(text.data(), text.size());
    return get_hash();
}

}  // namespace ca::crypto