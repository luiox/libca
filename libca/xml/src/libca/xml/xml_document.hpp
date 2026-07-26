#pragma once

/// @file xml_document.hpp
/// @brief XML 文档根容器：XmlDocument。
/// @details XML DOM 节点（XmlNode）存 Utf8StringRef 指向字符串池，但节点本身不拥有池。
///          XmlDocument 持有 `ca::str::Utf8StringArena` + 声明 + prolog/epilog 注释 +
///          root 元素，作为整个 DOM 树的所有权根。document 析构时 arena 释放所有 chunk，
///          所有 Utf8StringRef 失效。
/// @note XmlDocument 禁拷贝（含 arena，不可共享），仅可移动。root 默认是空 Text 节点
///       （解析/构建前的中性态）；一份良构 XML 文档的 root 应是唯一的根元素。
/// @warning XmlNode 与 Utf8StringRef 的生命周期绑定到所属 XmlDocument。document
///          析构/clear/move-assign 后，对原 document 内任何节点的引用都失效。

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "libca/xml/xml_node.hpp"

#include <vector>

namespace ca::xml {

class XmlReader;
class XmlWriter;
class XmlParser;

/// @brief XML 声明 `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>`。
/// @details `present` 为 false 时文档无声明，writer 也不输出。各字段为空视图表示该属性缺省。
struct XmlDeclaration {
    /// 文档是否带 XML 声明。
    bool present = false;
    /// version 值（如 "1.0"）。
    ca::str::Utf8StringRef version;
    /// encoding 值（如 "UTF-8"），可空。
    ca::str::Utf8StringRef encoding;
    /// standalone 值（"yes"/"no"），可空。
    ca::str::Utf8StringRef standalone;
};

/// @brief XML 文档：arena + 声明 + prolog/epilog + root 元素。
class XmlDocument {
public:
    XmlDocument();
    ~XmlDocument();

    XmlDocument(const XmlDocument&) = delete;
    XmlDocument& operator=(const XmlDocument&) = delete;
    XmlDocument(XmlDocument&& other) noexcept;
    XmlDocument& operator=(XmlDocument&& other) noexcept;

    /// @brief root 节点（良构文档为根元素；默认空 Text）。
    XmlNode& root() noexcept;
    /// @brief root 只读访问。
    const XmlNode& root() const noexcept;

    /// @brief XML 声明（可读写）。
    XmlDeclaration& declaration() noexcept;
    const XmlDeclaration& declaration() const noexcept;

    /// @brief root 元素之前的顶层节点（注释）。保序。
    std::vector<XmlNode>& prolog() noexcept;
    const std::vector<XmlNode>& prolog() const noexcept;

    /// @brief root 元素之后的顶层节点（注释）。保序。
    std::vector<XmlNode>& epilog() noexcept;
    const std::vector<XmlNode>& epilog() const noexcept;

    /// @brief 内部 arena（parser/用户需要 intern 字符串时用）。
    ca::str::Utf8StringArena& arena() noexcept;

    /// @brief 清空：释放 arena + root 重置为空 Text + 清声明与 prolog/epilog。所有引用失效。
    void clear() noexcept;

private:
    ca::str::Utf8StringArena arena_;
    XmlDeclaration declaration_;
    std::vector<XmlNode> prolog_;
    std::vector<XmlNode> epilog_;
    XmlNode root_;
};

}  // namespace ca::xml
