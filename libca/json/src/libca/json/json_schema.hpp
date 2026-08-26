/// @file json_schema.hpp
/// @brief JSON Schema 校验器（draft 2020-12 子集）。
/// @details 给定一份 JSON 文档和一份 schema（同样是 JsonValue），校验文档是否符合
///          schema 描述的形状。第一版只支持下游实际需要的关键字子集：
///          type / required / properties / const / enum / items /
///          minimum / maximum / minLength / maxLength。
///          schema 与文档都使用 ca::json::JsonValue（DOM）。错误经
///          ca::Result<std::vector<ValidationError>, SchemaError> 传播，不抛异常。

#pragma once

#include "libca/core/result.hpp"
#include "libca/json/json_value.hpp"
#include "libca/str/utf8_string.hpp"

#include <vector>

namespace ca::json {

/// @brief 单条校验失败记录。
/// @note 字段为 owning（Utf8String），可走出 document 生命周期持久化到上层报告。
struct ValidationError {
    /// 文档内定位（JSON Pointer 风格，如 "/before/classes"）。根文档为 ""。
    ca::str::Utf8String instance_path;
    /// schema 内定位（JSON Pointer 风格，如 "/properties/classes/type"）。根 schema 为 ""。
    ca::str::Utf8String schema_path;
    /// 人读的失败原因，如 "expected integer, got string"。
    ca::str::Utf8String message;
};

/// @brief schema 自身不合法（如 type 值非已知字符串、properties 非对象）。
/// @note 与文档校验失败区分：SchemaError 表示 schema 写错了，而非文档不匹配。
struct SchemaError {
    /// schema 内出错位置（JSON Pointer 风格）。
    ca::str::Utf8String schema_path;
    /// 人读的原因，如 "unknown type value: integre"。
    ca::str::Utf8String message;
};

/// @brief 校验 document 是否符合 schema。
/// @param document 待校验的 JSON DOM 根节点。
/// @param schema schema 根节点（也是普通 JsonValue，符合 draft 2020-12 子集）。
/// @return 成功返回所有 ValidationError（空表示完全匹配）；
///         schema 自身非法时返回 Err(SchemaError)。
/// @note 默认收集全部失败项（不 fail-fast），便于一次性报告 contract 的所有问题。
Result<std::vector<ValidationError>, SchemaError> validate(const JsonValue& document,
                                                           const JsonValue& schema);

}  // namespace ca::json
