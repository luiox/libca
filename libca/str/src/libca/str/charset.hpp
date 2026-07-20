/// @file charset.hpp
/// @brief Windows 代码页（GBK / 本地 ANSI）与 UTF-8 / wchar 互转工具。
/// @author Canrad
/// @date 2026/07/20
/// @note
/// - 实现基于 Win32 `MultiByteToWideChar` / `WideCharToMultiByte`，**仅在 Windows 上可用**。
/// - 在非 Windows 平台，所有方法返回 `UNIMPLEMENTED` 错误，但头文件仍可包含，
///   便于跨平台代码引用 `CharsetConverter` 类型签名。
/// - 取代了旧 `libca.core/src/base/Charset.{hpp,cpp}` 的混合（libiconv + Win32 + `<codecvt>`）
///   实现，去掉了 iconv 外部依赖、C++17 弃用的 `<codecvt>` 以及固定 255 字节缓冲截断等 bug。

#pragma once

#include "libca/core/status.hpp"

#include <string>
#include <string_view>

namespace ca::str {

/// @brief Windows 代码页字符编码转换工具（纯静态）。
///
/// 提供本地 ANSI 代码页（CP_ACP）和 GBK（CP_936）与 UTF-8 / `std::wstring` 之间的双向转换。
/// 所有方法均为静态函数，类不可实例化。
///
/// @note 仅 Windows 平台实际实现；其它平台返回 `UNIMPLEMENTED`。
/// @note GBK / GB2312 都通过 CP_936 处理：GBK 是 GB2312 的超集，CP_936 在现代 Windows
///       上等价于 GBK，因此使用 GBK 字节序调用方无需区分。
class CharsetConverter {
public:
    CharsetConverter()  = delete;
    ~CharsetConverter() = delete;
    CharsetConverter(const CharsetConverter&)            = delete;
    CharsetConverter& operator=(const CharsetConverter&) = delete;

    /// @brief UTF-8 字符串转 `std::wstring`（Windows 上为 UTF-16LE）。
    /// @param utf8 输入 UTF-8 字节序列，**必须是合法 UTF-8**，否则返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::wstring> utf8_to_wide(std::string_view utf8);

    /// @brief `std::wstring`（Windows 上为 UTF-16LE）转 UTF-8 字符串。
    /// @param wide 输入 wchar 序列，**必须是合法 UTF-16**，否则返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::string> wide_to_utf8(std::wstring_view wide);

    /// @brief 本地 ANSI 代码页（CP_ACP）字符串转 `std::wstring`。
    /// @note 代码页由系统区域决定（中文 Windows 通常是 GBK）。
    static core::StatusResult<std::wstring> local_to_wide(std::string_view local);

    /// @brief 本地 ANSI 代码页（CP_ACP）字符串转 UTF-8。等价于 `local_to_wide` + `wide_to_utf8`。
    static core::StatusResult<std::string> local_to_utf8(std::string_view local);

    /// @brief GBK（CP_936）字符串转 UTF-8。
    static core::StatusResult<std::string> gbk_to_utf8(std::string_view gbk);

    /// @brief UTF-8 字符串转 GBK（CP_936）。
    static core::StatusResult<std::string> utf8_to_gbk(std::string_view utf8);

    /// @brief GBK（CP_936）字符串转 `std::wstring`。
    static core::StatusResult<std::wstring> gbk_to_wide(std::string_view gbk);

    /// @brief `std::wstring` 转 GBK（CP_936）。
    static core::StatusResult<std::string> wide_to_gbk(std::wstring_view wide);
};

}  // namespace ca::str
