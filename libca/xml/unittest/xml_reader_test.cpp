#include <gtest/gtest.h>

#include <string>
#include <string_view>

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
std::string SV(const Utf8StringRef& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.byte_length());
}

// 解析成功，取出 document；失败则 ADD_FAILURE 并返回空 document。
XmlDocument read_ok(const char* text, XmlReaderOptions opts = XmlReaderOptions()) {
    auto result = XmlReader::read(R(text), opts);
    if (result.is_err()) {
        auto err = std::move(result).unwrap_err();
        ADD_FAILURE() << "parse failed @" << err.location.line << ":" << err.location.column
                      << " — " << S(err.message) << "\ninput: " << text;
        return XmlDocument();
    }
    return std::move(result).unwrap();
}

bool read_fails(const char* text) { return XmlReader::read(R(text)).is_err(); }

// 解析失败且错误消息含 substr。
void read_fails_with(const char* text, const char* substr) {
    auto result = XmlReader::read(R(text));
    ASSERT_TRUE(result.is_err()) << "expected failure but parsed ok: " << text;
    auto err = std::move(result).unwrap_err();
    EXPECT_NE(S(err.message).find(substr), std::string::npos)
        << "message [" << S(err.message) << "] missing [" << substr << "] for input: " << text;
}

}  // namespace

// ============================================================================
// 基本元素 / 属性
// ============================================================================

TEST(XmlReaderTest, SingleRootSelfClosing) {
    auto doc = read_ok("<root/>");
    ASSERT_TRUE(doc.root().is_element());
    EXPECT_EQ(doc.root().name(), R("root"));
    EXPECT_EQ(doc.root().child_count(), 0u);
}

TEST(XmlReaderTest, ElementWithText) {
    auto doc = read_ok("<port>8080</port>");
    EXPECT_EQ(doc.root().name(), R("port"));
    EXPECT_EQ(S(doc.root().text()), "8080");
}

TEST(XmlReaderTest, Attributes) {
    auto doc = read_ok(R"(<server host="localhost" port="8080" secure='true'/>)");
    const auto& e = doc.root();
    ASSERT_EQ(e.attributes().size(), 3u);
    EXPECT_EQ(SV(*e.attribute(R("host"))), "localhost");
    EXPECT_EQ(SV(*e.attribute(R("port"))), "8080");
    EXPECT_EQ(SV(*e.attribute(R("secure"))), "true");
}

TEST(XmlReaderTest, NestedChildren) {
    auto doc = read_ok(
        "<config>\n"
        "  <server>\n"
        "    <host>localhost</host>\n"
        "    <port>8080</port>\n"
        "  </server>\n"
        "</config>\n");
    const XmlNode* server = doc.root().first_element(R("server"));
    ASSERT_NE(server, nullptr);
    const XmlNode* host = server->first_element(R("host"));
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(S(host->text()), "localhost");
    EXPECT_EQ(S(server->first_element(R("port"))->text()), "8080");
    // trim_whitespace 默认开：server 的直接子节点只有两个元素，无空白文本节点。
    EXPECT_EQ(server->child_count(), 2u);
}

TEST(XmlReaderTest, RepeatedChildElements) {
    auto doc = read_ok("<list><item>a</item><item>b</item><item>c</item></list>");
    const auto& items = doc.root().children();
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(S(items[0].text()), "a");
    EXPECT_EQ(S(items[1].text()), "b");
    EXPECT_EQ(S(items[2].text()), "c");
}

// ============================================================================
// 实体 / 数字字符引用
// ============================================================================

TEST(XmlReaderTest, NamedEntitiesInText) {
    auto doc = read_ok("<t>a &lt; b &amp; c &gt; d &quot;e&quot; &apos;f&apos;</t>");
    EXPECT_EQ(S(doc.root().text()), "a < b & c > d \"e\" 'f'");
}

