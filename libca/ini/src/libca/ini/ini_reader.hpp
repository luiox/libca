#pragma once

/// @file ini_reader.hpp
/// @brief INI Reader，把字符串或文件解析为保格式 IniDocument。
/// @details 输入用 Utf8StringRef（零拷贝指向原文本），错误用 ParseError（带行+列）。

#include "libca/core/result.hpp"

#include "libca/ini/ini_document.hpp"
#include "libca/ini/parse_error.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::ini {

/// @brief 遇到重复 section / key 时的处理策略。
enum class DuplicatePolicy {
    KeepLast,  ///< 保留最后出现的（向后兼容默认行为）。
    Error      ///< 报错。
};

/// @brief INI 解析选项。
struct IniReaderOptions {
    /// 是否允许 section 前出现全局 key/value。
    bool allow_global_keys = true;

    /// 是否把 `#` 识别为注释起始符。
    bool hash_comment = true;

    /// 是否把 `;` 识别为注释起始符。
    bool semicolon_comment = true;

    /// 遇到重复 section 时的策略（默认保留最后一个）。
    DuplicatePolicy on_duplicate_section = DuplicatePolicy::KeepLast;

    /// 遇到同一 section 内重复 key 时的策略（默认保留最后一个）。
    DuplicatePolicy on_duplicate_key = DuplicatePolicy::KeepLast;

    /// 行内注释识别是否要求注释符前有空白。true（默认）= 紧贴 value 的 #/; 不算注释；
    /// false = 任何位置的 #/; 都算注释（更宽松）。
    bool inline_comment_strict_whitespace = true;
};

/// @brief INI Reader。
class IniReader {
public:
    /// @brief 从字符串解析 INI。
    /// @param input INI 文本（须在使用期内有效）。
    /// @param options 解析选项。
    /// @return 成功返回 IniDocument；格式错误返回 ParseError。
    static ca::Result<IniDocument, ParseError> read(
        const ca::str::Utf8StringRef& input,
        const IniReaderOptions& options = IniReaderOptions());

    /// @brief 从文件解析 INI。
    /// @param path 文件路径。
    /// @return 成功返回 IniDocument；打开失败或格式错误返回 ParseError。
    static ca::Result<IniDocument, ParseError> read_file(
        const ca::str::Utf8StringRef& path,
        const IniReaderOptions& options = IniReaderOptions());
};

}  // namespace ca::ini
