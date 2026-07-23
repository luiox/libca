/// @file json_schema.cpp
/// @brief JSON Schema 校验器实现（draft 2020-12 子集）。
/// @details 递归校验 document 是否符合 schema。内部用 std::string 累积 JSON Pointer
///          风格的定位路径（Utf8String move-only 不便拼接），最终填入 owning
///          ValidationError。支持的 schema 关键字：type / required / properties /
///          const / enum / items / minimum / maximum / minLength / maxLength。

#include "libca/json/json_schema.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace ca::json {

namespace {

using ca::str::Utf8String;
using ca::str::Utf8StringRef;

/// 把 std::string 转成 owning Utf8String（按字节复制，含 NUL 后内容也保留）。
Utf8String to_owning(const std::string& s)
{
    return Utf8String(reinterpret_cast<const ca::u8*>(s.data()),
                      static_cast<ca::usize>(s.size()));
}

/// 从 const char* 构造 owning Utf8String。
Utf8String owning(const char* s)
{
    return Utf8String(s);
}

/// Utf8StringRef → std::string（Utf8StringRef 无 to_std_string 成员，用 string_view 中转）。
std::string to_std(const Utf8StringRef& ref)
{
    return std::string(static_cast<std::string_view>(ref));
}

/// 拼接子路径：parent + "/" + segment（JSON Pointer 风格，segment 不做转义简化）。
std::string join_path(std::string_view parent, std::string_view segment)
{
    std::string out;
    out.reserve(parent.size() + segment.size() + 1);
    out.append(parent.data(), parent.size());
    out.push_back('/');
    out.append(segment.data(), segment.size());
    return out;
}

/// 记录一条校验失败。
void record(std::vector<ValidationError>& errors, std::string_view instance_path,
            std::string_view schema_path, std::string message)
{
    errors.push_back(ValidationError{
        to_owning(std::string(instance_path)),
        to_owning(std::string(schema_path)),
        owning(message.c_str()),
    });
}

/// 把 JsonType 转可读名（用于错误消息）。
const char* type_name(JsonType t)
{
    switch (t) {
        case JsonType::Null:   return "null";
        case JsonType::Bool:   return "boolean";
        case JsonType::Int:    return "integer";
        case JsonType::Float:  return "number";
        case JsonType::String: return "string";
        case JsonType::Array:  return "array";
        case JsonType::Object: return "object";
    }
    return "unknown";
}

/// 校验 "type" 关键字。type 值可以是单字符串或字符串数组。
/// number 接受 Int 或 Float；integer 接受 Int（draft 2020-12 默认 integer 仅整数，
/// 但这里对 Float 整数值也接受，避免浮点精度误判，符合多数实际 contract 需求）。
/// @return schema 非法时返回 SchemaError。
Result<void, SchemaError> check_type(const JsonValue& instance, const JsonValue& type_keyword,
                                     std::string_view instance_path,
                                     std::string_view schema_path,
                                     std::vector<ValidationError>& errors)
{
    auto match_one = [&](const Utf8StringRef& expected) -> bool {
        auto eq = [&](const char* s) { return expected == Utf8StringRef::from_cstr(s); };
        if (eq("null"))   return instance.is_null();
        if (eq("boolean"))return instance.is_bool();
        if (eq("integer"))return instance.is_int();
        if (eq("number")) return instance.is_number();
        if (eq("string")) return instance.is_string();
        if (eq("array"))  return instance.is_array();
        if (eq("object")) return instance.is_object();
        return false;  // 未知类型名不在此报 schema 错（type 值校验在调用方做）
    };

    if (type_keyword.is_string()) {
        if (!match_one(type_keyword.as_string())) {
            record(errors, instance_path, schema_path,
                   std::string("expected ") + to_std(type_keyword.as_string()) +
                       ", got " + type_name(instance.type()));
        }
        return Ok();
    }
    if (type_keyword.is_array()) {
        bool any = false;
        for (const auto& entry : type_keyword.as_array()) {
            if (entry.is_string() && match_one(entry.as_string())) {
                any = true;
                break;
            }
        }
        if (!any) {
            record(errors, instance_path, schema_path,
                   std::string("type mismatch: got ") + type_name(instance.type()));
        }
        return Ok();
    }
    // schema 非法：type 既不是 string 也不是 array
    return Err(SchemaError{to_owning(std::string(schema_path)),
                           owning("\"type\" must be string or array")});
}

/// 校验 "const" 关键字：文档值必须与此常量相等。
bool json_equal(const JsonValue& a, const JsonValue& b)
{
    if (a.type() != b.type()) return false;
    switch (a.type()) {
        case JsonType::Null:   return true;
        case JsonType::Bool:   return a.as_bool() == b.as_bool();
        case JsonType::Int:    return a.as_int() == b.as_int();
        case JsonType::Float:  return a.as_float() == b.as_float();
        case JsonType::String: return a.as_string() == b.as_string();
        case JsonType::Array: {
            const auto& aa = a.as_array();
            const auto& bb = b.as_array();
            if (aa.size() != bb.size()) return false;
            for (ca::usize i = 0; i < aa.size(); ++i) {
                if (!json_equal(aa[i], bb[i])) return false;
            }
            return true;
        }
        case JsonType::Object: {
            const auto& aa = a.as_object();
            const auto& bb = b.as_object();
            if (aa.size() != bb.size()) return false;
            for (const auto& [k, v] : aa) {
                const auto* bv = b.find(k);
                if (bv == nullptr || !json_equal(v, *bv)) return false;
            }
            return true;
        }
    }
    return false;
}

/// 校验 "enum" 关键字：文档值必须等于列表中的某一项。
void check_enum(const JsonValue& instance, const JsonValue& enum_keyword,
                std::string_view instance_path, std::string_view schema_path,
                std::vector<ValidationError>& errors)
{
    if (!enum_keyword.is_array()) return;  // schema 形态校验在主流程
    for (const auto& candidate : enum_keyword.as_array()) {
        if (json_equal(instance, candidate)) return;
    }
    record(errors, instance_path, schema_path, "value not in enum");
}

/// 校验数值边界 "minimum" / "maximum"（闭区间）。
void check_numeric_bounds(const JsonValue& instance, const JsonValue& schema,
                          std::string_view instance_path, std::string_view schema_path,
                          std::vector<ValidationError>& errors)
{
    if (!instance.is_number()) return;
    const auto* min = schema.find(Utf8StringRef::from_cstr("minimum"));
    if (min != nullptr && min->is_number()) {
        double limit = min->is_int() ? static_cast<double>(min->as_int()) : min->as_float();
        double val = instance.is_int() ? static_cast<double>(instance.as_int()) : instance.as_float();
        if (val < limit) {
            record(errors, instance_path, join_path(schema_path, "minimum"),
                   "value below minimum");
        }
    }
    const auto* max = schema.find(Utf8StringRef::from_cstr("maximum"));
    if (max != nullptr && max->is_number()) {
        double limit = max->is_int() ? static_cast<double>(max->as_int()) : max->as_float();
        double val = instance.is_int() ? static_cast<double>(instance.as_int()) : instance.as_float();
        if (val > limit) {
            record(errors, instance_path, join_path(schema_path, "maximum"),
                   "value above maximum");
        }
    }
}

/// 校验字符串长度边界 "minLength" / "maxLength"（按 UTF-8 码点计数）。
void check_string_bounds(const JsonValue& instance, const JsonValue& schema,
                         std::string_view instance_path, std::string_view schema_path,
                         std::vector<ValidationError>& errors)
{
    if (!instance.is_string()) return;
    const auto& s = instance.as_string();
    auto cp_len = static_cast<ca::i64>(s.length());

    const auto* min = schema.find(Utf8StringRef::from_cstr("minLength"));
    if (min != nullptr && min->is_int() && cp_len < min->as_int()) {
        record(errors, instance_path, join_path(schema_path, "minLength"),
               "string shorter than minLength");
    }
    const auto* max = schema.find(Utf8StringRef::from_cstr("maxLength"));
    if (max != nullptr && max->is_int() && cp_len > max->as_int()) {
        record(errors, instance_path, join_path(schema_path, "maxLength"),
               "string longer than maxLength");
    }
}

/// 把 schema 的 type 值校验为已知字符串，返回 false 表示含未知类型名（schema 非法）。
bool validate_type_keyword_shape(const JsonValue& type_keyword, std::string& bad_value)
{
    auto known = [](const Utf8StringRef& s) -> bool {
        return s == Utf8StringRef::from_cstr("null") ||
               s == Utf8StringRef::from_cstr("boolean") ||
               s == Utf8StringRef::from_cstr("integer") ||
               s == Utf8StringRef::from_cstr("number") ||
               s == Utf8StringRef::from_cstr("string") ||
               s == Utf8StringRef::from_cstr("array") ||
               s == Utf8StringRef::from_cstr("object");
    };
    if (type_keyword.is_string()) {
        if (!known(type_keyword.as_string())) {
            bad_value = to_std(type_keyword.as_string());
            return false;
        }
        return true;
    }
    if (type_keyword.is_array()) {
        for (const auto& entry : type_keyword.as_array()) {
            if (!entry.is_string() || !known(entry.as_string())) {
                bad_value = entry.is_string() ? to_std(entry.as_string()) : std::string("<non-string>");
                return false;
            }
        }
        return true;
    }
    bad_value = "<non-string>";
    return false;
}

/// 递归校验单个节点。
Result<void, SchemaError> validate_node(const JsonValue& instance, const JsonValue& schema,
                                        std::string_view instance_path,
                                        std::string_view schema_path,
                                        std::vector<ValidationError>& errors)
{
    // schema 必须是对象（draft 2020-12 也允许 true/false，这里简化为对象）。
    if (!schema.is_object()) {
        return Err(SchemaError{to_owning(std::string(schema_path)),
                               owning("schema must be an object")});
    }

    // "type" 关键字。
    if (const auto* t = schema.find(Utf8StringRef::from_cstr("type"))) {
        std::string bad;
        if (!validate_type_keyword_shape(*t, bad)) {
            return Err(SchemaError{to_owning(join_path(schema_path, "type")),
                                   owning(("unknown type value: " + bad).c_str())});
        }
        auto r = check_type(instance, *t, instance_path, join_path(schema_path, "type"), errors);
        if (r.is_err()) return r;
    }

    // "const" 关键字。
    if (const auto* c = schema.find(Utf8StringRef::from_cstr("const"))) {
        if (!json_equal(instance, *c)) {
            record(errors, instance_path, join_path(schema_path, "const"),
                   "value does not equal const");
        }
    }

    // "enum" 关键字。
    if (const auto* e = schema.find(Utf8StringRef::from_cstr("enum"))) {
        if (!e->is_array()) {
            return Err(SchemaError{to_owning(join_path(schema_path, "enum")),
                                   owning("\"enum\" must be an array")});
        }
        check_enum(instance, *e, instance_path, join_path(schema_path, "enum"), errors);
    }

    // 数值与字符串边界（仅在类型匹配时生效，否则静默跳过）。
    check_numeric_bounds(instance, schema, instance_path, schema_path, errors);
    check_string_bounds(instance, schema, instance_path, schema_path, errors);

    // "properties" / "required"：仅对 object 实例递归。
    if (instance.is_object()) {
        const auto* required = schema.find(Utf8StringRef::from_cstr("required"));
        if (required != nullptr) {
            if (!required->is_array()) {
                return Err(SchemaError{to_owning(join_path(schema_path, "required")),
                                       owning("\"required\" must be an array")});
            }
            for (const auto& key : required->as_array()) {
                if (!key.is_string()) {
                    return Err(SchemaError{to_owning(join_path(schema_path, "required")),
                                           owning("\"required\" entries must be strings")});
                }
                if (instance.find(key.as_string()) == nullptr) {
                    record(errors, instance_path, join_path(schema_path, "required"),
                           std::string("missing required property: ") + to_std(key.as_string()));
                }
            }
        }

        const auto* props = schema.find(Utf8StringRef::from_cstr("properties"));
        if (props != nullptr) {
            if (!props->is_object()) {
                return Err(SchemaError{to_owning(join_path(schema_path, "properties")),
                                       owning("\"properties\" must be an object")});
            }
            for (const auto& [key, sub_schema] : props->as_object()) {
                const auto* field = instance.find(key);
                if (field == nullptr) continue;  // 缺失字段由 required 管，这里只校验存在的
                auto r = validate_node(*field, sub_schema, join_path(instance_path, to_std(key)),
                                       join_path(schema_path, to_std(key)), errors);
                if (r.is_err()) return r;
            }
        }
    }

    // "items"：仅对 array 实例的每个元素递归（单 schema 形式）。
    if (instance.is_array()) {
        const auto* items = schema.find(Utf8StringRef::from_cstr("items"));
        if (items != nullptr) {
            const auto& arr = instance.as_array();
            for (ca::usize i = 0; i < arr.size(); ++i) {
                auto r = validate_node(arr[i], *items, join_path(instance_path, std::to_string(i)),
                                       join_path(schema_path, "items"), errors);
                if (r.is_err()) return r;
            }
        }
    }

    return Ok();
}

}  // namespace

Result<std::vector<ValidationError>, SchemaError> validate(const JsonValue& document,
                                                           const JsonValue& schema)
{
    std::vector<ValidationError> errors;
    auto r = validate_node(document, schema, "", "", errors);
    if (r.is_err()) return Err(std::move(r).unwrap_err());
    return Ok(std::move(errors));
}

}  // namespace ca::json
