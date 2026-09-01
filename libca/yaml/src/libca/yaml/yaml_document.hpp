#pragma once

/// @file yaml_document.hpp
/// @brief YAML 文档根容器：YamlDocument。
/// @details YAML DOM 节点（YamlValue）存 Utf8StringRef 指向字符串池，但节点本身不拥有池。
///          YamlDocument 持有 `ca::str::Utf8StringArena` + root YamlValue，作为整个 DOM 树
///          的所有权根。document 析构时 arena 释放所有 chunk，所有 Utf8StringRef 失效。
/// @note YamlDocument 禁拷贝（含 arena，不可共享），仅可移动。root 默认 Null（YAML 根
///       可为任意节点：标量/序列/映射，空文档即 Null）。
/// @warning YamlValue 与 Utf8StringRef 的生命周期绑定到所属 YamlDocument。document
///          析构/clear/move-assign 后，对原 document 内任何节点的引用都失效。

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "libca/yaml/yaml_value.hpp"

namespace ca::yaml {

class YamlReader;
class YamlWriter;
class YamlParser;

/// @brief YAML 文档：arena + root。
class YamlDocument
{
public:
    YamlDocument();
    ~YamlDocument();

    YamlDocument(const YamlDocument&)            = delete;
    YamlDocument& operator=(const YamlDocument&) = delete;
    YamlDocument(YamlDocument&& other) noexcept;
    YamlDocument& operator=(YamlDocument&& other) noexcept;

    /// @brief root（默认 Null，空文档即 Null）。
    YamlValue& root() noexcept;
    /// @brief root 只读访问。
    const YamlValue& root() const noexcept;

    /// @brief 内部 arena（parser/用户需要 intern 字符串时用）。
    ca::str::Utf8StringArena& arena() noexcept;

    /// @brief 清空：释放 arena + 把 root 重置为 Null。所有引用失效。
    void clear() noexcept;

private:
    ca::str::Utf8StringArena arena_;
    YamlValue                root_;
};

}   // namespace ca::yaml
