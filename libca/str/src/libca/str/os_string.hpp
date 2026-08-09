/// @file os_string.hpp
/// @brief OsString / OsStr —— 对齐平台原生编码的字符串载体。
///        Windows 内部存 UTF-16（std::wstring），POSIX 内部存 UTF-8（Utf8String）。
///        与 Utf8String 之间不做隐式转换，必须显式调用，提醒编码开销。
/// @note OsString 为 move-only（POSIX 持有 move-only 的 Utf8String），与 Rust OsString
///       语义一致。OsStr 是只读视图，仅在同一平台语义下使用。

#pragma once

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace ca::str {

class OsString;

/// @brief 平台原生字符串的只读视图。
///
/// 不拥有数据，调用方须保证源 OsString 在视图使用期内有效。提供平台原生编码的只读访问，
/// 不做隐式 UTF-8 转换。
class OsStr
{
public:
    OsStr() noexcept = default;

#if defined(_WIN32)
    /// @brief 从宽字符视图构造（Windows）。
    explicit OsStr(std::wstring_view wide) noexcept
        : wide_(wide)
    {}

    /// @brief 返回 UTF-16 视图（Windows 原生编码）。
    std::wstring_view as_wide() const noexcept { return wide_; }
#else
    /// @brief 从 UTF-8 视图构造（POSIX）。
    explicit OsStr(std::string_view utf8) noexcept
        : utf8_(utf8)
    {}

    /// @brief 返回 UTF-8 视图（POSIX 原生编码）。
    std::string_view as_utf8() const noexcept { return utf8_; }
#endif

    /// @brief 字节长度（Windows = UTF-16 单元数 * 2 近似；POSIX = UTF-8 字节数）。
    usize size() const noexcept;

    /// @brief 是否为空。
    bool is_empty() const noexcept { return size() == 0; }

private:
#if defined(_WIN32)
    std::wstring_view wide_;
#else
    std::string_view utf8_;
#endif

    friend class OsString;
};

/// @brief 拥有所有权的平台原生字符串。
///
/// Windows 内部以 UTF-16 存储，POSIX 内部以 UTF-8 存储。调用 Win32 `*W` API 或与
/// fs/process 交互时，用它避免分散的编码转换。与 Utf8String 之间的转换均为显式且
/// 名字标注编码开销（`to_utf8_lossy` / `from_utf8`）。
class OsString
{
public:
    OsString() noexcept;
    ~OsString();

    OsString(const OsString&)            = delete;
    OsString& operator=(const OsString&) = delete;
    OsString(OsString&& other) noexcept;
    OsString& operator=(OsString&& other) noexcept;

    // ── 平台原生互操作（零拷贝） ──

#if defined(_WIN32)
    /// @brief 从 std::wstring 构造（Windows，零拷贝 move）。
    explicit OsString(std::wstring wide) noexcept;
    /// @brief 返回内部 UTF-16 视图（零拷贝）。
    std::wstring_view as_wide() const noexcept;
    /// @brief move 出内部 wstring（零拷贝）。
    std::wstring into_wstring() noexcept;
    /// @brief 从 wstring 构造 OsString（零拷贝）。
    static OsString from_wstring(std::wstring wide) noexcept;
#else
    /// @brief 从 Utf8String 构造（POSIX，零拷贝 move）。
    explicit OsString(Utf8String utf8) noexcept;
    /// @brief 返回内部 UTF-8 视图（零拷贝）。
    std::string_view as_utf8() const noexcept;
    /// @brief move 出内部 Utf8String（零拷贝）。
    Utf8String into_utf8_string() noexcept;
    /// @brief 从 Utf8String 构造 OsString（零拷贝）。
    static OsString from_utf8_string(Utf8String utf8) noexcept;
#endif

    // ── 跨平台显式编码转换（有开销） ──

    /// @brief 转换为 UTF-8 字符串（Windows 上做 UTF-16→UTF-8 编码转换）。
    /// @note 遇到非法序列（如未配对代理）用替换字符 U+FFFD 替代，不抛异常。
    Utf8String to_utf8_lossy() const;

    /// @brief 从 UTF-8 字节构造（Windows 上做 UTF-8→UTF-16 编码转换）。
    /// @throws std::runtime_error 输入非法 UTF-8 时。
    static OsString from_utf8(std::string_view utf8);

    /// @brief 从 UTF-8 字节构造，非法序列用替换字符（U+FFFD）替代，不抛异常。
    static OsString from_utf8_lossy(std::string_view utf8);

    // ── 查询 ──

    /// @brief 是否为空。
    bool is_empty() const noexcept;

    /// @brief 返回只读视图（零拷贝，不持有所有权）。
    OsStr as_view() const noexcept;

private:
#if defined(_WIN32)
    std::wstring storage_;
#else
    Utf8String storage_;
#endif
};

}  // namespace ca::str
