///
/// @brief Base64 编解码实现
///

#include "base64.hpp"

namespace ca::crypto {

namespace {
    constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    constexpr char kBase64Pad = '=';

    char base64CharIndex(char c) {
        for (size_t i = 0; i < sizeof(kBase64Chars); ++i)
            if (kBase64Chars[i] == c) return static_cast<char>(i);
        return -1;
    }
}

std::string base64Encode(const char* src, size_t len) {
    std::string ret;
    ret.reserve(len * 4 / 3 + 4);
    for (size_t i = 0; i < len; i += 3) {
        char first  = src[i];
        char second = i + 1 < len ? src[i + 1] : 0;
        char third  = i + 2 < len ? src[i + 2] : 0;

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
        char b1 = base64CharIndex(src[i]);
        char b2 = base64CharIndex(src[i + 1]);
        if (b1 < 0 || b2 < 0) break;

        ret.push_back(static_cast<char>((b1 << 2) | ((b2 >> 4) & 0b11)));

        if (src[i + 2] == kBase64Pad) break;
        char b3 = base64CharIndex(src[i + 2]);
        if (b3 < 0) break;
        ret.push_back(static_cast<char>(((b2 & 0b1111) << 4) | ((b3 >> 2) & 0b1111)));

        if (src[i + 3] == kBase64Pad) break;
        char b4 = base64CharIndex(src[i + 3]);
        if (b4 < 0) break;
        ret.push_back(static_cast<char>(((b3 & 0b11) << 6) | b4));
    }
    return ret;
}

} // namespace ca::crypto
