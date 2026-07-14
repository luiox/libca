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

    i32 base64CharIndex(char c) {
        for (usize i = 0; i < sizeof(kBase64Chars) - 1; ++i)
            if (kBase64Chars[i] == c) return static_cast<i32>(i);
        return -1;
    }
}

std::string base64Encode(const char* src, size_t len) {
    std::string ret;
    ret.reserve(len * 4 / 3 + 4);
    for (size_t i = 0; i < len; i += 3) {
        const u8 first = static_cast<u8>(src[i]);
        const u8 second = i + 1 < len ? static_cast<u8>(src[i + 1]) : 0;
        const u8 third = i + 2 < len ? static_cast<u8>(src[i + 2]) : 0;

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

std::vector<char> base64Decode(const std::string& src) {
    std::vector<char> ret;
    ret.reserve(src.length() * 3 / 4 + 4);
    for (size_t i = 0; i < src.length(); i += 4) {
        if (i + 3 >= src.length()) break;
        i32 b1 = base64CharIndex(src[i]);
        i32 b2 = base64CharIndex(src[i + 1]);
        if (b1 < 0 || b2 < 0) break;

        ret.push_back(static_cast<char>((b1 << 2) | ((b2 >> 4) & 0b11)));

        if (src[i + 2] == kBase64Pad) break;
        i32 b3 = base64CharIndex(src[i + 2]);
        if (b3 < 0) break;
        ret.push_back(static_cast<char>(((b2 & 0b1111) << 4) | ((b3 >> 2) & 0b1111)));

        if (src[i + 3] == kBase64Pad) break;
        i32 b4 = base64CharIndex(src[i + 3]);
        if (b4 < 0) break;
        ret.push_back(static_cast<char>(((b3 & 0b11) << 6) | b4));
    }
    return ret;
}

std::string base64_encode(ca::core::ByteSlice data)
{
    return base64Encode(reinterpret_cast<const char*>(data.data()), data.size());
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

        const i32 b0 = base64CharIndex(c0);
        const i32 b1 = base64CharIndex(c1);
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
            const i32 b2 = base64CharIndex(c2);
            if (b2 < 0)
                return Err(CryptoError::INVALID_BASE64);
            if (pad3 && (b2 & 0x03) != 0)
                return Err(CryptoError::INVALID_BASE64);
            output.put_u8(static_cast<u8>(((b1 & 0x0F) << 4) | ((b2 >> 2) & 0x0F)));

            if (!pad3) {
                const i32 b3 = base64CharIndex(c3);
                if (b3 < 0)
                    return Err(CryptoError::INVALID_BASE64);
                output.put_u8(static_cast<u8>(((b2 & 0x03) << 6) | b3));
            }
        }
    }

    return Ok(output.freeze());
}

} // namespace ca::crypto
