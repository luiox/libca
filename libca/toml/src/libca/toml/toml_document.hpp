#pragma once

/// @file toml_document.hpp
/// @brief TOML 文档根容器：TomlDocument。
/// @details TOML DOM 节点（TomlValue）存 Utf8StringRef 指向字符串池，但节点本身不拥有池。
///          TomlDocument 持有 `ca::str::Utf8StringArena` + root TomlValue，作为整个 DOM 树
///          的所有权根。document 析构时 arena 释放所有 chunk，所有 Utf8StringRef 失效。
/// @note TomlDocument 禁拷贝（含 arena，不可共享），仅可移动。
/// @warning TomlValue 与 Utf8StringRef 的生命周期绑定到所属 TomlDocument。document
///          析构/clear/move-assign 后，对原 document 内任何节点的引用都失效。

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "libca/toml/toml_value.hpp"

namespace ca::toml {

class TomlReader;
class TomlWriter;
class TomlParser;

/// @brief TOML 文档：arena + root。
class TomlDocument {
public:
    TomlDocument();
    ~TomlDocument();

    TomlDocument(const TomlDocument&) = delete;
    TomlDocument& operator=(const TomlDocument&) = delete;
    TomlDocument(TomlDocument&& other) noexcept;
    TomlDocument& operator=(TomlDocument&& other) noexcept;

    /// @brief root（默认是 Table，文档为空时即空 Table）。
    TomlValue& root() noexcept;
    /// @brief root 只读访问。
    const TomlValue& root() const noexcept;

    /// @brief 内部 arena（parser/用户需要 intern 字符串时用）。
    ca::str::Utf8StringArena& arena() noexcept;

    /// @brief 清空：释放 arena + 把 root 重置为空 Table。所有引用失效。
    void clear() noexcept;

private:
    ca::str::Utf8StringArena arena_;
    TomlValue root_;
};

}  // namespace ca::toml
