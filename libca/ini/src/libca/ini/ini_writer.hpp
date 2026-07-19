#pragma once

/// @file ini_writer.hpp
/// @brief INI Writer，把 IniDocument 按行节点顺序写回字符串或文件。
/// @details Writer 只做"按 records_ 顺序拼接 raw + line_ending"，不重新格式化已存在的行
///          （保格式是 INI 模块的核心价值）。IniWriterOptions 控制输出层的换行符等。

#include "libca/core/result.hpp"

#include "libca/ini/ini_document.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::ini {

/// @brief INI 序列化选项。
struct IniWriterOptions {
    /// 输出换行符。空字符串表示沿用各 record 解析时记录的原换行符（默认，最大保真）；
    /// 设为 "\n" 或 "\r\n" 则统一覆盖。
    std::string line_ending;
};

/// @brief INI Writer。
class IniWriter {
public:
    /// @brief 序列化 INI 文档为 Utf8String。
    /// @return INI 文本；未修改行会按原始文本输出。
    static ca::str::Utf8String write(const IniDocument& document,
                                     const IniWriterOptions& options = IniWriterOptions());

    /// @brief 将 INI 文档写入文件。
    /// @return 写入成功返回 Ok；打开或写入失败返回错误说明。
    static ca::Result<void, ca::str::Utf8String> write_file(
        const ca::str::Utf8StringRef& path,
        const IniDocument& document,
        const IniWriterOptions& options = IniWriterOptions());
};

}  // namespace ca::ini
