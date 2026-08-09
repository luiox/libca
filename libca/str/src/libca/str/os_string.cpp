#include "os_string.hpp"

#include "utf8_util.hpp"

#include <stdexcept>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ca::str {

// ============================ OsStr ============================

usize OsStr::size() const noexcept
{
#if defined(_WIN32)
    return wide_.size() * sizeof(wchar_t);
#else
    return utf8_.size();
#endif
}

// ============================ OsString ============================

OsString::OsString() noexcept = default;
OsString::~OsString() = default;

OsString::OsString(OsString&& other) noexcept = default;
OsString& OsString::operator=(OsString&& other) noexcept = default;

#if defined(_WIN32)

OsString::OsString(std::wstring wide) noexcept
    : storage_(std::move(wide))
{}

std::wstring_view OsString::as_wide() const noexcept
{
    return storage_;
}

std::wstring OsString::into_wstring() noexcept
{
    return std::move(storage_);
}

OsString OsString::from_wstring(std::wstring wide) noexcept
{
    return OsString(std::move(wide));
}

Utf8String OsString::to_utf8_lossy() const
{
    if (storage_.empty())
        return Utf8String();
    // 不用 WC_ERR_INVALID_CHARS：遇非法 UTF-16（如未配对代理）时由系统替换为 U+FFFD，
    // 对应 "lossy" 语义（绝不抛异常，坏字符替换）。末参数传 nullptr 表示用默认替换字符。
    int len = WideCharToMultiByte(CP_UTF8,
                                  0,
                                  storage_.data(),
                                  static_cast<int>(storage_.size()),
                                  nullptr,
                                  0,
                                  nullptr,
                                  nullptr);
    if (len <= 0)
        return Utf8String();  // 极端失败（如 CP_UTF8 不可用）退化为空串，不抛异常
    std::string buffer(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        storage_.data(),
                        static_cast<int>(storage_.size()),
                        buffer.data(),
                        len,
                        nullptr,
                        nullptr);
    return Utf8String::from_data_unchecked(reinterpret_cast<const u8*>(buffer.data()),
                                           buffer.size());
}

OsString OsString::from_utf8(std::string_view utf8)
{
    if (utf8.empty())
        return OsString();
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                  static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0)
        throw std::runtime_error("ca::str::OsString::from_utf8: invalid UTF-8 sequence");
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                        static_cast<int>(utf8.size()), wide.data(), len);
    return OsString(std::move(wide));
}

OsString OsString::from_utf8_lossy(std::string_view utf8)
{
    // 不用 MB_ERR_INVALID_CHARS：遇到非法字节时 MultiByteToWideChar 把它替换成 U+FFFD。
    if (utf8.empty())
        return OsString();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                  static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0)
        return OsString();
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                        static_cast<int>(utf8.size()), wide.data(), len);
    return OsString(std::move(wide));
}

bool OsString::is_empty() const noexcept
{
    return storage_.empty();
}

OsStr OsString::as_view() const noexcept
{
    return OsStr(storage_);
}

#else  // POSIX

OsString::OsString(Utf8String utf8) noexcept
    : storage_(std::move(utf8))
{}

std::string_view OsString::as_utf8() const noexcept
{
    return storage_;  // Utf8String 有 operator string_view()
}

Utf8String OsString::into_utf8_string() noexcept
{
    return std::move(storage_);
}

OsString OsString::from_utf8_string(Utf8String utf8) noexcept
{
    return OsString(std::move(utf8));
}

Utf8String OsString::to_utf8_lossy() const
{
    // POSIX 原生即 UTF-8，直接克隆返回（move 出来更高效，但保持 const 语义用 clone）。
    return storage_.clone();
}

OsString OsString::from_utf8(std::string_view utf8)
{
    return OsString(Utf8String::from_data(reinterpret_cast<const u8*>(utf8.data()),
                                          utf8.size()));
}

OsString OsString::from_utf8_lossy(std::string_view utf8)
{
    // 快速路径：输入合法时直接克隆（from_data 内部克隆字节），跳过逐码点扫描。
    auto* bytes = reinterpret_cast<const u8*>(utf8.data());
    if (utf8_is_valid(bytes, utf8.size()))
        return OsString(Utf8String::from_data(bytes, utf8.size()));

    // 慢路径：逐码点扫描，非法首字节替换为 U+FFFD（0xEF 0xBF 0xBD）。
    // 替换规则遵循 WHATWG/Unicode：遇到非法首字节（长度为 0 / 截断 / 续字节非法）
    // 用一个 U+FFFD 替代并前进 1 字节，其余合法码点原样保留。
    std::string out;
    out.reserve(utf8.size() + 8);
    usize pos = 0;
    while (pos < utf8.size()) {
        auto len = utf8_code_point_bytes(bytes[pos]);
        bool ok = len > 0 && pos + len <= utf8.size();
        if (ok) {
            for (usize i = 1; i < len; ++i) {
                if ((bytes[pos + i] & 0xC0) != 0x80) {
                    ok = false;
                    break;
                }
            }
        }
        if (ok) {
            out.append(reinterpret_cast<const char*>(bytes + pos), len);
            pos += len;
        }
        else {
            out.append("\xEF\xBF\xBD", 3);  // U+FFFD
            pos += 1;
        }
    }
    return OsString(Utf8String::from_data(reinterpret_cast<const u8*>(out.data()),
                                          out.size()));
}

bool OsString::is_empty() const noexcept
{
    return storage_.is_empty();
}

OsStr OsString::as_view() const noexcept
{
    return OsStr(static_cast<std::string_view>(storage_));
}

#endif  // _WIN32

}  // namespace ca::str
