#include <gtest/gtest.h>

#include <string>

#include "libca/xml/xml_document.hpp"
#include "libca/xml/xml_node.hpp"

using namespace ca;
using namespace ca::xml;
using ca::str::Utf8String;
using ca::str::Utf8StringRef;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

std::string S(const Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.byte_length());
}

}  // namespace

// ============================================================================
// 工厂 + 类型谓词
// ============================================================================

TEST(XmlNodeTest, FactoriesAndPredicates) {
    XmlDocument doc;
    auto& a = doc.arena();

    XmlNode el = XmlNode::make_element(a.intern("root"));
    EXPECT_TRUE(el.is_element());
    EXPECT_EQ(el.type(), XmlNodeType::Element);
    EXPECT_EQ(el.name(), R("root"));

    XmlNode tx = XmlNode::make_text(a.intern("hello"));
    EXPECT_TRUE(tx.is_text());
    EXPECT_EQ(tx.value(), R("hello"));

    XmlNode cm = XmlNode::make_comment(a.intern(" note "));
    EXPECT_TRUE(cm.is_comment());
    EXPECT_EQ(cm.value(), R(" note "));

    XmlNode cd = XmlNode::make_cdata(a.intern("<raw> & data"));
    EXPECT_TRUE(cd.is_cdata());
    EXPECT_EQ(cd.value(), R("<raw> & data"));
}

// ============================================================================
// 属性：保序 + 索引 + 覆盖 + 删除
// ============================================================================

TEST(XmlNodeTest, AttributesOrderAndIndex) {
    XmlDocument doc;
    auto& a = doc.arena();
    XmlNode el = XmlNode::make_element(a.intern("server"));

    el.set_attribute(a.intern("host"), a.intern("localhost"));
    el.set_attribute(a.intern("port"), a.intern("8080"));
    el.set_attribute(a.intern("proto"), a.intern("tcp"));

    // 保插入序
    ASSERT_EQ(el.attributes().size(), 3u);
    EXPECT_EQ(el.attributes()[0].first, R("host"));
    EXPECT_EQ(el.attributes()[1].first, R("port"));
    EXPECT_EQ(el.attributes()[2].first, R("proto"));

    // O(1) 查找
    const auto* p = el.attribute(R("port"));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, R("8080"));
    EXPECT_TRUE(el.has_attribute(R("host")));
    EXPECT_EQ(el.attribute(R("missing")), nullptr);

    // 覆盖同名不改顺序
    el.set_attribute(a.intern("port"), a.intern("9090"));
    EXPECT_EQ(el.attributes().size(), 3u);
    EXPECT_EQ(*el.attribute(R("port")), R("9090"));
    EXPECT_EQ(el.attributes()[1].first, R("port"));
}

TEST(XmlNodeTest, RemoveAttributeKeepsIndexConsistent) {
    XmlDocument doc;
    auto& a = doc.arena();
    XmlNode el = XmlNode::make_element(a.intern("e"));
    el.set_attribute(a.intern("a"), a.intern("1"));
    el.set_attribute(a.intern("b"), a.intern("2"));
    el.set_attribute(a.intern("c"), a.intern("3"));

    EXPECT_TRUE(el.remove_attribute(R("b")));
    EXPECT_FALSE(el.remove_attribute(R("b")));  // 已删
    ASSERT_EQ(el.attributes().size(), 2u);
    // 删除后剩余属性仍可经索引正确定位（前移后索引同步）
    EXPECT_EQ(*el.attribute(R("a")), R("1"));
    EXPECT_EQ(*el.attribute(R("c")), R("3"));
    EXPECT_EQ(el.attributes()[0].first, R("a"));
    EXPECT_EQ(el.attributes()[1].first, R("c"));
}

// ============================================================================
// 子节点：混合内容 + 导航 + text()
// ============================================================================

TEST(XmlNodeTest, MixedChildrenAndNavigation) {
    XmlDocument doc;
    auto& a = doc.arena();
    // <p>Hello <b>world</b>!</p>
    XmlNode p = XmlNode::make_element(a.intern("p"));
    p.append_child(XmlNode::make_text(a.intern("Hello ")));
    XmlNode b = XmlNode::make_element(a.intern("b"));
    b.append_child(XmlNode::make_text(a.intern("world")));
    p.append_child(std::move(b));
    p.append_child(XmlNode::make_text(a.intern("!")));

    EXPECT_EQ(p.child_count(), 3u);
    EXPECT_TRUE(p.children()[0].is_text());
    EXPECT_TRUE(p.children()[1].is_element());
    EXPECT_TRUE(p.children()[2].is_text());

    // 直接文本拼接（跳过子元素 <b> 的内容）
    EXPECT_EQ(S(p.text()), "Hello !");

    // first_element 只找元素子节点
    const XmlNode* bp = p.first_element(R("b"));
    ASSERT_NE(bp, nullptr);
    EXPECT_EQ(S(bp->text()), "world");
    EXPECT_EQ(p.first_element(R("i")), nullptr);
}

// ============================================================================
// clone 深拷贝
// ============================================================================

TEST(XmlNodeTest, CloneIsDeep) {
    XmlDocument doc;
    auto& a = doc.arena();
    XmlNode root = XmlNode::make_element(a.intern("root"));
    root.set_attribute(a.intern("k"), a.intern("v"));
    XmlNode child = XmlNode::make_element(a.intern("child"));
    child.append_child(XmlNode::make_text(a.intern("txt")));
    root.append_child(std::move(child));

    XmlNode copy = root.clone();
    // 改动原树不影响副本
    root.first_element(R("child"))->append_child(XmlNode::make_text(a.intern("more")));
    EXPECT_EQ(copy.first_element(R("child"))->child_count(), 1u);
    EXPECT_EQ(root.first_element(R("child"))->child_count(), 2u);
    EXPECT_EQ(*copy.attribute(R("k")), R("v"));
}

// ============================================================================
// Document：默认态 / 声明 / prolog / clear / move
// ============================================================================

TEST(XmlNodeTest, DocumentDefaults) {
    XmlDocument doc;
    // root 默认空 Text
    EXPECT_TRUE(doc.root().is_text());
    EXPECT_FALSE(doc.declaration().present);
    EXPECT_TRUE(doc.prolog().empty());
    EXPECT_TRUE(doc.epilog().empty());
}

TEST(XmlNodeTest, DocumentBuildAndClear) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.declaration().present = true;
    doc.declaration().version = a.intern("1.0");
    doc.prolog().push_back(XmlNode::make_comment(a.intern(" header ")));
    doc.root() = XmlNode::make_element(a.intern("config"));
    doc.root().set_attribute(a.intern("v"), a.intern("2"));

    EXPECT_TRUE(doc.root().is_element());
    EXPECT_EQ(doc.prolog().size(), 1u);
    EXPECT_EQ(doc.declaration().version, R("1.0"));

    doc.clear();
    EXPECT_TRUE(doc.root().is_text());
    EXPECT_FALSE(doc.declaration().present);
    EXPECT_TRUE(doc.prolog().empty());
}

TEST(XmlNodeTest, DocumentMove) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("r"));
    doc.root().append_child(XmlNode::make_text(a.intern("body")));

    XmlDocument moved = std::move(doc);
    ASSERT_TRUE(moved.root().is_element());
    EXPECT_EQ(moved.root().name(), R("r"));
    EXPECT_EQ(S(moved.root().text()), "body");
}
