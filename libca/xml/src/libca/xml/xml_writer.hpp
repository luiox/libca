#pragma once

/// @file xml_writer.hpp
/// @brief XML 序列化器：XmlWriter。把 XmlDocument 写为 Utf8String。
/// @details 缩进美化输出，但对**混合内容保真**：一个元素只要含 Text/Cdata 子节点，就整体
///          行内输出（不加任何缩进/换行，避免破坏有意义的空白）；子节点全是元素/注释时才
///          分行缩进。文本与属性值按需转义（`& < >` 等）。空元素输出自闭合 `<x/>`。
/// @note 与 reader 默认 trim_whitespace 配合：解析得到的元素树无纯空白文本节点，故写出的
///       缩进空白在重新解析时被 trim 掉，write→read DOM 结构保真。

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/xml/xml_document.hpp"

namespace ca::xml {

/// @brief XML 序列化选项。
struct XmlWriterOptions {
    /// 每层缩进空格数。
    ca::usize indent = 2;
};

/// @brief XML 序列化器。
class XmlWriter {
public:
    /// @brief 把 XmlDocument 序列化为 Utf8String。
    static ca::str::Utf8String write(const XmlDocument& document,
                                     const XmlWriterOptions& options = XmlWriterOptions());

    /// @brief 把 XmlDocument 写入文件。
    /// @return 成功返回 Ok；写失败返回错误说明 Utf8String。
    static ca::Result<void, ca::str::Utf8String> write_file(
        const ca::str::Utf8StringRef& path, const XmlDocument& document,
        const XmlWriterOptions& options = XmlWriterOptions());
};

}  // namespace ca::xml
