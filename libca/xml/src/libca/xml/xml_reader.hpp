#pragma once

/// @file xml_reader.hpp
/// @brief XML Reader：字符串/文件 → XmlDocument 的静态入口。
/// @details 内部驱动 XmlParser。所有字符串 intern 进返回的 document 的 arena，
///          document 是整个 DOM 的所有权根。

#include "libca/core/result.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/xml/parse_error.hpp"
#include "libca/xml/xml_document.hpp"

namespace ca::xml {

/// @brief XML 读取选项。
struct XmlReaderOptions
{
    /// 丢弃元素之间的纯空白文本节点（默认开）。含非空白字符的文本节点永远完整保留。
    bool trim_whitespace = true;
    /// 最大元素嵌套深度，超出报错。
    /// @note 默认 256（与 XmlParserOptions 一致；理由见其注释——防 1MB 栈先于守卫耗尽）。
    ca::usize max_depth = 256;
};

/// @brief XML Reader，静态入口。
class XmlReader
{
public:
    /// @brief 解析 XML 文本。
    /// @param input 输入视图（零拷贝，使用期内须有效）。
    /// @return 成功返回持有 DOM 的 XmlDocument；失败返回首个 ParseError。
    static ca::Result<XmlDocument, ParseError> read(
        const ca::str::Utf8StringRef& input, const XmlReaderOptions& options = XmlReaderOptions());

    /// @brief 读取并解析 XML 文件。
    /// @param path 文件路径（UTF-8）。
    /// @return 成功返回 XmlDocument；打开失败或解析失败返回 ParseError。
    static ca::Result<XmlDocument, ParseError> read_file(
        const ca::str::Utf8StringRef& path, const XmlReaderOptions& options = XmlReaderOptions());
};

}   // namespace ca::xml
