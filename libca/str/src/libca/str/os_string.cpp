#include "os_string.hpp"

#include <stdexcept>

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
    int len = WideCharToMultiByte(CP_UTF8,
                                  WC_ERR_INVALID_CHARS,
                                  storage_.data(),
                                  static_cast<int>(storage_.size()),
                                  nullptr,
                                  0,
                                  nullptr,
                                  nullptr);
    if (len <= 0)
        throw std::runtime_error("ca::str::OsString::to_utf8_lossy: invalid UTF-16 sequence");
    std::string buffer(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8,
                        WC_ERR_INVALID_CHARS,
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
    // 不用 MB_ERR_INVALID_CHARS：遇到非法字节时 WideCharToMultiByte 把它替换成 U+FFFD。
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
    // 校验失败时 from_data_unchecked 保留原始字节（有损语义：不抛异常，原样保留）。
    return OsString(Utf8String::from_data_unchecked(
        reinterpret_cast<const u8*>(utf8.data()), utf8.size()));
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
