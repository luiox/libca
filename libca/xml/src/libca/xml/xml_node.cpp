#include "libca/xml/xml_node.hpp"

#include <cassert>
#include <cstddef>
#include <utility>

namespace ca::xml {

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

// 默认空 Text 节点：无害的中性默认（XmlDocument root 在解析/构建前即此态）。
XmlNode::XmlNode() noexcept
    : type_(XmlNodeType::Text)
    , data_(ca::str::Utf8StringRef{})
{}

XmlNode::XmlNode(XmlNode&& other) noexcept
    : type_(other.type_)
    , data_(std::move(other.data_))
{
    other.type_ = XmlNodeType::Text;
    other.data_ = ca::str::Utf8StringRef{};
}

XmlNode& XmlNode::operator=(XmlNode&& other) noexcept
{
    if (this != &other) {
        type_       = other.type_;
        data_       = std::move(other.data_);
        other.type_ = XmlNodeType::Text;
        other.data_ = ca::str::Utf8StringRef{};
    }
    return *this;
}

XmlNode::~XmlNode() = default;

XmlNode XmlNode::clone() const
{
    XmlNode copy;
    copy.type_ = type_;
    if (type_ == XmlNodeType::Element) {
        const auto& src = std::get<ElementData>(data_);
        ElementData dup;
        dup.name       = src.name;
        dup.attributes = src.attributes;   // Utf8StringRef 可拷贝
        dup.attr_index = src.attr_index;   // string_view 指向同一 arena，可直接复制
        dup.children.reserve(src.children.size());
        for (const auto& c : src.children)
            dup.children.push_back(c.clone());
        copy.data_ = std::move(dup);
    }
    else {
        copy.data_ = std::get<ca::str::Utf8StringRef>(data_);
    }
    return copy;
}

// ============================================================================
// 工厂
// ============================================================================

XmlNode XmlNode::make_element(ca::str::Utf8StringRef name)
{
    XmlNode n;
    n.type_ = XmlNodeType::Element;
    ElementData e;
    e.name  = name;
    n.data_ = std::move(e);
    return n;
}

XmlNode XmlNode::make_text(ca::str::Utf8StringRef value) noexcept
{
    XmlNode n;
    n.type_ = XmlNodeType::Text;
    n.data_ = value;
    return n;
}

XmlNode XmlNode::make_comment(ca::str::Utf8StringRef value) noexcept
{
    XmlNode n;
    n.type_ = XmlNodeType::Comment;
    n.data_ = value;
    return n;
}

XmlNode XmlNode::make_cdata(ca::str::Utf8StringRef value) noexcept
{
    XmlNode n;
    n.type_ = XmlNodeType::Cdata;
    n.data_ = value;
    return n;
}

// ============================================================================
// 类型查询
// ============================================================================

XmlNodeType XmlNode::type() const noexcept
{
    return type_;
}

bool XmlNode::is_element() const noexcept
{
    return type_ == XmlNodeType::Element;
}
bool XmlNode::is_text() const noexcept
{
    return type_ == XmlNodeType::Text;
}
bool XmlNode::is_comment() const noexcept
{
    return type_ == XmlNodeType::Comment;
}
bool XmlNode::is_cdata() const noexcept
{
    return type_ == XmlNodeType::Cdata;
}

// ============================================================================
// 元素：名字
// ============================================================================

const ca::str::Utf8StringRef& XmlNode::name() const noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::name on non-Element");
    return std::get<ElementData>(data_).name;
}

void XmlNode::set_name(ca::str::Utf8StringRef name) noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::set_name on non-Element");
    std::get<ElementData>(data_).name = name;
}

// ============================================================================
// 元素：属性
// ============================================================================

