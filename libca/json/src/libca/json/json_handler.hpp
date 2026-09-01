#pragma once

/// @file json_handler.hpp
/// @brief JSON SAX 事件接口：JsonHandler。
/// @details JsonHandler 是 SAX 解析的事件接收基类。JsonParser 在解析过程中按顺序回调：
///          - 值：on_null / on_bool / on_int / on_float / on_string
///          - 容器：on_array_start / on_array_end / on_object_start / on_object_end
///          - 对象成员：on_object_key（在每个 value 之前调用一次）
///          - 错误：on_error（纯虚，必须实现）
///          所有事件回调默认空实现，调用方按需 override。解析器发现首个错误时调用 on_error
///          并立即停止。
/// @note 事件流没有返回值（不支持"提前终止解析"），如需提前终止可在 handler 内置标志位
///       并在后续回调中抛出（解析器不捕获），但这不是推荐用法。

#include "libca/core/datatype.hpp"

#include "libca/json/parse_error.hpp"
#include "libca/json/source_location.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::json {

/// @brief SAX 事件接收接口。
class JsonHandler
{
public:
    virtual ~JsonHandler() = default;

    /// 解析到 null 值。
    virtual void on_null(const SourceLocation& /*loc*/) {}
    /// 解析到 bool 值。
    virtual void on_bool(bool /*v*/, const SourceLocation& /*loc*/) {}
    /// 解析到整数值。
    virtual void on_int(ca::i64 /*v*/, const SourceLocation& /*loc*/) {}
    /// 解析到浮点值。
    virtual void on_float(ca::f64 /*v*/, const SourceLocation& /*loc*/) {}
    /// 解析到字符串值（已做转义解码，Utf8StringRef 指向 parser 关联的 arena）。
    virtual void on_string(ca::str::Utf8StringRef /*v*/, const SourceLocation& /*loc*/) {}

    /// 解析到数组开始 `[`。
    virtual void on_array_start(const SourceLocation& /*loc*/) {}
    /// 解析到数组结束 `]`。
    virtual void on_array_end(const SourceLocation& /*loc*/) {}
    /// 解析到对象开始 `{`。
    virtual void on_object_start(const SourceLocation& /*loc*/) {}
    /// 解析到对象结束 `}`。
    virtual void on_object_end(const SourceLocation& /*loc*/) {}
    /// 解析到对象成员的 key（在每个成员的 value 事件之前调用；Utf8StringRef 指向
    /// parser 关联的 arena）。
    virtual void on_object_key(ca::str::Utf8StringRef /*key*/, const SourceLocation& /*loc*/) {}

    /// @brief 解析遇到错误。必须实现。解析器调用后立即停止解析。
    virtual void on_error(const ParseError& err) = 0;
};

}   // namespace ca::json
