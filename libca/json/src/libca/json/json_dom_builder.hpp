#pragma once

/// @file json_dom_builder.hpp
/// @brief JsonHandler 的 DOM 装配实现：JsonDomBuilder。
/// @details 接收 JsonParser 的事件，构造出一棵 JsonValue 子树。解析结束后调用 take_root()
///          取走结果。容器嵌套用栈维护，遇到 start/end 配对压栈弹栈。

#include "libca/json/json_handler.hpp"
#include "libca/json/json_value.hpp"

#include <vector>

namespace ca::json {

/// @brief 把 SAX 事件装配为 JsonValue DOM 树的 handler。
class JsonDomBuilder : public JsonHandler {
public:
    JsonDomBuilder();
    ~JsonDomBuilder() override;

    void on_null(const SourceLocation&) override;
    void on_bool(bool v, const SourceLocation&) override;
    void on_int(ca::i64 v, const SourceLocation&) override;
    void on_float(ca::f64 v, const SourceLocation&) override;
    void on_string(ca::str::Utf8StringRef v, const SourceLocation&) override;
    void on_array_start(const SourceLocation&) override;
    void on_array_end(const SourceLocation&) override;
    void on_object_start(const SourceLocation&) override;
    void on_object_end(const SourceLocation&) override;
    void on_object_key(ca::str::Utf8StringRef key, const SourceLocation&) override;
    void on_error(const ParseError& err) override;

    /// @brief 取走根节点。未解析或解析失败时返回 null 节点。
    JsonValue take_root() noexcept;

    /// @brief 解析中遇到的错误（仅 on_error 被调用后有效）。
    bool has_error() const noexcept;
    const ParseError& error() const noexcept;

private:
    // 容器栈帧：array 或 object 的半成品
    struct Frame {
        bool is_array;
        JsonValue::ArrayStorage array_items;
        JsonValue::ObjectStorage object_items;
        ca::str::Utf8StringRef pending_key;  // object 收到 key 后、value 前暂存
        bool has_pending_key;
    };

    std::vector<Frame> stack_;
    JsonValue root_;
    bool has_root_;
    bool has_error_;
    ParseError error_;

    // 把一个完成的 value 喂给当前栈顶（容器）或成为根。
    void emit_value(JsonValue v);
};

}  // namespace ca::json
