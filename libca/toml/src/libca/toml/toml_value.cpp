#include "libca/toml/toml_value.hpp"

#include <cassert>
#include <utility>

namespace ca::toml {

// ============================================================================
// 构造 / 析构 / 移动
// ============================================================================

// 默认 Table：TOML 文档根、子表未显式构造时都是 Table。
TomlValue::TomlValue() noexcept : type_(TomlType::Table), data_(TableStorage{}) {}

TomlValue::TomlValue(TomlValue&& other) noexcept
    : type_(other.type_), data_(std::move(other.data_)) {
    other.type_ = TomlType::Table;
    other.data_ = TableStorage{};
}

TomlValue& TomlValue::operator=(TomlValue&& other) noexcept {
    if (this != &other) {
        type_ = other.type_;
        data_ = std::move(other.data_);
        other.type_ = TomlType::Table;
        other.data_ = TableStorage{};
    }
    return *this;
}

TomlValue::~TomlValue() = default;

TomlValue TomlValue::clone() const {
    // Utf8StringRef 可拷贝，TomlDatetime / 标量可拷贝，vector 容器深拷贝。
    TomlValue copy;
    copy.type_ = type_;
    switch (type_) {
        case TomlType::String:          copy.data_ = std::get<ca::str::Utf8StringRef>(data_); break;
        case TomlType::Integer:         copy.data_ = std::get<ca::i64>(data_); break;
        case TomlType::Float:           copy.data_ = std::get<ca::f64>(data_); break;
        case TomlType::Boolean:         copy.data_ = std::get<bool>(data_); break;
        case TomlType::OffsetDatetime:
        case TomlType::LocalDateTime:
        case TomlType::LocalDate:
        case TomlType::LocalTime:       copy.data_ = std::get<TomlDatetime>(data_); break;
        case TomlType::Array: {
            ArrayStorage dup;
            const auto& src = std::get<ArrayStorage>(data_);
            dup.reserve(src.size());
            for (const auto& v : src) dup.push_back(v.clone());
            copy.data_ = std::move(dup);
            break;
        }
        case TomlType::Table: {
            TableStorage dup;
            const auto& src = std::get<TableStorage>(data_);
            dup.reserve(src.size());
            for (const auto& m : src) {
                dup.push_back(TableMember{m.first, m.second.clone()});
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

TomlValue TomlValue::make_string(ca::str::Utf8StringRef v) noexcept {
    TomlValue t;
    t.type_ = TomlType::String;
    t.data_ = v;
    return t;
}

TomlValue TomlValue::make_integer(ca::i64 v) noexcept {
    TomlValue t;
    t.type_ = TomlType::Integer;
    t.data_ = v;
    return t;
}

TomlValue TomlValue::make_float(ca::f64 v) noexcept {
    TomlValue t;
    t.type_ = TomlType::Float;
    t.data_ = v;
    return t;
}

TomlValue TomlValue::make_boolean(bool v) noexcept {
    TomlValue t;
    t.type_ = TomlType::Boolean;
    t.data_ = v;
    return t;
}

TomlValue TomlValue::make_offset_datetime(const TomlDatetime& v) noexcept {
    TomlValue t;
    t.type_ = TomlType::OffsetDatetime;
    TomlDatetime d = v;
    d.kind = TomlDatetimeKind::OffsetDatetime;
    d.has_tz = true;
    t.data_ = d;
    return t;
}

TomlValue TomlValue::make_local_datetime(const TomlDatetime& v) noexcept {
    TomlValue t;
    t.type_ = TomlType::LocalDateTime;
    TomlDatetime d = v;
    d.kind = TomlDatetimeKind::LocalDateTime;
    d.has_tz = false;
    t.data_ = d;
    return t;
}

TomlValue TomlValue::make_local_date(const TomlDatetime& v) noexcept {
    TomlValue t;
    t.type_ = TomlType::LocalDate;
    TomlDatetime d = v;
    d.kind = TomlDatetimeKind::LocalDate;
    t.data_ = d;
    return t;
}

TomlValue TomlValue::make_local_time(const TomlDatetime& v) noexcept {
    TomlValue t;
    t.type_ = TomlType::LocalTime;
    TomlDatetime d = v;
    d.kind = TomlDatetimeKind::LocalTime;
    t.data_ = d;
    return t;
}

TomlValue TomlValue::make_array() {
    TomlValue t;
    t.type_ = TomlType::Array;
    t.data_ = ArrayStorage{};
    return t;
}

TomlValue TomlValue::make_table() {
    TomlValue t;
    t.type_ = TomlType::Table;
    t.data_ = TableStorage{};
    return t;
}

// ============================================================================
// 类型查询
// ============================================================================

TomlType TomlValue::type() const noexcept { return type_; }

bool TomlValue::is_string() const noexcept          { return type_ == TomlType::String; }
bool TomlValue::is_integer() const noexcept         { return type_ == TomlType::Integer; }
bool TomlValue::is_float() const noexcept           { return type_ == TomlType::Float; }
bool TomlValue::is_boolean() const noexcept         { return type_ == TomlType::Boolean; }
bool TomlValue::is_offset_datetime() const noexcept { return type_ == TomlType::OffsetDatetime; }
bool TomlValue::is_local_datetime() const noexcept  { return type_ == TomlType::LocalDateTime; }
bool TomlValue::is_local_date() const noexcept      { return type_ == TomlType::LocalDate; }
bool TomlValue::is_local_time() const noexcept      { return type_ == TomlType::LocalTime; }
bool TomlValue::is_datetime() const noexcept {
    return type_ == TomlType::OffsetDatetime || type_ == TomlType::LocalDateTime ||
           type_ == TomlType::LocalDate || type_ == TomlType::LocalTime;
}
bool TomlValue::is_array() const noexcept  { return type_ == TomlType::Array; }
bool TomlValue::is_table() const noexcept  { return type_ == TomlType::Table; }

// ============================================================================
// 严格访问
// ============================================================================

const ca::str::Utf8StringRef& TomlValue::as_string() const noexcept {
    assert(type_ == TomlType::String && "TomlValue::as_string on non-String");
    return std::get<ca::str::Utf8StringRef>(data_);
}

ca::i64 TomlValue::as_integer() const noexcept {
    assert(type_ == TomlType::Integer && "TomlValue::as_integer on non-Integer");
    return std::get<ca::i64>(data_);
}

ca::f64 TomlValue::as_float() const noexcept {
    assert(type_ == TomlType::Float && "TomlValue::as_float on non-Float");
    return std::get<ca::f64>(data_);
}

bool TomlValue::as_boolean() const noexcept {
    assert(type_ == TomlType::Boolean && "TomlValue::as_boolean on non-Boolean");
    return std::get<bool>(data_);
}

const TomlDatetime& TomlValue::as_offset_datetime() const noexcept {
    assert(type_ == TomlType::OffsetDatetime && "TomlValue::as_offset_datetime on non-OffsetDatetime");
    return std::get<TomlDatetime>(data_);
}

const TomlDatetime& TomlValue::as_local_datetime() const noexcept {
    assert(type_ == TomlType::LocalDateTime && "TomlValue::as_local_datetime on non-LocalDateTime");
    return std::get<TomlDatetime>(data_);
}

const TomlDatetime& TomlValue::as_local_date() const noexcept {
    assert(type_ == TomlType::LocalDate && "TomlValue::as_local_date on non-LocalDate");
    return std::get<TomlDatetime>(data_);
}

const TomlDatetime& TomlValue::as_local_time() const noexcept {
    assert(type_ == TomlType::LocalTime && "TomlValue::as_local_time on non-LocalTime");
    return std::get<TomlDatetime>(data_);
}

const TomlDatetime& TomlValue::as_datetime() const noexcept {
    assert(is_datetime() && "TomlValue::as_datetime on non-datetime");
    return std::get<TomlDatetime>(data_);
}

const TomlValue::ArrayStorage& TomlValue::as_array() const noexcept {
    assert(type_ == TomlType::Array && "TomlValue::as_array on non-Array");
    return std::get<ArrayStorage>(data_);
}

TomlValue::ArrayStorage& TomlValue::as_array() noexcept {
    assert(type_ == TomlType::Array && "TomlValue::as_array on non-Array");
    return std::get<ArrayStorage>(data_);
}

const TomlValue::TableStorage& TomlValue::as_table() const noexcept {
    assert(type_ == TomlType::Table && "TomlValue::as_table on non-Table");
    return std::get<TableStorage>(data_);
}

TomlValue::TableStorage& TomlValue::as_table() noexcept {
    assert(type_ == TomlType::Table && "TomlValue::as_table on non-Table");
    return std::get<TableStorage>(data_);
}

// ============================================================================
// 数值安全互转
// ============================================================================

ca::f64 TomlValue::as_float_or(ca::f64 fallback) const noexcept {
    if (type_ == TomlType::Float)   return std::get<ca::f64>(data_);
    if (type_ == TomlType::Integer) return static_cast<ca::f64>(std::get<ca::i64>(data_));
    return fallback;
}

ca::i64 TomlValue::as_integer_or(ca::i64 fallback) const noexcept {
    if (type_ == TomlType::Integer) return std::get<ca::i64>(data_);
    if (type_ == TomlType::Float)   return static_cast<ca::i64>(std::get<ca::f64>(data_));
    return fallback;
}

// ============================================================================
// Array 编辑
// ============================================================================

void TomlValue::append(TomlValue v) {
    assert(type_ == TomlType::Array && "TomlValue::append on non-Array");
    std::get<ArrayStorage>(data_).push_back(std::move(v));
}

const TomlValue& TomlValue::at(ca::usize index) const noexcept {
    assert(type_ == TomlType::Array && "TomlValue::at on non-Array");
    const auto& arr = std::get<ArrayStorage>(data_);
    assert(index < arr.size() && "TomlValue::at index out of bounds");
    return arr[index];
}

TomlValue& TomlValue::at(ca::usize index) noexcept {
    assert(type_ == TomlType::Array && "TomlValue::at on non-Array");
    auto& arr = std::get<ArrayStorage>(data_);
    assert(index < arr.size() && "TomlValue::at index out of bounds");
    return arr[index];
}

ca::usize TomlValue::size() const noexcept {
    assert(type_ == TomlType::Array && "TomlValue::size on non-Array");
    return std::get<ArrayStorage>(data_).size();
}

// ============================================================================
// Table 编辑
// ============================================================================

void TomlValue::set(ca::str::Utf8StringRef key, TomlValue v) {
    assert(type_ == TomlType::Table && "TomlValue::set on non-Table");
    auto& tbl = std::get<TableStorage>(data_);
    for (auto& m : tbl) {
        if (m.first == key) {
            m.second = std::move(v);
            return;
        }
    }
    tbl.push_back(TableMember{key, std::move(v)});
}

const TomlValue* TomlValue::find(const ca::str::Utf8StringRef& key) const noexcept {
    assert(type_ == TomlType::Table && "TomlValue::find on non-Table");
    const auto& tbl = std::get<TableStorage>(data_);
    for (const auto& m : tbl) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

TomlValue* TomlValue::find(const ca::str::Utf8StringRef& key) noexcept {
    assert(type_ == TomlType::Table && "TomlValue::find on non-Table");
    auto& tbl = std::get<TableStorage>(data_);
    for (auto& m : tbl) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

bool TomlValue::remove(const ca::str::Utf8StringRef& key) noexcept {
    assert(type_ == TomlType::Table && "TomlValue::remove on non-Table");
    auto& tbl = std::get<TableStorage>(data_);
    for (auto it = tbl.begin(); it != tbl.end(); ++it) {
        if (it->first == key) {
            tbl.erase(it);
            return true;
        }
    }
    return false;
}

}  // namespace ca::toml
