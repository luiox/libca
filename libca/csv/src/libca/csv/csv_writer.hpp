#pragma once

/// @file csv_writer.hpp
/// @brief CSV Writer，把 CsvDocument 序列化为字符串或文件。
/// @details Writer 会按需给字段加引号，并把字段内的引号写成两个连续引号。默认换行
///          使用 `\n`，可通过 CsvWriterOptions::line_ending 改成 `\r\n`。

#include "libca/core/result.hpp"
#include "libca/csv/csv_document.hpp"

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
    /// @brief 将文档序列化为字符串。
    /// @param document CSV 文档。
    /// @param options 写出选项。
    /// @return CSV 文本。
    static std::string write(
        const CsvDocument& document,
        const CsvWriterOptions& options = CsvWriterOptions());

    /// @brief 将文档写入文件。
    /// @param path 文件路径。
    /// @param document CSV 文档。
    /// @param options 写出选项。
    /// @return 写入成功返回 Ok，打开或写入失败返回错误说明。
    static ca::Result<void, std::string> write_file(
        const std::string& path,
        const CsvDocument& document,
        const CsvWriterOptions& options = CsvWriterOptions());
};

}  // namespace ca::csv
