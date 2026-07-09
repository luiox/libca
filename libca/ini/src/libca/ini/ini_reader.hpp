#pragma once

/// @file ini_reader.hpp
/// @brief INI Reader，把字符串或文件解析为保格式 IniDocument。

#include "libca/core/result.hpp"
#include "libca/ini/ini_document.hpp"

#include <string>

namespace ca::ini {

/// @brief INI 解析选项。
struct IniReaderOptions {
    /// @brief 是否允许 section 前出现全局 key/value。
    bool allow_global_keys = true;

    /// @brief 是否把 `#` 识别为注释起始符。
    bool hash_comment = true;

    /// @brief 是否把 `;` 识别为注释起始符。
    bool semicolon_comment = true;
};

/// @brief INI Reader。
class IniReader {
public:
    /// @brief 从字符串解析 INI。
    /// @param text INI 文本。
    /// @param options 解析选项。
    /// @return 成功返回 IniDocument；格式错误返回错误说明。
    static ca::Result<IniDocument, std::string> read(
        const std::string& text,
        const IniReaderOptions& options = IniReaderOptions());

    /// @brief 从文件解析 INI。
    /// @param path 文件路径。
    /// @param options 解析选项。
    /// @return 成功返回 IniDocument；打开失败或格式错误返回错误说明。
    static ca::Result<IniDocument, std::string> read_file(
        const std::string& path,
        const IniReaderOptions& options = IniReaderOptions());
};

}  // namespace ca::ini
