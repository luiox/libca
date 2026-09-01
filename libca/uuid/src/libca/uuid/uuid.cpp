#include "libca/uuid/uuid.hpp"

#include <stdexcept>

#include "libca/core/datatype.hpp"
#include "libca/crypto/random.hpp"

namespace ca::uuid {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

// RFC 4122 v4 字节布局（16 字节）：
//   byte[6] 高 4 位 = 0b0100 (version 4)
//   byte[8] 高 2 位 = 0b10   (variant 1, 即 8/9/a/b)
// 连字符位置：8-4-4-4-12 -> 字节序 [0..3] [4..5] [6..7] [8..9] [10..15]

void stamp_v4(unsigned char* bytes)
{
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
}

std::string format_uuid(const unsigned char* bytes)
{
    std::string out;
    out.reserve(36);
    auto put = [&](usize begin, usize end) {
        for (usize i = begin; i < end; ++i) {
            out.push_back(kHexDigits[bytes[i] >> 4]);
            out.push_back(kHexDigits[bytes[i] & 0x0F]);
        }
    };
    put(0, 4);
    out.push_back('-');
    put(4, 6);
    out.push_back('-');
    put(6, 8);
    out.push_back('-');
    put(8, 10);
    out.push_back('-');
    put(10, 16);
    return out;
}

}   // namespace

std::string v4()
{
    unsigned char bytes[16]{};
    auto          result = ca::crypto::secure_random_bytes(sizeof(bytes));
    if (result.is_err())
        throw std::runtime_error("ca::uuid::v4: secure random source failed");

    auto ptr = result.unwrap();
    if (ptr.len() < sizeof(bytes))
        throw std::runtime_error("ca::uuid::v4: insufficient random bytes");

    for (usize i = 0; i < sizeof(bytes); ++i)
        bytes[i] = ptr.as_ptr()[i];

    stamp_v4(bytes);
    return format_uuid(bytes);
}

std::string nil()
{
    return "00000000-0000-0000-0000-000000000000";
}

bool is_valid(std::string_view s, bool check_variant_version) noexcept
{
    if (s.size() != 36)
        return false;

    static constexpr usize kHyphens[4] = {8, 13, 18, 23};
    for (usize pos : kHyphens) {
        if (s[pos] != '-')
            return false;
    }

    auto is_hex = [](char c) noexcept -> bool {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };

    for (usize i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            continue;
        if (!is_hex(s[i]))
            return false;
    }

    if (check_variant_version) {
        char c = s[14];   // 第 3 段首位 -> version
        if (c != '4')
            return false;
        char v = s[19];   // 第 4 段首位 -> variant
        if (v != '8' && v != '9' && v != 'a' && v != 'A' && v != 'b' && v != 'B')
            return false;
    }

    return true;
}

}   // namespace ca::uuid
