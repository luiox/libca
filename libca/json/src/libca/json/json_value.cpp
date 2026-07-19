#include "libca/json/json_value.hpp"

#include <cassert>
#include <utility>

namespace ca::json {

namespace {
using ObjectMember = JsonValue::ObjectMember;
}  // namespace

// ============================================================================
// 构造 / 析构 / 移动
// ============================================================================

JsonValue::JsonValue() noexcept : type_(JsonType::Null), data_(std::monostate{}) {}

JsonValue::JsonValue(JsonValue&& other) noexcept
    : type_(other.type_), data_(std::move(other.data_)) {
    other.type_ = JsonType::Null;
    other.data_ = std::monostate{};
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
    if (this != &other) {
        type_ = other.type_;
        data_ = std::move(other.data_);
        other.type_ = JsonType::Null;
        other.data_ = std::monostate{};
    }
    return *this;
}

JsonValue::~JsonValue() = default;

JsonValue JsonValue::clone() const {
    JsonValue copy;
    copy.type_ = type_;
    switch (type_) {
        case JsonType::Null:    copy.data_ = std::monostate{}; break;
        case JsonType::Bool:    copy.data_ = std::get<bool>(data_); break;
        case JsonType::Int:     copy.data_ = std::get<i64>(data_); break;
        case JsonType::Float:   copy.data_ = std::get<f64>(data_); break;
        case JsonType::String:  copy.data_ = std::get<ca::str::Utf8StringRef>(data_); break;
        case JsonType::Array: {
            ArrayStorage dup;
            const auto& src = std::get<ArrayStorage>(data_);
            dup.reserve(src.size());
            for (const auto& v : src) dup.push_back(v.clone());
            copy.data_ = std::move(dup);
            break;
        }
        case JsonType::Object: {
            ObjectStorage dup;
            const auto& src = std::get<ObjectStorage>(data_);
            dup.reserve(src.size());
            for (const auto& m : src) {
                dup.push_back(ObjectMember{m.first, m.second.clone()});
            }
            copy.data_ = std::move(dup);
            break;
        }
    }
    return copy;
}

// ============================================================================
// 工厂
// ============================================================================

JsonValue JsonValue::make_null() noexcept { return JsonValue{}; }

JsonValue JsonValue::make_bool(bool v) noexcept {
    JsonValue j;
    j.type_ = JsonType::Bool;
    j.data_ = v;
    return j;
}

JsonValue JsonValue::make_int(i64 v) noexcept {
    JsonValue j;
    j.type_ = JsonType::Int;
    j.data_ = v;
    return j;
}

JsonValue JsonValue::make_float(f64 v) noexcept {
    JsonValue j;
    j.type_ = JsonType::Float;
    j.data_ = v;
    return j;
}

JsonValue JsonValue::make_string(ca::str::Utf8StringRef v) noexcept {
    JsonValue j;
    j.type_ = JsonType::String;
    j.data_ = v;
    return j;
}

JsonValue JsonValue::make_array() {
    JsonValue j;
    j.type_ = JsonType::Array;
    j.data_ = ArrayStorage{};
    return j;
}

JsonValue JsonValue::make_object() {
    JsonValue j;
    j.type_ = JsonType::Object;
    j.data_ = ObjectStorage{};
    return j;
}

// ============================================================================
// 类型查询
// ============================================================================

JsonType JsonValue::type() const noexcept { return type_; }
bool JsonValue::is_null() const noexcept   { return type_ == JsonType::Null; }
bool JsonValue::is_bool() const noexcept   { return type_ == JsonType::Bool; }
bool JsonValue::is_int() const noexcept    { return type_ == JsonType::Int; }
bool JsonValue::is_float() const noexcept  { return type_ == JsonType::Float; }
bool JsonValue::is_number() const noexcept { return type_ == JsonType::Int || type_ == JsonType::Float; }
bool JsonValue::is_string() const noexcept { return type_ == JsonType::String; }
bool JsonValue::is_array() const noexcept  { return type_ == JsonType::Array; }
bool JsonValue::is_object() const noexcept { return type_ == JsonType::Object; }

// ============================================================================
// 严格访问
// ============================================================================

bool JsonValue::as_bool() const noexcept {
    assert(type_ == JsonType::Bool && "JsonValue::as_bool on non-Bool");
    return std::get<bool>(data_);
}

i64 JsonValue::as_int() const noexcept {
    assert(type_ == JsonType::Int && "JsonValue::as_int on non-Int");
    return std::get<i64>(data_);
}

f64 JsonValue::as_float() const noexcept {
    assert(type_ == JsonType::Float && "JsonValue::as_float on non-Float");
    return std::get<f64>(data_);
}

const ca::str::Utf8StringRef& JsonValue::as_string() const noexcept {
    assert(type_ == JsonType::String && "JsonValue::as_string on non-String");
    return std::get<ca::str::Utf8StringRef>(data_);
}

const JsonValue::ArrayStorage& JsonValue::as_array() const noexcept {
    assert(type_ == JsonType::Array && "JsonValue::as_array on non-Array");
    return std::get<ArrayStorage>(data_);
}

const JsonValue::ObjectStorage& JsonValue::as_object() const noexcept {
    assert(type_ == JsonType::Object && "JsonValue::as_object on non-Object");
    return std::get<ObjectStorage>(data_);
}

// ============================================================================
// 数值安全互转
// ============================================================================

f64 JsonValue::as_float_or(f64 fallback) const noexcept {
    if (type_ == JsonType::Float) return std::get<f64>(data_);
    if (type_ == JsonType::Int)   return static_cast<f64>(std::get<i64>(data_));
    return fallback;
}

i64 JsonValue::as_int_or(i64 fallback) const noexcept {
    if (type_ == JsonType::Int)   return std::get<i64>(data_);
    if (type_ == JsonType::Float) return static_cast<i64>(std::get<f64>(data_));
    return fallback;
}

// ============================================================================
// Array 编辑
// ============================================================================

void JsonValue::append(JsonValue v) {
    assert(type_ == JsonType::Array && "JsonValue::append on non-Array");
    std::get<ArrayStorage>(data_).push_back(std::move(v));
}

const JsonValue& JsonValue::at(usize index) const noexcept {
    assert(type_ == JsonType::Array && "JsonValue::at on non-Array");
    const auto& arr = std::get<ArrayStorage>(data_);
    assert(index < arr.size() && "JsonValue::at index out of bounds");
    return arr[index];
}

JsonValue& JsonValue::at(usize index) noexcept {
    assert(type_ == JsonType::Array && "JsonValue::at on non-Array");
    auto& arr = std::get<ArrayStorage>(data_);
    assert(index < arr.size() && "JsonValue::at index out of bounds");
    return arr[index];
}

usize JsonValue::size() const noexcept {
    assert(type_ == JsonType::Array && "JsonValue::size on non-Array");
    return std::get<ArrayStorage>(data_).size();
}

// ============================================================================
// Object 编辑
// ============================================================================

void JsonValue::set(ca::str::Utf8StringRef key, JsonValue v) {
    assert(type_ == JsonType::Object && "JsonValue::set on non-Object");
    auto& obj = std::get<ObjectStorage>(data_);
    for (auto& m : obj) {
        if (m.first == key) {
            m.second = std::move(v);
            return;
        }
    }
    obj.push_back(ObjectMember{key, std::move(v)});
}

const JsonValue* JsonValue::find(const ca::str::Utf8StringRef& key) const noexcept {
    assert(type_ == JsonType::Object && "JsonValue::find on non-Object");
    const auto& obj = std::get<ObjectStorage>(data_);
    for (const auto& m : obj) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

JsonValue* JsonValue::find(const ca::str::Utf8StringRef& key) noexcept {
    assert(type_ == JsonType::Object && "JsonValue::find on non-Object");
    auto& obj = std::get<ObjectStorage>(data_);
    for (auto& m : obj) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

bool JsonValue::remove(const ca::str::Utf8StringRef& key) noexcept {
    assert(type_ == JsonType::Object && "JsonValue::remove on non-Object");
    auto& obj = std::get<ObjectStorage>(data_);
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it->first == key) {
            obj.erase(it);
            return true;
        }
    }
    return false;
}

}  // namespace ca::json
