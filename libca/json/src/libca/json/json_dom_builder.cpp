#include "libca/json/json_dom_builder.hpp"

#include <utility>

namespace ca::json {

JsonDomBuilder::JsonDomBuilder()
    : stack_(), root_(), has_root_(false), has_error_(false), error_() {}

JsonDomBuilder::~JsonDomBuilder() = default;

void JsonDomBuilder::on_null(const SourceLocation&) {
    emit_value(JsonValue::make_null());
}

void JsonDomBuilder::on_bool(bool v, const SourceLocation&) {
    emit_value(JsonValue::make_bool(v));
}

void JsonDomBuilder::on_int(ca::i64 v, const SourceLocation&) {
    emit_value(JsonValue::make_int(v));
}

void JsonDomBuilder::on_float(ca::f64 v, const SourceLocation&) {
    emit_value(JsonValue::make_float(v));
}

void JsonDomBuilder::on_string(ca::str::Utf8String v, const SourceLocation&) {
    emit_value(JsonValue::make_string(std::move(v)));
}

void JsonDomBuilder::on_array_start(const SourceLocation&) {
    Frame frame;
    frame.is_array = true;
    frame.has_pending_key = false;
    stack_.push_back(std::move(frame));
}

void JsonDomBuilder::on_array_end(const SourceLocation&) {
    // 弹出当前 array 栈帧，作为完成的 value 喂给父
    Frame frame = std::move(stack_.back());
    stack_.pop_back();
    JsonValue arr = JsonValue::make_array();
    for (auto& item : frame.array_items) {
        arr.append(std::move(item));
    }
    emit_value(std::move(arr));
}

void JsonDomBuilder::on_object_start(const SourceLocation&) {
    Frame frame;
    frame.is_array = false;
    frame.has_pending_key = false;
    stack_.push_back(std::move(frame));
}

void JsonDomBuilder::on_object_end(const SourceLocation&) {
    Frame frame = std::move(stack_.back());
    stack_.pop_back();
    JsonValue obj = JsonValue::make_object();
    for (auto& m : frame.object_items) {
        obj.set(std::move(m.first), std::move(m.second));
    }
    emit_value(std::move(obj));
}

void JsonDomBuilder::on_object_key(ca::str::Utf8String key, const SourceLocation&) {
    Frame& top = stack_.back();
    top.pending_key = std::move(key);
    top.has_pending_key = true;
}

void JsonDomBuilder::on_error(const ParseError& err) {
    if (!has_error_) {
        has_error_ = true;
        error_ = ParseError{err.location, err.message.clone()};
    }
}

JsonValue JsonDomBuilder::take_root() noexcept {
    has_root_ = false;
    return std::move(root_);
}

bool JsonDomBuilder::has_error() const noexcept { return has_error_; }

const ParseError& JsonDomBuilder::error() const noexcept { return error_; }

void JsonDomBuilder::emit_value(JsonValue v) {
    if (stack_.empty()) {
        // 根值
        if (!has_root_) {
            root_ = std::move(v);
            has_root_ = true;
        }
        // 多个根值是 parser 的责任，这里忽略
        return;
    }
    Frame& top = stack_.back();
    if (top.is_array) {
        top.array_items.push_back(std::move(v));
    } else {
        // object：必须有 pending key
        if (top.has_pending_key) {
            top.object_items.push_back(JsonValue::ObjectMember{std::move(top.pending_key), std::move(v)});
            top.has_pending_key = false;
        }
        // 没有 pending key 就收到 value，是 parser 的责任（object_key 必须先到）
    }
}

}  // namespace ca::json
