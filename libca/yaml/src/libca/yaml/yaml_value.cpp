#include "libca/yaml/yaml_value.hpp"

#include <cassert>
#include <cstddef>
#include <utility>

namespace ca::yaml {

namespace {

// key 的字节视图（指向 arena，生命周期随所属 document）。空 key 安全回落。
std::string_view key_view(const ca::str::Utf8StringRef& key) noexcept
{
    if (key.data() == nullptr || key.byte_length() == 0)
        return {};
    return std::string_view(reinterpret_cast<const char*>(key.data()), key.byte_length());
}

}   // namespace

// ============================================================================
// 构造 / 析构 / 移动
// ============================================================================

// 默认 Null：YAML 根可为任意节点，空文档即 Null。
YamlValue::YamlValue() noexcept
    : type_(YamlType::Null)
    , data_(std::monostate{})
{}

YamlValue::YamlValue(YamlValue&& other) noexcept
    : type_(other.type_)
    , data_(std::move(other.data_))
{
    other.type_ = YamlType::Null;
    other.data_ = std::monostate{};
}

YamlValue& YamlValue::operator=(YamlValue&& other) noexcept
{
    if (this != &other) {
        type_       = other.type_;
        data_       = std::move(other.data_);
        other.type_ = YamlType::Null;
        other.data_ = std::monostate{};
    }
    return *this;
}

YamlValue::~YamlValue() = default;

YamlValue YamlValue::clone() const
{
    // Utf8StringRef / 标量可拷贝，容器深拷贝。
    YamlValue copy;
    copy.type_ = type_;
    switch (type_) {
    case YamlType::Null: break;
    case YamlType::Boolean: copy.data_ = std::get<bool>(data_); break;
    case YamlType::Integer: copy.data_ = std::get<ca::i64>(data_); break;
    case YamlType::Float: copy.data_ = std::get<ca::f64>(data_); break;
    case YamlType::String: copy.data_ = std::get<ca::str::Utf8StringRef>(data_); break;
    case YamlType::Sequence:
    {
        SequenceStorage dup;
        const auto&     src = std::get<SequenceStorage>(data_);
        dup.reserve(src.size());
        for (const auto& v : src)
            dup.push_back(v.clone());
        copy.data_ = std::move(dup);
        break;
    }
    case YamlType::Mapping:
    {
        MappingData dup;
        const auto& src = std::get<MappingData>(data_);
        dup.members.reserve(src.members.size());
        for (const auto& m : src.members) {
            dup.members.push_back(MappingMember{m.first, m.second.clone()});
        }
        // 索引的 string_view 指向 arena（clone 共享同一 arena），可直接复制。
        dup.index  = src.index;
        copy.data_ = std::move(dup);
        break;
    }
    }
    return copy;
}

// ============================================================================
// 工厂
// ============================================================================

YamlValue YamlValue::make_null() noexcept
{
    return YamlValue();
}

YamlValue YamlValue::make_boolean(bool v) noexcept
{
    YamlValue t;
    t.type_ = YamlType::Boolean;
    t.data_ = v;
    return t;
}

YamlValue YamlValue::make_integer(ca::i64 v) noexcept
{
    YamlValue t;
    t.type_ = YamlType::Integer;
    t.data_ = v;
    return t;
}

YamlValue YamlValue::make_float(ca::f64 v) noexcept
{
    YamlValue t;
    t.type_ = YamlType::Float;
    t.data_ = v;
    return t;
}

YamlValue YamlValue::make_string(ca::str::Utf8StringRef v) noexcept
{
    YamlValue t;
    t.type_ = YamlType::String;
    t.data_ = v;
    return t;
}

YamlValue YamlValue::make_sequence()
{
    YamlValue t;
    t.type_ = YamlType::Sequence;
    t.data_ = SequenceStorage{};
    return t;
}

YamlValue YamlValue::make_mapping()
{
    YamlValue t;
    t.type_ = YamlType::Mapping;
    t.data_ = MappingData{};
    return t;
}

// ============================================================================
// 类型查询
// ============================================================================

YamlType YamlValue::type() const noexcept
{
    return type_;
}

bool YamlValue::is_null() const noexcept
{
    return type_ == YamlType::Null;
}
bool YamlValue::is_boolean() const noexcept
{
    return type_ == YamlType::Boolean;
}
bool YamlValue::is_integer() const noexcept
{
    return type_ == YamlType::Integer;
}
bool YamlValue::is_float() const noexcept
{
    return type_ == YamlType::Float;
}
bool YamlValue::is_string() const noexcept
{
    return type_ == YamlType::String;
}
bool YamlValue::is_sequence() const noexcept
{
    return type_ == YamlType::Sequence;
}
bool YamlValue::is_mapping() const noexcept
{
    return type_ == YamlType::Mapping;
}

// ============================================================================
// 严格访问
// ============================================================================

bool YamlValue::as_boolean() const noexcept
{
    assert(type_ == YamlType::Boolean && "YamlValue::as_boolean on non-Boolean");
    return std::get<bool>(data_);
}

ca::i64 YamlValue::as_integer() const noexcept
{
    assert(type_ == YamlType::Integer && "YamlValue::as_integer on non-Integer");
    return std::get<ca::i64>(data_);
}

ca::f64 YamlValue::as_float() const noexcept
{
    assert(type_ == YamlType::Float && "YamlValue::as_float on non-Float");
    return std::get<ca::f64>(data_);
}

const ca::str::Utf8StringRef& YamlValue::as_string() const noexcept
{
    assert(type_ == YamlType::String && "YamlValue::as_string on non-String");
    return std::get<ca::str::Utf8StringRef>(data_);
}

const YamlValue::SequenceStorage& YamlValue::as_sequence() const noexcept
{
    assert(type_ == YamlType::Sequence && "YamlValue::as_sequence on non-Sequence");
    return std::get<SequenceStorage>(data_);
}

YamlValue::SequenceStorage& YamlValue::as_sequence() noexcept
{
    assert(type_ == YamlType::Sequence && "YamlValue::as_sequence on non-Sequence");
    return std::get<SequenceStorage>(data_);
}

const YamlValue::MappingStorage& YamlValue::as_mapping() const noexcept
{
    assert(type_ == YamlType::Mapping && "YamlValue::as_mapping on non-Mapping");
    return std::get<MappingData>(data_).members;
}

// ============================================================================
// 数值安全互转
// ============================================================================

ca::f64 YamlValue::as_float_or(ca::f64 fallback) const noexcept
{
    if (type_ == YamlType::Float)
        return std::get<ca::f64>(data_);
    if (type_ == YamlType::Integer)
        return static_cast<ca::f64>(std::get<ca::i64>(data_));
    return fallback;
}

ca::i64 YamlValue::as_integer_or(ca::i64 fallback) const noexcept
{
    if (type_ == YamlType::Integer)
        return std::get<ca::i64>(data_);
    if (type_ == YamlType::Float)
        return static_cast<ca::i64>(std::get<ca::f64>(data_));
    return fallback;
}

// ============================================================================
// Sequence 编辑
// ============================================================================

void YamlValue::append(YamlValue v)
{
    assert(type_ == YamlType::Sequence && "YamlValue::append on non-Sequence");
    std::get<SequenceStorage>(data_).push_back(std::move(v));
}

const YamlValue& YamlValue::at(ca::usize index) const noexcept
{
    assert(type_ == YamlType::Sequence && "YamlValue::at on non-Sequence");
    const auto& seq = std::get<SequenceStorage>(data_);
    assert(index < seq.size() && "YamlValue::at index out of bounds");
    return seq[index];
}

YamlValue& YamlValue::at(ca::usize index) noexcept
{
    assert(type_ == YamlType::Sequence && "YamlValue::at on non-Sequence");
    auto& seq = std::get<SequenceStorage>(data_);
    assert(index < seq.size() && "YamlValue::at index out of bounds");
    return seq[index];
}

ca::usize YamlValue::size() const noexcept
{
    assert(type_ == YamlType::Sequence && "YamlValue::size on non-Sequence");
    return std::get<SequenceStorage>(data_).size();
}

// ============================================================================
// Mapping 编辑
// ============================================================================

void YamlValue::set(ca::str::Utf8StringRef key, YamlValue v)
{
    assert(type_ == YamlType::Mapping && "YamlValue::set on non-Mapping");
    auto&      map = std::get<MappingData>(data_);
    const auto k   = key_view(key);
    auto       it  = map.index.find(k);
    if (it != map.index.end()) {
        map.members[it->second].second = std::move(v);
        return;
    }
    map.index.emplace(k, map.members.size());
    map.members.push_back(MappingMember{key, std::move(v)});
}

const YamlValue* YamlValue::find(const ca::str::Utf8StringRef& key) const noexcept
{
    assert(type_ == YamlType::Mapping && "YamlValue::find on non-Mapping");
    const auto& map = std::get<MappingData>(data_);
    auto        it  = map.index.find(key_view(key));
    return it == map.index.end() ? nullptr : &map.members[it->second].second;
}

YamlValue* YamlValue::find(const ca::str::Utf8StringRef& key) noexcept
{
    assert(type_ == YamlType::Mapping && "YamlValue::find on non-Mapping");
    auto& map = std::get<MappingData>(data_);
    auto  it  = map.index.find(key_view(key));
    return it == map.index.end() ? nullptr : &map.members[it->second].second;
}

bool YamlValue::remove(const ca::str::Utf8StringRef& key) noexcept
{
    assert(type_ == YamlType::Mapping && "YamlValue::remove on non-Mapping");
    auto& map = std::get<MappingData>(data_);
    auto  it  = map.index.find(key_view(key));
    if (it == map.index.end())
        return false;
    const ca::usize removed = it->second;
    map.members.erase(map.members.begin() + static_cast<std::ptrdiff_t>(removed));
    map.index.erase(it);
    // 后续成员整体前移一位，同步修正索引。
    for (auto& entry : map.index) {
        if (entry.second > removed)
            --entry.second;
    }
    return true;
}

}   // namespace ca::yaml
