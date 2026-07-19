#pragma once

/// @file csv_writer.hpp
/// @brief CSV Writer，把 CsvDocument 序列化为字符串或文件。
/// @details Writer 会按需给字段加引号，并把字段内的引号写成两个连续引号。默认换行
///          使用 `\n`，可通过 CsvWriterOptions::line_ending 改成 `\r\n`。
///          输出 Utf8String（与 json/ini 一致），写入文件接受 Utf8StringRef 路径。

#include "libca/core/result.hpp"
#include "libca/csv/csv_document.hpp"
#include "libca/str/utf8_string.hpp"

#include <string>

namespace ca::csv {

/// @brief CSV 写出选项。
struct CsvWriterOptions {
    /// @brief 是否写出 CsvDocument 的标题行。
    bool write_header = true;

    /// @brief 字段分隔符，默认逗号。
    char delimiter = ',';

    /// @brief 引号字符，默认双引号。
    char quote = '"';

    /// @brief 行结束符，默认 LF。
    std::string line_ending = "\n";

    /// @brief 是否强制每个字段都加引号。
    bool always_quote = false;
};

/// @brief CSV Writer。
class CsvWriter {
public:
    /// @brief 将文档序列化为 Utf8String。
    static ca::str::Utf8String write(
        const CsvDocument& document,
        const CsvWriterOptions& options = CsvWriterOptions());

    /// @brief 将文档写入文件。
    /// @return 写入成功返回 Ok；打开或写入失败返回错误说明。
    static ca::Result<void, ca::str::Utf8String> write_file(
        const ca::str::Utf8StringRef& path,
        const CsvDocument& document,
        const CsvWriterOptions& options = CsvWriterOptions());
};

}  // namespace ca::csv
