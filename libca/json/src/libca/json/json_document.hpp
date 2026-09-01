#pragma once

/// @file json_document.hpp
/// @brief JSON 文档根容器：JsonDocument。
/// @details JSON DOM 节点（JsonValue）存 Utf8StringRef 指向字符串池，但节点本身不拥有池。
///          JsonDocument 持有 `ca::str::Utf8StringArena` + root JsonValue，作为整个 DOM 树
///          的所有权根。document 析构时 arena 释放所有 chunk，所有 Utf8StringRef 失效。
/// @note JsonDocument 禁拷贝（含 arena，不可共享），仅可移动。
/// @warning JsonValue 与 Utf8StringRef 的生命周期绑定到所属 JsonDocument。document
///          析构/clear/move-assign 后，对原 document 内任何节点的引用都失效。

#include "libca/core/datatype.hpp"
#include "libca/json/json_value.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace ca::json {

class JsonReader;
class JsonWriter;
class JsonParser;
class JsonDomBuilder;

/// @brief JSON 文档：arena + root。
class JsonDocument
{
public:
    JsonDocument();
    ~JsonDocument();

    JsonDocument(const JsonDocument&)            = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;
    JsonDocument(JsonDocument&& other) noexcept;
    JsonDocument& operator=(JsonDocument&& other) noexcept;

    /// @brief root（默认是 null，解析后由 JsonReader 填充）。
    JsonValue& root() noexcept;
    /// @brief root 只读访问。
    const JsonValue& root() const noexcept;

    /// @brief 内部 arena（parser/用户需要 intern 字符串时用）。
    ca::str::Utf8StringArena& arena() noexcept;

    /// @brief 清空：释放 arena + 把 root 重置为 null。所有引用失效。
    void clear() noexcept;

private:
    ca::str::Utf8StringArena arena_;
    JsonValue                root_;
};

}   // namespace ca::json
