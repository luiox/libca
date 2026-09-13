//
// @brief Base32 编解码实现（RFC 4648 standard alphabet）
//

#include "libca/crypto/base32.hpp"

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

namespace {
    constexpr char kBase32Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    constexpr char kBase32Pad = '=';

    // 256 项反查表：字符 → 5bit 值，非法字符为 -1。编译期生成，解码 O(1) 查找。
    struct Base32ReverseTable {
        i8 map[256];
        constexpr Base32ReverseTable() : map{} {
            for (i32 i = 0; i < 256; ++i) map[i] = -1;
            for (i32 i = 0; i < 32; ++i) map[static_cast<u8>(kBase32Chars[i])] = static_cast<i8>(i);
        }
    };
    constexpr Base32ReverseTable kReverse{};

    inline i32 base32_char_index(char c)
    {
        return kReverse.map[static_cast<u8>(c)];
    }
}

std::string base32_encode(ByteSlice data, bool padding)
{
    const u8* src = data.data();
    const usize len = data.size();
    std::string ret;
    ret.reserve((len * 8 + 7) / 5 + 8);

    usize i = 0;
    // 整组：5 字节 → 8 字符
    for (; i + 5 <= len; i += 5) {
        const u64 acc = (static_cast<u64>(src[i]) << 32) |
                        (static_cast<u64>(src[i + 1]) << 24) |
                        (static_cast<u64>(src[i + 2]) << 16) |
                        (static_cast<u64>(src[i + 3]) << 8) |
                        static_cast<u64>(src[i + 4]);
        for (i32 j = 7; j >= 0; --j)
            ret += kBase32Chars[(acc >> (5 * j)) & 0x1F];
    }

    // 尾组：剩余 1~4 字节左对齐到 40 位后按 5bit 切分
    const usize tail_len = len - i;
    if (tail_len != 0) {
        u64 acc = 0;
        for (usize j = 0; j < tail_len; ++j)
            acc = (acc << 8) | static_cast<u64>(src[i + j]);
        // 尾组按字节累加（tail_len*8 位），左移 40-tail_len*8 位对齐到 40 位字段顶端
        acc <<= 8 * (5 - tail_len);

        const usize out_chars = (tail_len * 8 + 4) / 5;
        for (usize c = 0; c < out_chars; ++c)
            ret += kBase32Chars[(acc >> (35 - 5 * c)) & 0x1F];
        if (padding)
            ret.append(8 - out_chars, kBase32Pad);
    }
    return ret;
}

Result<Bytes, CryptoError> base32_decode(const std::string& src)
{
    const usize len = src.size();

    // 定位 padding 起点，data_len 为数据部分长度
    usize data_len = len;
    for (usize i = 0; i < len; ++i) {
        if (src[i] == kBase32Pad) {
            data_len = i;
            break;
        }
    }

    if (data_len != len) {
        // 数据部分之后必须全是 '='
        for (usize i = data_len; i < len; ++i) {
            if (src[i] != kBase32Pad)
                return Err(CryptoError::INVALID_BASE32);
        }
        // 数据长度（模 8）只能是 2/4/5/7，其余长度不存在对应的 canonical padding
        const usize group_rem = data_len % 8;
        if (group_rem == 0 || group_rem == 1 || group_rem == 3 || group_rem == 6)
            return Err(CryptoError::INVALID_BASE32);
        if (8 - group_rem != len - data_len)
            return Err(CryptoError::INVALID_BASE32);
    } else if (len % 8 == 1 || len % 8 == 3 || len % 8 == 6) {
        // 无 padding 时长度余数只能是 0/2/4/5/7
        return Err(CryptoError::INVALID_BASE32);
    }

    BytesMut output = BytesMut::with_capacity(data_len * 5 / 8 + 1);

    usize i = 0;
    // 整组：8 字符 → 5 字节
    for (; i + 8 <= data_len; i += 8) {
        u64 acc = 0;
        for (usize j = 0; j < 8; ++j) {
            const i32 v = base32_char_index(src[i + j]);
            if (v < 0)
                return Err(CryptoError::INVALID_BASE32);
            acc = (acc << 5) | static_cast<u64>(v);
        }
        for (i32 b = 4; b >= 0; --b)
            output.put_u8(static_cast<u8>((acc >> (8 * b)) & 0xFF));
    }

    // 尾组：2/4/5/7 字符 → 1/2/3/4 字节
    if (i < data_len) {
        const usize n = data_len - i;
        u64 acc = 0;
        for (usize j = 0; j < n; ++j) {
            const i32 v = base32_char_index(src[i + j]);
            if (v < 0)
                return Err(CryptoError::INVALID_BASE32);
            acc = (acc << 5) | static_cast<u64>(v);
        }
        // 尾组末尾不足一个字节的剩余数据位必须为 0（canonical 编码，与 base64 解码一致）
        const usize total_bits = n * 5;
        const usize unused_bits = total_bits % 8;
        if (unused_bits != 0 && (acc & ((static_cast<u64>(1) << unused_bits) - 1)) != 0)
            return Err(CryptoError::INVALID_BASE32);

        acc <<= 5 * (8 - n);
        const usize out_bytes = total_bits / 8;
        for (usize b = 0; b < out_bytes; ++b)
            output.put_u8(static_cast<u8>((acc >> (32 - 8 * b)) & 0xFF));
    }

    return Ok(output.freeze());
}

}  // namespace ca::crypto