TEST(XmlReaderTest, EntitiesInAttribute) {
    auto doc = read_ok(R"(<a v="x &amp; y &lt; z"/>)");
    EXPECT_EQ(SV(*doc.root().attribute(R("v"))), "x & y < z");
}

TEST(XmlReaderTest, NumericCharRefDecimalAndHex) {
    auto doc = read_ok("<t>&#65;&#66;&#x43;&#x2764;</t>");
    // 65=A 66=B 0x43=C 0x2764=❤(U+2764, UTF-8 E2 9D A4)
    EXPECT_EQ(S(doc.root().text()), std::string("ABC") + "\xE2\x9D\xA4");
}

TEST(XmlReaderTest, UnknownEntityRejected) {
    read_fails_with("<t>&nbsp;</t>", "unknown entity");
}

// ============================================================================
// 注释 / CDATA
// ============================================================================

TEST(XmlReaderTest, CommentsPreservedAsNodes) {
    auto doc = read_ok("<r><!-- hi --><a/></r>");
    const auto& kids = doc.root().children();
    ASSERT_EQ(kids.size(), 2u);
    EXPECT_TRUE(kids[0].is_comment());
    EXPECT_EQ(SV(kids[0].value()), " hi ");
    EXPECT_TRUE(kids[1].is_element());
}

TEST(XmlReaderTest, PrologComment) {
    auto doc = read_ok("<!-- license header -->\n<root/>");
    ASSERT_EQ(doc.prolog().size(), 1u);
    EXPECT_TRUE(doc.prolog()[0].is_comment());
    EXPECT_EQ(SV(doc.prolog()[0].value()), " license header ");
}

TEST(XmlReaderTest, CdataRawContent) {
    auto doc = read_ok("<code><![CDATA[ if (a < b && c > d) x = \"y\"; ]]></code>");
    const auto& kids = doc.root().children();
    ASSERT_EQ(kids.size(), 1u);
    EXPECT_TRUE(kids[0].is_cdata());
    EXPECT_EQ(SV(kids[0].value()), " if (a < b && c > d) x = \"y\"; ");
    // text() 也纳入 CDATA 内容
    EXPECT_EQ(S(doc.root().text()), " if (a < b && c > d) x = \"y\"; ");
}

// ============================================================================
// 混合内容
// ============================================================================

TEST(XmlReaderTest, MixedContentPreserved) {
    auto doc = read_ok("<p>Hello <b>world</b>!</p>");
    const auto& kids = doc.root().children();
    ASSERT_EQ(kids.size(), 3u);
    EXPECT_TRUE(kids[0].is_text());
    EXPECT_EQ(SV(kids[0].value()), "Hello ");
    EXPECT_TRUE(kids[1].is_element());
    EXPECT_EQ(kids[1].name(), R("b"));
    EXPECT_TRUE(kids[2].is_text());
    EXPECT_EQ(SV(kids[2].value()), "!");
}

TEST(XmlReaderTest, TrimWhitespaceOffKeepsWhitespaceNodes) {
    XmlReaderOptions opts;
    opts.trim_whitespace = false;
    auto doc = read_ok("<a>\n  <b/>\n</a>", opts);
    // 关掉 trim：<a> 下有 "\n  " / <b> / "\n" 三个子节点
    EXPECT_EQ(doc.root().child_count(), 3u);
    EXPECT_TRUE(doc.root().children()[0].is_text());
}

// ============================================================================
// 声明 / BOM
// ============================================================================

TEST(XmlReaderTest, XmlDeclaration) {
    auto doc = read_ok(R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><root/>)");
    ASSERT_TRUE(doc.declaration().present);
    EXPECT_EQ(SV(doc.declaration().version), "1.0");
    EXPECT_EQ(SV(doc.declaration().encoding), "UTF-8");
    EXPECT_EQ(SV(doc.declaration().standalone), "yes");
}

TEST(XmlReaderTest, BomSkipped) {
    auto doc = read_ok("\xEF\xBB\xBF<root>x</root>");
    EXPECT_EQ(doc.root().name(), R("root"));
    EXPECT_EQ(S(doc.root().text()), "x");
}

