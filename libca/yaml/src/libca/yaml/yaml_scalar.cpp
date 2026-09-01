#include "libca/yaml/yaml_scalar.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace ca::yaml {

namespace {

bool is_digit(u8 c)
{
    return c >= '0' && c <= '9';
}
bool is_hex_digit(u8 c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool is_octal_digit(u8 c)
{
    return c >= '0' && c <= '7';
}

bool equals(const u8* data, usize len, const char* word)
{
    const usize wlen = std::strlen(word);
    return len == wlen && std::memcmp(data, word, wlen) == 0;
}

// 十进制整数形状：[+-]? digit+（core schema 允许前导零）。
bool is_decimal_int_shape(const u8* data, usize len)
{
    usize i = 0;
    if (i < len && (data[i] == '+' || data[i] == '-'))
        ++i;
    if (i >= len)
        return false;
    for (; i < len; ++i) {
        if (!is_digit(data[i]))
            return false;
    }
    return true;
}

// 浮点形状：[+-]? ( digit+ [. digit*] | . digit+ ) ([eE][+-]?digit+)?，
// 且必须真的出现小数点或指数（否则属整数形状）。
bool is_float_shape(const u8* data, usize len)
{
    usize i       = 0;
    bool  saw_dot = false, saw_exp = false, saw_digit = false;
    if (i < len && (data[i] == '+' || data[i] == '-'))
        ++i;
    // 整数部分或前导小数点
    while (i < len && is_digit(data[i])) {
        ++i;
        saw_digit = true;
    }
    if (i < len && data[i] == '.') {
        saw_dot = true;
        ++i;
        while (i < len && is_digit(data[i])) {
            ++i;
            saw_digit = true;
        }
    }
    if (!saw_digit)
        return false;
    if (i < len && (data[i] == 'e' || data[i] == 'E')) {
        saw_exp = true;
        ++i;
        if (i < len && (data[i] == '+' || data[i] == '-'))
            ++i;
        if (i >= len || !is_digit(data[i]))
            return false;
        while (i < len && is_digit(data[i]))
            ++i;
    }
    return i == len && (saw_dot || saw_exp);
}

}   // namespace

ResolvedScalar resolve_plain_scalar(const u8* data, usize len) noexcept
{
    ResolvedScalar r;

    // Null：空 / ~ / null 家族。
    if (len == 0 || equals(data, len, "~") || equals(data, len, "null") ||
        equals(data, len, "Null") || equals(data, len, "NULL")) {
        r.kind = PlainScalarKind::Null;
        return r;
    }

    // 布尔：仅 true/false 家族（不认 yes/no/on/off）。
    if (equals(data, len, "true") || equals(data, len, "True") || equals(data, len, "TRUE")) {
        r.kind    = PlainScalarKind::Boolean;
        r.boolean = true;
        return r;
    }
    if (equals(data, len, "false") || equals(data, len, "False") || equals(data, len, "FALSE")) {
        r.kind    = PlainScalarKind::Boolean;
        r.boolean = false;
        return r;
    }

    // .inf / .nan 家族（可带符号的 inf）。
    {
        const u8* p    = data;
        usize     n    = len;
        f64       sign = 1.0;
        if (n > 0 && (p[0] == '+' || p[0] == '-')) {
            if (p[0] == '-')
                sign = -1.0;
            ++p;
            --n;
        }
        if (equals(p, n, ".inf") || equals(p, n, ".Inf") || equals(p, n, ".INF")) {
            r.kind     = PlainScalarKind::Float;
            r.floating = sign * std::numeric_limits<f64>::infinity();
            return r;
        }
        if (sign > 0 && (equals(p, n, ".nan") || equals(p, n, ".NaN") || equals(p, n, ".NAN"))) {
            r.kind     = PlainScalarKind::Float;
            r.floating = std::numeric_limits<f64>::quiet_NaN();
            return r;
        }
    }

    // 整数：先形状检查后 strtoll（防 strtoll 吞前导空白/部分消费）。
    const bool hex = len > 2 && data[0] == '0' && data[1] == 'x';
    const bool oct = len > 2 && data[0] == '0' && data[1] == 'o';
    if (hex || oct) {
        bool ok = true;
        for (usize i = 2; i < len; ++i) {
            if (hex ? !is_hex_digit(data[i]) : !is_octal_digit(data[i])) {
                ok = false;
                break;
            }
        }
        if (ok) {
            std::string buf(reinterpret_cast<const char*>(data) + (oct ? 2 : 0),
                            oct ? len - 2 : len);
            char*       end   = nullptr;
            errno             = 0;
            const long long v = std::strtoll(buf.c_str(), &end, hex ? 16 : 8);
            if (errno == ERANGE) {
                r.kind = PlainScalarKind::IntOverflow;
                return r;
            }
            if (end == buf.c_str() + buf.size()) {
                r.kind    = PlainScalarKind::Integer;
                r.integer = static_cast<ca::i64>(v);
                return r;
            }
        }
        r.kind = PlainScalarKind::String;
        return r;
    }
    if (is_decimal_int_shape(data, len)) {
        std::string buf(reinterpret_cast<const char*>(data), len);
        char*       end   = nullptr;
        errno             = 0;
        const long long v = std::strtoll(buf.c_str(), &end, 10);
        if (errno == ERANGE) {
            r.kind = PlainScalarKind::IntOverflow;
            return r;
        }
        if (end == buf.c_str() + buf.size()) {
            r.kind    = PlainScalarKind::Integer;
            r.integer = static_cast<ca::i64>(v);
            return r;
        }
        r.kind = PlainScalarKind::String;
        return r;
    }

    // 浮点。
    if (is_float_shape(data, len)) {
        std::string buf(reinterpret_cast<const char*>(data), len);
        char*       end = nullptr;
        errno           = 0;
        const double v  = std::strtod(buf.c_str(), &end);
        if (errno == ERANGE) {
            r.kind = PlainScalarKind::FloatOverflow;
            return r;
        }
        if (end == buf.c_str() + buf.size()) {
            r.kind     = PlainScalarKind::Float;
            r.floating = static_cast<ca::f64>(v);
            return r;
        }
        r.kind = PlainScalarKind::String;
        return r;
    }

    r.kind = PlainScalarKind::String;
    return r;
}

}   // namespace ca::yaml
