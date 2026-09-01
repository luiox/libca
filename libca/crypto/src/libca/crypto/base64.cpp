//
// @brief Base64 编解码实现
//

#include "base64.hpp"

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

namespace {
constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr char kBase64Pad = '=';

// 256 项反查表：字符 → 6bit 值，非法字符为 -1。编译期生成，解码 O(1) 查找。
struct Base64ReverseTable
{
    i8 map[256];
    constexpr Base64ReverseTable()
        : map{}
    {
        for (i32 i = 0; i < 256; ++i)
            map[i] = -1;
        for (i32 i = 0; i < 64; ++i)
            map[static_cast<u8>(kBase64Chars[i])] = static_cast<i8>(i);
    }
};
constexpr Base64ReverseTable kReverse{};

inline i32 base64_char_index(char c)
{
    return kReverse.map[static_cast<u8>(c)];
}
}   // namespace

std::string base64_encode(ca::core::ByteSlice data)
{
    const u8*   src = data.data();
    const usize len = data.size();
    std::string ret;
    ret.reserve(len * 4 / 3 + 4);
    for (usize i = 0; i < len; i += 3) {
        const u8 first  = src[i];
        const u8 second = i + 1 < len ? src[i + 1] : 0;
        const u8 third  = i + 2 < len ? src[i + 2] : 0;

        ret += kBase64Chars[(first >> 2) & 0b111111];
        ret += kBase64Chars[((first & 0b11) << 4) | ((second >> 4) & 0b1111)];
        if (i + 1 >= len) {
            ret += kBase64Pad;
            ret += kBase64Pad;
            break;
        }
        ret += kBase64Chars[((second & 0b1111) << 2) | ((third >> 6) & 0b11)];
        if (i + 2 >= len) {
            ret += kBase64Pad;
            break;
        }
        ret += kBase64Chars[third & 0b111111];
    }
    return ret;
}

Result<Bytes, CryptoError> base64_decode(const std::string& src)
{
    if ((src.size() % 4) != 0)
        return Err(CryptoError::INVALID_BASE64);

    BytesMut output = BytesMut::with_capacity(src.size() * 3 / 4);
    for (usize i = 0; i < src.size(); i += 4) {
        const char c0 = src[i];
        const char c1 = src[i + 1];
        const char c2 = src[i + 2];
        const char c3 = src[i + 3];

        const i32 b0 = base64_char_index(c0);
        const i32 b1 = base64_char_index(c1);
        if (b0 < 0 || b1 < 0)
            return Err(CryptoError::INVALID_BASE64);

        const bool pad2 = c2 == kBase64Pad;
        const bool pad3 = c3 == kBase64Pad;
        if (pad2 && !pad3)
            return Err(CryptoError::INVALID_BASE64);
        if ((pad2 || pad3) && i + 4 != src.size())
            return Err(CryptoError::INVALID_BASE64);
        if (pad2 && (b1 & 0x0F) != 0)
            return Err(CryptoError::INVALID_BASE64);

        output.put_u8(static_cast<u8>((b0 << 2) | ((b1 >> 4) & 0x03)));

        if (!pad2) {
            const i32 b2 = base64_char_index(c2);
            if (b2 < 0)
                return Err(CryptoError::INVALID_BASE64);
            if (pad3 && (b2 & 0x03) != 0)
                return Err(CryptoError::INVALID_BASE64);
            output.put_u8(static_cast<u8>(((b1 & 0x0F) << 4) | ((b2 >> 2) & 0x0F)));

            if (!pad3) {
                const i32 b3 = base64_char_index(c3);
                if (b3 < 0)
                    return Err(CryptoError::INVALID_BASE64);
                output.put_u8(static_cast<u8>(((b2 & 0x03) << 6) | b3));
            }
        }
    }

    return Ok(output.freeze());
}

}   // namespace ca::crypto