TEST(XmlReaderTest, UnicodeElementNameAndContent) {
    auto doc = read_ok("<\xE6\xA0\x87\xE7\xAD\xBE>\xE5\x80\xBC</\xE6\xA0\x87\xE7\xAD\xBE>");
    EXPECT_EQ(S(doc.root().text()), "\xE5\x80\xBC");
}

// ============================================================================
// 良构性 / 拒绝
// ============================================================================

TEST(XmlReaderTest, MismatchedCloseTag) {
    read_fails_with("<a></b>", "mismatched closing tag");
}

TEST(XmlReaderTest, UnclosedElement) { EXPECT_TRUE(read_fails("<a><b></a>")); }

TEST(XmlReaderTest, MultipleRootsRejected) {
    read_fails_with("<a/><b/>", "only one root element");
}

TEST(XmlReaderTest, EmptyDocumentRejected) {
    read_fails_with("   \n  ", "expected a root element");
}

TEST(XmlReaderTest, DoctypeRejected) {
    read_fails_with("<!DOCTYPE html><html/>", "DOCTYPE");
}

TEST(XmlReaderTest, ProcessingInstructionRejected) {
    read_fails_with("<root><?php echo 1;?></root>", "processing instructions");
}

TEST(XmlReaderTest, DuplicateAttributeRejected) {
    read_fails_with(R"(<a x="1" x="2"/>)", "duplicate attribute");
}

TEST(XmlReaderTest, LessThanInAttributeRejected) {
    read_fails_with(R"(<a v="x < y"/>)", "not allowed in an attribute value");
}

TEST(XmlReaderTest, UnterminatedComment) { EXPECT_TRUE(read_fails("<a><!-- oops</a>")); }

TEST(XmlReaderTest, UnterminatedCdata) { EXPECT_TRUE(read_fails("<a><![CDATA[oops</a>")); }

TEST(XmlReaderTest, ErrorLocationReported) {
    auto result = XmlReader::read(R("<a>\n<b></c>\n</a>"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    EXPECT_EQ(err.location.line, 2u);  // </c> 在第 2 行
}

// 非法 UTF-8 此前静默变空：元素名/文本经 arena intern 变空串（产出空名元素的
// 损坏 DOM 且解析"成功"），注释/CDATA 却原样保留。入口整体校验后统一拒绝。
TEST(XmlReaderTest, RejectsInvalidUtf8) {
    // 非法元素名：<0xFF/>
    const u8 bad_name[] = "<\xFF/>";
    EXPECT_TRUE(XmlReader::read(Utf8StringRef::from_data(
        bad_name, sizeof(bad_name) - 1)).is_err());
    // 文本节点含 GBK 字节（"张" 的 GBK 编码）
    const u8 bad_text[] = "<a>\xD5\xC5</a>";
    EXPECT_TRUE(XmlReader::read(Utf8StringRef::from_data(
        bad_text, sizeof(bad_text) - 1)).is_err());
    // 对照：合法 UTF-8 内容不受影响
    EXPECT_TRUE(XmlReader::read(R("<a>中文</a>")).is_ok());
}

// 深度守卫：默认 max_depth=1000，嵌套元素超限须报错而非栈溢出。
TEST(XmlReaderTest, RejectsExcessiveNesting) {
    std::string deep;
    for (int i = 0; i < 1200; ++i) deep += "<a>";
    for (int i = 0; i < 1200; ++i) deep += "</a>";
    EXPECT_TRUE(XmlReader::read(Utf8StringRef::from_string_view(deep)).is_err());

    std::string moderate;
    for (int i = 0; i < 50; ++i) moderate += "<a>";
    for (int i = 0; i < 50; ++i) moderate += "</a>";
    EXPECT_TRUE(XmlReader::read(Utf8StringRef::from_string_view(moderate)).is_ok());
}