void XmlNode::set_attribute(ca::str::Utf8StringRef key, ca::str::Utf8StringRef value)
{
    assert(type_ == XmlNodeType::Element && "XmlNode::set_attribute on non-Element");
    auto&      e  = std::get<ElementData>(data_);
    const auto k  = key_view(key);
    auto       it = e.attr_index.find(k);
    if (it != e.attr_index.end()) {
        e.attributes[it->second].second = value;
        return;
    }
    e.attr_index.emplace(k, e.attributes.size());
    e.attributes.push_back(Attribute{key, value});
}

const ca::str::Utf8StringRef* XmlNode::attribute(const ca::str::Utf8StringRef& key) const noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::attribute on non-Element");
    const auto& e  = std::get<ElementData>(data_);
    auto        it = e.attr_index.find(key_view(key));
    return it == e.attr_index.end() ? nullptr : &e.attributes[it->second].second;
}

bool XmlNode::has_attribute(const ca::str::Utf8StringRef& key) const noexcept
{
    return attribute(key) != nullptr;
}

bool XmlNode::remove_attribute(const ca::str::Utf8StringRef& key) noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::remove_attribute on non-Element");
    auto& e  = std::get<ElementData>(data_);
    auto  it = e.attr_index.find(key_view(key));
    if (it == e.attr_index.end())
        return false;
    const ca::usize removed = it->second;
    e.attributes.erase(e.attributes.begin() + static_cast<std::ptrdiff_t>(removed));
    e.attr_index.erase(it);
    // 后续属性整体前移一位，同步修正索引。
    for (auto& entry : e.attr_index) {
        if (entry.second > removed)
            --entry.second;
    }
    return true;
}

const XmlNode::AttributeStorage& XmlNode::attributes() const noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::attributes on non-Element");
    return std::get<ElementData>(data_).attributes;
}

// ============================================================================
// 元素：子节点
// ============================================================================

void XmlNode::append_child(XmlNode child)
{
    assert(type_ == XmlNodeType::Element && "XmlNode::append_child on non-Element");
    std::get<ElementData>(data_).children.push_back(std::move(child));
}

const XmlNode::ChildStorage& XmlNode::children() const noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::children on non-Element");
    return std::get<ElementData>(data_).children;
}

XmlNode::ChildStorage& XmlNode::children() noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::children on non-Element");
    return std::get<ElementData>(data_).children;
}

ca::usize XmlNode::child_count() const noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::child_count on non-Element");
    return std::get<ElementData>(data_).children.size();
}

const XmlNode* XmlNode::first_element(const ca::str::Utf8StringRef& name) const noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::first_element on non-Element");
    const auto& e = std::get<ElementData>(data_);
    for (const auto& c : e.children) {
        if (c.is_element() && c.name() == name)
            return &c;
    }
    return nullptr;
}

XmlNode* XmlNode::first_element(const ca::str::Utf8StringRef& name) noexcept
{
    assert(type_ == XmlNodeType::Element && "XmlNode::first_element on non-Element");
    auto& e = std::get<ElementData>(data_);
    for (auto& c : e.children) {
        if (c.is_element() && c.name() == name)
            return &c;
    }
    return nullptr;
}

ca::str::Utf8String XmlNode::text() const
{
    assert(type_ == XmlNodeType::Element && "XmlNode::text on non-Element");
    const auto&                e = std::get<ElementData>(data_);
    ca::str::Utf8StringBuilder sb;
    for (const auto& c : e.children) {
        if (c.is_text() || c.is_cdata()) {
            const auto& v = c.value();
            sb.append(v.data(), v.byte_length());
        }
    }
    return sb.build_or_empty();
}

// ============================================================================
// Text / Comment / Cdata：内容
// ============================================================================

const ca::str::Utf8StringRef& XmlNode::value() const noexcept
{
    assert(type_ != XmlNodeType::Element && "XmlNode::value on Element");
    return std::get<ca::str::Utf8StringRef>(data_);
}

void XmlNode::set_value(ca::str::Utf8StringRef value) noexcept
{
    assert(type_ != XmlNodeType::Element && "XmlNode::set_value on Element");
    data_ = value;
}

}   // namespace ca::xml
