#pragma once

/// @file csv_reader.hpp
/// @brief CSV Reader，把字符串或文件解析为 CsvDocument。
/// @details Reader 支持 RFC 4180 常见规则：双引号字段、双引号转义、CRLF/LF 换行、
///          以及带换行符的 quoted field。未加引号字段默认原样保留，可通过
///          CsvReaderOptions::trim_unquoted_space 修剪两侧 ASCII 空白。
///          输入接受 Utf8StringRef（零拷贝指向原文本），错误用 ParseError（带行+列）。

#include "libca/core/result.hpp"
#include "libca/csv/csv_document.hpp"
#include "libca/csv/parse_error.hpp"
#include "libca/str/utf8_string.hpp"

#include <string>

namespace ca::csv {

/// @brief CSV 解析选项。
struct CsvReaderOptions {
    /// @brief 第一行是否作为标题行。
    bool first_row_is_header = false;

    /// @brief 字段分隔符，默认逗号。
    char delimiter = ',';

    /// @brief 引号字符，默认双引号。
    char quote = '"';

    /// @brief 是否修剪未加引号字段两侧的 ASCII 空白。
    bool trim_unquoted_space = false;
};

/// @brief CSV Reader。
class CsvReader {
public:
    /// @brief 从字符串解析 CSV。
    /// @param input CSV 文本（须在使用期内有效）。
    /// @param options 解析选项。
    /// @return 成功返回 CsvDocument；格式错误返回 ParseError。
    static ca::Result<CsvDocument, ParseError> read(
        const ca::str::Utf8StringRef& input,
        const CsvReaderOptions& options = CsvReaderOptions());

    /// @brief 从文件解析 CSV。
    /// @param path 文件路径。
    /// @param options 解析选项。
    /// @return 成功返回 CsvDocument；打开失败或格式错误返回 ParseError。
    static ca::Result<CsvDocument, ParseError> read_file(
        const ca::str::Utf8StringRef& path,
        const CsvReaderOptions& options = CsvReaderOptions());
};

}  // namespace ca::csv
