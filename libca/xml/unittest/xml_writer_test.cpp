#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "libca/xml/xml.hpp"

using namespace ca;
using namespace ca::xml;
using ca::str::Utf8String;
using ca::str::Utf8StringRef;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

std::string S(const Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.byte_length());
}

// 递归比较两棵 XML DOM（含属性保序、子节点顺序）。
bool dom_equal(const XmlNode& a, const XmlNode& b) {
    if (a.type() != b.type()) return false;
    if (a.is_element()) {
        if (a.name() != b.name()) return false;
        if (a.attributes().size() != b.attributes().size()) return false;
        for (usize i = 0; i < a.attributes().size(); ++i) {
            if (a.attributes()[i].first != b.attributes()[i].first) return false;
            if (a.attributes()[i].second != b.attributes()[i].second) return false;
        }
        if (a.child_count() != b.child_count()) return false;
        for (usize i = 0; i < a.child_count(); ++i) {
            if (!dom_equal(a.children()[i], b.children()[i])) return false;
        }
        return true;
    }
    return a.value() == b.value();
}

// write(read(text)) 再 read，验证与首解析 DOM 相等。
void expect_roundtrip(const char* text) {
    auto first = XmlReader::read(R(text));
    ASSERT_TRUE(first.is_ok()) << "initial parse failed: " << text;
    auto doc1 = std::move(first).unwrap();
    Utf8String out = XmlWriter::write(doc1);
    auto second = XmlReader::read(out.ref());
    ASSERT_TRUE(second.is_ok()) << "re-parse failed for output:\n" << S(out);
    auto doc2 = std::move(second).unwrap();
    EXPECT_TRUE(dom_equal(doc1.root(), doc2.root())) << "roundtrip mismatch. output:\n" << S(out);
}

}  // namespace

// ============================================================================
// 缩进美化布局
// ============================================================================

TEST(XmlWriterTest, NestedLayoutGolden) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("config"));
    XmlNode server = XmlNode::make_element(a.intern("server"));
    XmlNode host = XmlNode::make_element(a.intern("host"));
    host.append_child(XmlNode::make_text(a.intern("localhost")));
    XmlNode port = XmlNode::make_element(a.intern("port"));
    port.append_child(XmlNode::make_text(a.intern("8080")));
    server.append_child(std::move(host));
    server.append_child(std::move(port));
    doc.root().append_child(std::move(server));

    EXPECT_EQ(S(XmlWriter::write(doc)),
              "<config>\n"
              "  <server>\n"
              "    <host>localhost</host>\n"
              "    <port>8080</port>\n"
              "  </server>\n"
              "</config>\n");
}

TEST(XmlWriterTest, EmptyElementSelfCloses) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("root"));
    doc.root().append_child(XmlNode::make_element(a.intern("empty")));
    EXPECT_EQ(S(XmlWriter::write(doc)),
              "<root>\n"
              "  <empty/>\n"
              "</root>\n");
}

TEST(XmlWriterTest, AttributesSerialized) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("e"));
    doc.root().set_attribute(a.intern("a"), a.intern("1"));
    doc.root().set_attribute(a.intern("b"), a.intern("two"));
    EXPECT_EQ(S(XmlWriter::write(doc)), "<e a=\"1\" b=\"two\"/>\n");
}

// ============================================================================
// 转义
// ============================================================================

TEST(XmlWriterTest, TextEscaping) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("t"));
    doc.root().append_child(XmlNode::make_text(a.intern("a < b & c > d")));
    EXPECT_EQ(S(XmlWriter::write(doc)), "<t>a &lt; b &amp; c &gt; d</t>\n");
}

TEST(XmlWriterTest, AttributeEscaping) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("e"));
    doc.root().set_attribute(a.intern("v"), a.intern("x & y < z \" w"));
    const std::string out = S(XmlWriter::write(doc));
    EXPECT_NE(out.find("&amp;"), std::string::npos);
    EXPECT_NE(out.find("&lt;"), std::string::npos);
    EXPECT_NE(out.find("&quot;"), std::string::npos);
}

TEST(XmlWriterTest, CdataAndComment) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("r"));
    doc.root().append_child(XmlNode::make_comment(a.intern(" note ")));
    XmlNode code = XmlNode::make_element(a.intern("code"));
    code.append_child(XmlNode::make_cdata(a.intern("if (a<b) {}")));
    doc.root().append_child(std::move(code));
    const std::string out = S(XmlWriter::write(doc));
    EXPECT_NE(out.find("<!-- note -->"), std::string::npos) << out;
    EXPECT_NE(out.find("<![CDATA[if (a<b) {}]]>"), std::string::npos) << out;
}

// ============================================================================
// 声明
// ============================================================================

TEST(XmlWriterTest, DeclarationEmitted) {
    XmlDocument doc;
    auto& a = doc.arena();
    doc.declaration().present = true;
    doc.declaration().version = a.intern("1.0");
    doc.declaration().encoding = a.intern("UTF-8");
    doc.root() = XmlNode::make_element(a.intern("r"));
    const std::string out = S(XmlWriter::write(doc));
    EXPECT_EQ(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<r/>\n");
}

// ============================================================================
// write_file + read_file
// ============================================================================

TEST(XmlWriterTest, WriteFileReadFileRoundtrip) {
    const std::string path = "build/libca_xml_roundtrip_test.xml";
    XmlDocument doc;
    auto& a = doc.arena();
    doc.root() = XmlNode::make_element(a.intern("root"));
    XmlNode k = XmlNode::make_element(a.intern("key"));
    k.append_child(XmlNode::make_text(a.intern("value")));
    doc.root().append_child(std::move(k));

    ASSERT_TRUE(XmlWriter::write_file(R(path.c_str()), doc).is_ok());
    auto loaded = XmlReader::read_file(R(path.c_str()));
    ASSERT_TRUE(loaded.is_ok());
    auto ldoc = std::move(loaded).unwrap();
    EXPECT_EQ(S(ldoc.root().first_element(R("key"))->text()), "value");
    std::remove(path.c_str());
}

// ============================================================================
// write → read → dom_equal 往返
// ============================================================================

TEST(XmlWriterTest, RoundtripNestedConfig) {
    expect_roundtrip(
        "<config>\n"
        "  <server>\n"
        "    <host>localhost</host>\n"
        "    <port>8080</port>\n"
        "  </server>\n"
        "  <features>\n"
        "    <feature name=\"a\"/>\n"
        "    <feature name=\"b\"/>\n"
        "  </features>\n"
        "</config>\n");
}

TEST(XmlWriterTest, RoundtripMixedContent) {
    expect_roundtrip("<p>Hello <b>world</b> and <i>more</i>!</p>");
}

TEST(XmlWriterTest, RoundtripEntitiesAndCdata) {
    expect_roundtrip(
        "<doc>\n"
        "  <text>a &lt; b &amp; c &gt; d</text>\n"
        "  <raw><![CDATA[if (x < y && z > w) {}]]></raw>\n"
        "</doc>\n");
}

TEST(XmlWriterTest, RoundtripAttributesWithSpecials) {
    expect_roundtrip(R"(<e path="a/b&amp;c" q="x&lt;y" note="he said &quot;hi&quot;"/>)");
}

TEST(XmlWriterTest, RoundtripCommentsPreserved) {
    expect_roundtrip(
        "<root>\n"
        "  <!-- a comment -->\n"
        "  <child/>\n"
        "</root>\n");
}

TEST(XmlWriterTest, RoundtripWithDeclaration) {
    expect_roundtrip(R"(<?xml version="1.0" encoding="UTF-8"?><root><a>1</a></root>)");
}
