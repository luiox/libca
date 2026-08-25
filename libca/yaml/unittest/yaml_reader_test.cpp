#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <string_view>

#include "libca/yaml/yaml.hpp"

using namespace ca;
using namespace ca::yaml;
using ca::str::Utf8String;
using ca::str::Utf8StringRef;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

YamlDocument read_ok(const char* text) {
    auto result = YamlReader::read(R(text));
    if (result.is_err()) {
        const auto err = std::move(result).unwrap_err();
        ADD_FAILURE() << "parse failed: " << std::string(std::string_view(err.message))
                      << " (line " << err.location.line << ", col " << err.location.column << ")";
        return YamlDocument();
    }
    return std::move(result).unwrap();
}

bool read_fails(const char* text) {
    return YamlReader::read(R(text)).is_err();
}

// 断言解析失败且错误消息含 substr（拒绝类特性要求错误消息明确）。
bool read_fails_with(const char* text, const char* substr) {
    auto result = YamlReader::read(R(text));
    if (result.is_ok()) return false;
    const auto err = std::move(result).unwrap_err();
    const std::string_view msg(err.message);
    return msg.find(substr) != std::string_view::npos;
}

}  // namespace

// ============================================================================
// 标量与类型判定（YAML 1.2 core schema）
// ============================================================================

TEST(YamlReaderTest, RejectsInvalidUtf8) {
    // plain 标量里的非法序列（0xC3 0x28）：此前经 arena intern 静默变空串。
    const u8 bad_plain[] = "key: \xC3\x28";
    EXPECT_TRUE(YamlReader::read(
        Utf8StringRef::from_data(bad_plain, sizeof(bad_plain) - 1)).is_err());
    // 双引号标量里的非法尾字节。
    const u8 bad_quoted[] = "key: \"\xFF\"";
    EXPECT_TRUE(YamlReader::read(
        Utf8StringRef::from_data(bad_quoted, sizeof(bad_quoted) - 1)).is_err());
    // 对照：合法多字节内容不受影响。
    auto doc = read_ok("key: 中文");
    const auto* v = doc.root().find(R("key"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_string(), R("中文"));
}

TEST(YamlReaderTest, StringScalar) {
    auto doc = read_ok("key: hello");
    const auto* v = doc.root().find(R("key"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_string());
    EXPECT_EQ(v->as_string(), R("hello"));
}

TEST(YamlReaderTest, NullForms) {
    auto doc = read_ok("a:\nb: null\nc: Null\nd: NULL\ne: ~");
    for (const char* k : {"a", "b", "c", "d", "e"}) {
        const auto* v = doc.root().find(R(k));
        ASSERT_NE(v, nullptr) << k;
        EXPECT_TRUE(v->is_null()) << k;
    }
}

TEST(YamlReaderTest, BooleanForms) {
    auto doc = read_ok("t1: true\nt2: True\nt3: TRUE\nf1: false\nf2: False\nf3: FALSE");
    EXPECT_TRUE(doc.root().find(R("t1"))->as_boolean());
    EXPECT_TRUE(doc.root().find(R("t2"))->as_boolean());
    EXPECT_TRUE(doc.root().find(R("t3"))->as_boolean());
    EXPECT_FALSE(doc.root().find(R("f1"))->as_boolean());
    EXPECT_FALSE(doc.root().find(R("f2"))->as_boolean());
    EXPECT_FALSE(doc.root().find(R("f3"))->as_boolean());
}

TEST(YamlReaderTest, NorwayProblemStaysString) {
    // yes/no/on/off 不是布尔（core schema），保持字符串。
    auto doc = read_ok("a: yes\nb: no\nc: on\nd: off\ne: y\nf: n");
    for (const char* k : {"a", "b", "c", "d", "e", "f"}) {
        EXPECT_TRUE(doc.root().find(R(k))->is_string()) << k;
    }
    EXPECT_EQ(doc.root().find(R("b"))->as_string(), R("no"));
}

TEST(YamlReaderTest, Integers) {
    auto doc = read_ok("a: 1\nb: -7\nc: +5\nd: 0\ne: 0123\nf: 0x1F\ng: 0o17");
    EXPECT_EQ(doc.root().find(R("a"))->as_integer(), 1);
    EXPECT_EQ(doc.root().find(R("b"))->as_integer(), -7);
    EXPECT_EQ(doc.root().find(R("c"))->as_integer(), 5);
    EXPECT_EQ(doc.root().find(R("d"))->as_integer(), 0);
    EXPECT_EQ(doc.root().find(R("e"))->as_integer(), 123);
    EXPECT_EQ(doc.root().find(R("f"))->as_integer(), 31);
    EXPECT_EQ(doc.root().find(R("g"))->as_integer(), 15);
}

TEST(YamlReaderTest, Floats) {
    auto doc = read_ok("a: 3.14\nb: -2e5\nc: .5\nd: 6.\ne: 1E-3");
    EXPECT_DOUBLE_EQ(doc.root().find(R("a"))->as_float(), 3.14);
    EXPECT_DOUBLE_EQ(doc.root().find(R("b"))->as_float(), -2e5);
    EXPECT_DOUBLE_EQ(doc.root().find(R("c"))->as_float(), 0.5);
    EXPECT_DOUBLE_EQ(doc.root().find(R("d"))->as_float(), 6.0);
    EXPECT_DOUBLE_EQ(doc.root().find(R("e"))->as_float(), 1e-3);
}

TEST(YamlReaderTest, InfAndNan) {
    auto doc = read_ok("a: .inf\nb: -.inf\nc: .NaN");
    EXPECT_TRUE(doc.root().find(R("a"))->as_float() > 0 &&
                std::isinf(doc.root().find(R("a"))->as_float()));
    EXPECT_TRUE(doc.root().find(R("b"))->as_float() < 0 &&
                std::isinf(doc.root().find(R("b"))->as_float()));
    EXPECT_TRUE(std::isnan(doc.root().find(R("c"))->as_float()));
}

TEST(YamlReaderTest, IntegerOverflow) {
    EXPECT_TRUE(read_fails_with("v: 9223372036854775808", "out of range"));
    auto doc = read_ok("v: 9223372036854775807");
    EXPECT_EQ(doc.root().find(R("v"))->as_integer(), 9223372036854775807LL);
}

TEST(YamlReaderTest, NotNumbersStayStrings) {
    auto doc = read_ok("a: 1.2.3\nb: 0xZ\nc: 1_000\nd: -\ne: .");
    for (const char* k : {"a", "b", "c", "d", "e"}) {
        EXPECT_TRUE(doc.root().find(R(k))->is_string()) << k;
    }
}

TEST(YamlReaderTest, UrlValueKeepsColon) {
    auto doc = read_ok("url: http://example.com/a");
    EXPECT_EQ(doc.root().find(R("url"))->as_string(), R("http://example.com/a"));
}

TEST(YamlReaderTest, TimeLikeValueStaysString) {
    auto doc = read_ok("time: 12:30:45");
    EXPECT_EQ(doc.root().find(R("time"))->as_string(), R("12:30:45"));
}

TEST(YamlReaderTest, HashInsideScalarIsNotComment) {
    // '#' 前无空白 → 属于标量；有空白 → 注释。
    auto doc = read_ok("a: x#y\nb: x # comment");
    EXPECT_EQ(doc.root().find(R("a"))->as_string(), R("x#y"));
    EXPECT_EQ(doc.root().find(R("b"))->as_string(), R("x"));
}

// ============================================================================
// mapping
// ============================================================================

TEST(YamlReaderTest, FlatMappingPreservesOrder) {
    auto doc = read_ok("b: 2\na: 1\nc: 3");
    const auto& members = doc.root().as_mapping();
    ASSERT_EQ(members.size(), 3u);
    EXPECT_EQ(members[0].first, R("b"));
    EXPECT_EQ(members[1].first, R("a"));
    EXPECT_EQ(members[2].first, R("c"));
}

TEST(YamlReaderTest, NestedMappings) {
    auto doc = read_ok(
        "server:\n"
        "  http:\n"
        "    port: 8080\n"
        "    host: localhost\n");
    const auto* server = doc.root().find(R("server"));
    ASSERT_NE(server, nullptr);
    const auto* http = server->find(R("http"));
    ASSERT_NE(http, nullptr);
    EXPECT_EQ(http->find(R("port"))->as_integer(), 8080);
    EXPECT_EQ(http->find(R("host"))->as_string(), R("localhost"));
}

TEST(YamlReaderTest, MultiLevelDedent) {
    auto doc = read_ok(
        "a:\n"
        "  b:\n"
        "    c: 1\n"
        "top: 2\n");
    EXPECT_EQ(doc.root().find(R("a"))->find(R("b"))->find(R("c"))->as_integer(), 1);
    EXPECT_EQ(doc.root().find(R("top"))->as_integer(), 2);
}

TEST(YamlReaderTest, DuplicateKeyFails) {
    EXPECT_TRUE(read_fails_with("a: 1\na: 2", "duplicate"));
    EXPECT_TRUE(read_fails_with("m:\n  x: 1\n  x: 2", "duplicate"));
}

TEST(YamlReaderTest, ColonWithoutSpaceIsPlainScalar) {
    auto doc = read_ok("a:b");
    ASSERT_TRUE(doc.root().is_string());
    EXPECT_EQ(doc.root().as_string(), R("a:b"));
}

TEST(YamlReaderTest, QuotedKeys) {
    auto doc = read_ok("\"a b\": 1\n'x:y': 2");
    EXPECT_EQ(doc.root().find(R("a b"))->as_integer(), 1);
    EXPECT_EQ(doc.root().find(R("x:y"))->as_integer(), 2);
}

TEST(YamlReaderTest, KeyOnlyAtEofIsNull) {
    auto doc = read_ok("a: 1\nb:");
    EXPECT_TRUE(doc.root().find(R("b"))->is_null());
}

// ============================================================================
// sequence 与紧凑形式
// ============================================================================

TEST(YamlReaderTest, FlatSequence) {
    auto doc = read_ok("- a\n- b\n- c");
    ASSERT_TRUE(doc.root().is_sequence());
    ASSERT_EQ(doc.root().size(), 3u);
    EXPECT_EQ(doc.root().at(0).as_string(), R("a"));
    EXPECT_EQ(doc.root().at(2).as_string(), R("c"));
}

TEST(YamlReaderTest, TypedSequenceItems) {
    auto doc = read_ok("- 1\n- true\n- null\n- x");
    EXPECT_TRUE(doc.root().at(0).is_integer());
    EXPECT_TRUE(doc.root().at(1).is_boolean());
    EXPECT_TRUE(doc.root().at(2).is_null());
    EXPECT_TRUE(doc.root().at(3).is_string());
}

TEST(YamlReaderTest, NestedSequenceViaIndent) {
    auto doc = read_ok("-\n  - a\n  - b\n- c");
    ASSERT_EQ(doc.root().size(), 2u);
    ASSERT_TRUE(doc.root().at(0).is_sequence());
    EXPECT_EQ(doc.root().at(0).size(), 2u);
    EXPECT_EQ(doc.root().at(1).as_string(), R("c"));
}

TEST(YamlReaderTest, CompactNestedSequence) {
    auto doc = read_ok("- - a\n  - b");
    ASSERT_EQ(doc.root().size(), 1u);
    ASSERT_TRUE(doc.root().at(0).is_sequence());
    ASSERT_EQ(doc.root().at(0).size(), 2u);
    EXPECT_EQ(doc.root().at(0).at(1).as_string(), R("b"));
}

TEST(YamlReaderTest, CompactMappingInSequence) {
    auto doc = read_ok(
        "- name: a\n"
        "  port: 1\n"
        "- name: b\n"
        "  port: 2\n");
    ASSERT_EQ(doc.root().size(), 2u);
    EXPECT_EQ(doc.root().at(0).find(R("name"))->as_string(), R("a"));
    EXPECT_EQ(doc.root().at(0).find(R("port"))->as_integer(), 1);
    EXPECT_EQ(doc.root().at(1).find(R("name"))->as_string(), R("b"));
}

TEST(YamlReaderTest, LoneDashIsNullItem) {
    auto doc = read_ok("- 1\n-\n- 3");
    ASSERT_EQ(doc.root().size(), 3u);
    EXPECT_TRUE(doc.root().at(1).is_null());
}

TEST(YamlReaderTest, DashThenNestedBlockOnLaterLines) {
    auto doc = read_ok("-\n  a: 1\n  b: 2");
    ASSERT_EQ(doc.root().size(), 1u);
    EXPECT_EQ(doc.root().at(0).find(R("b"))->as_integer(), 2);
}

TEST(YamlReaderTest, ZeroIndentSequenceUnderKey) {
    auto doc = read_ok(
        "servers:\n"
        "- alpha\n"
        "- beta\n"
        "count: 2\n");
    const auto* servers = doc.root().find(R("servers"));
    ASSERT_NE(servers, nullptr);
    ASSERT_TRUE(servers->is_sequence());
    ASSERT_EQ(servers->size(), 2u);
    EXPECT_EQ(servers->at(1).as_string(), R("beta"));
    EXPECT_EQ(doc.root().find(R("count"))->as_integer(), 2);
}

TEST(YamlReaderTest, IndentedSequenceUnderKey) {
    auto doc = read_ok("list:\n  - 1\n  - 2");
    const auto* list = doc.root().find(R("list"));
    ASSERT_TRUE(list->is_sequence());
    EXPECT_EQ(list->at(0).as_integer(), 1);
}

// ============================================================================
// 引号字符串
// ============================================================================

TEST(YamlReaderTest, SingleQuoted) {
    auto doc = read_ok("a: 'it''s'\nb: 'x # not comment'\nc: 'k: v'");
    EXPECT_EQ(doc.root().find(R("a"))->as_string(), R("it's"));
    EXPECT_EQ(doc.root().find(R("b"))->as_string(), R("x # not comment"));
    EXPECT_EQ(doc.root().find(R("c"))->as_string(), R("k: v"));
}

TEST(YamlReaderTest, DoubleQuotedEscapes) {
    auto doc = read_ok("a: \"l1\\nl2\"\nb: \"tab\\there\"\nc: \"q\\\"q\"\nd: \"b\\\\s\"");
    EXPECT_EQ(doc.root().find(R("a"))->as_string(), R("l1\nl2"));
    EXPECT_EQ(doc.root().find(R("b"))->as_string(), R("tab\there"));
    EXPECT_EQ(doc.root().find(R("c"))->as_string(), R("q\"q"));
    EXPECT_EQ(doc.root().find(R("d"))->as_string(), R("b\\s"));
}

TEST(YamlReaderTest, UnicodeEscapes) {
    auto doc = read_ok("a: \"\\u00e9\"\nb: \"\\u4e2d\"\nc: \"\\U0001F600\"\nd: \"\\x41\"");
    EXPECT_EQ(doc.root().find(R("a"))->as_string(), R("\xC3\xA9"));          // é
    EXPECT_EQ(doc.root().find(R("b"))->as_string(), R("\xE4\xB8\xAD"));      // 中
    EXPECT_EQ(doc.root().find(R("c"))->as_string(), R("\xF0\x9F\x98\x80"));  // 😀
    EXPECT_EQ(doc.root().find(R("d"))->as_string(), R("A"));
}

TEST(YamlReaderTest, SurrogatePairEscape) {
    auto doc = read_ok("a: \"\\uD83D\\uDE00\"");
    EXPECT_EQ(doc.root().find(R("a"))->as_string(), R("\xF0\x9F\x98\x80"));  // 😀
}

TEST(YamlReaderTest, UnterminatedQuoteFails) {
    EXPECT_TRUE(read_fails_with("a: \"abc", "unterminated"));
    EXPECT_TRUE(read_fails_with("a: 'abc", "unterminated"));
}

TEST(YamlReaderTest, BadEscapeFails) {
    EXPECT_TRUE(read_fails_with("a: \"\\q\"", "escape"));
    EXPECT_TRUE(read_fails_with("a: \"\\uD800\"", "surrogate"));
}

// ============================================================================
// flow 集合
// ============================================================================

TEST(YamlReaderTest, FlowSequence) {
    auto doc = read_ok("v: [1, two, 3.0, null, true]");
    const auto* v = doc.root().find(R("v"));
    ASSERT_TRUE(v->is_sequence());
    ASSERT_EQ(v->size(), 5u);
    EXPECT_EQ(v->at(0).as_integer(), 1);
    EXPECT_EQ(v->at(1).as_string(), R("two"));
    EXPECT_DOUBLE_EQ(v->at(2).as_float(), 3.0);
    EXPECT_TRUE(v->at(3).is_null());
    EXPECT_TRUE(v->at(4).as_boolean());
}

TEST(YamlReaderTest, FlowMapping) {
    auto doc = read_ok("v: {a: 1, b: x}");
    const auto* v = doc.root().find(R("v"));
    ASSERT_TRUE(v->is_mapping());
    EXPECT_EQ(v->find(R("a"))->as_integer(), 1);
    EXPECT_EQ(v->find(R("b"))->as_string(), R("x"));
}

TEST(YamlReaderTest, NestedFlow) {
    auto doc = read_ok("v: [{a: [1, 2]}, []]");
    const auto* v = doc.root().find(R("v"));
    ASSERT_EQ(v->size(), 2u);
    EXPECT_EQ(v->at(0).find(R("a"))->at(1).as_integer(), 2);
    EXPECT_TRUE(v->at(1).is_sequence());
    EXPECT_EQ(v->at(1).size(), 0u);
}

TEST(YamlReaderTest, EmptyFlowCollections) {
    auto doc = read_ok("a: []\nb: {}");
    EXPECT_TRUE(doc.root().find(R("a"))->is_sequence());
    EXPECT_EQ(doc.root().find(R("a"))->size(), 0u);
    EXPECT_TRUE(doc.root().find(R("b"))->is_mapping());
    EXPECT_EQ(doc.root().find(R("b"))->as_mapping().size(), 0u);
}

TEST(YamlReaderTest, TrailingCommaFails) {
    EXPECT_TRUE(read_fails_with("v: [1, 2,]", "trailing comma"));
    EXPECT_TRUE(read_fails_with("v: {a: 1,}", "trailing comma"));
}

TEST(YamlReaderTest, UnclosedFlowFails) {
    EXPECT_TRUE(read_fails_with("v: [1, 2", "closed on the same line"));
    EXPECT_TRUE(read_fails_with("v: {a: 1", "closed on the same line"));
}

TEST(YamlReaderTest, DuplicateFlowKeyFails) {
    EXPECT_TRUE(read_fails_with("v: {a: 1, a: 2}", "duplicate"));
}

// ============================================================================
// 块标量 | / >
// ============================================================================

TEST(YamlReaderTest, LiteralBlockScalar) {
    auto doc = read_ok("v: |\n  line1\n  line2\n");
    EXPECT_EQ(doc.root().find(R("v"))->as_string(), R("line1\nline2\n"));
}

TEST(YamlReaderTest, LiteralChomping) {
    auto strip = read_ok("v: |-\n  text\n\n");
    EXPECT_EQ(strip.root().find(R("v"))->as_string(), R("text"));
    auto keep = read_ok("v: |+\n  text\n\nnext: 1");
    EXPECT_EQ(keep.root().find(R("v"))->as_string(), R("text\n\n"));
    EXPECT_EQ(keep.root().find(R("next"))->as_integer(), 1);
}

TEST(YamlReaderTest, FoldedBlockScalar) {
    // 单换行折叠为空格；空行保留为换行。
    auto doc = read_ok("v: >\n  a\n  b\n\n  c\n");
    EXPECT_EQ(doc.root().find(R("v"))->as_string(), R("a b\nc\n"));
}

TEST(YamlReaderTest, FoldedMoreIndentedKeepsBreaks) {
    auto doc = read_ok("v: >\n  a\n   b\n  c\n");
    EXPECT_EQ(doc.root().find(R("v"))->as_string(), R("a\n b\nc\n"));
}

TEST(YamlReaderTest, BlockScalarHashAndDashAreContent) {
    auto doc = read_ok("v: |\n  # not a comment\n  - not an item\n");
    EXPECT_EQ(doc.root().find(R("v"))->as_string(), R("# not a comment\n- not an item\n"));
}

TEST(YamlReaderTest, BlockScalarTerminatedByDedent) {
    auto doc = read_ok("v: |\n  text\nnext: 1");
    EXPECT_EQ(doc.root().find(R("v"))->as_string(), R("text\n"));
    EXPECT_EQ(doc.root().find(R("next"))->as_integer(), 1);
}

TEST(YamlReaderTest, ExplicitIndentIndicatorFails) {
    EXPECT_TRUE(read_fails_with("v: |2\n  text\n", "indent indicator"));
}

// ============================================================================
// 注释与布局
// ============================================================================

TEST(YamlReaderTest, CommentsEverywhere) {
    auto doc = read_ok(
        "# 头注释\n"
        "a: 1  # 尾注释\n"
        "\n"
        "# 中间注释\n"
        "list:\n"
        "  # 项前注释\n"
        "  - x\n"
        "  - y  # 项尾注释\n"
        "b: 2\n");
    EXPECT_EQ(doc.root().find(R("a"))->as_integer(), 1);
    EXPECT_EQ(doc.root().find(R("list"))->size(), 2u);
    EXPECT_EQ(doc.root().find(R("b"))->as_integer(), 2);
}

TEST(YamlReaderTest, CommentOnlyDocumentIsNull) {
    auto doc = read_ok("# only a comment\n\n");
    EXPECT_TRUE(doc.root().is_null());
    auto empty = read_ok("");
    EXPECT_TRUE(empty.root().is_null());
}

TEST(YamlReaderTest, RootScalarAndRootSequence) {
    auto scalar = read_ok("hello");
    EXPECT_EQ(scalar.root().as_string(), R("hello"));
    auto seq = read_ok("[1, 2]");
    ASSERT_TRUE(seq.root().is_sequence());
    EXPECT_EQ(seq.root().size(), 2u);
}

// ============================================================================
// 拒绝的特性（错误消息须明确）
// ============================================================================

TEST(YamlReaderTest, RejectsAnchorsAliasesTags) {
    EXPECT_TRUE(read_fails_with("a: &x 1", "anchors"));
    EXPECT_TRUE(read_fails_with("a: *x", "aliases"));
    EXPECT_TRUE(read_fails_with("a: !!str x", "tags"));
    EXPECT_TRUE(read_fails_with("a: !tag x", "tags"));
}

TEST(YamlReaderTest, RejectsDirectivesAndComplexKeys) {
    EXPECT_TRUE(read_fails_with("%YAML 1.2\n---\na: 1", "directives"));
    EXPECT_TRUE(read_fails_with("? complex\n: value", "complex mapping keys"));
}

TEST(YamlReaderTest, RejectsMultiDocument) {
    EXPECT_TRUE(read_fails_with("a: 1\n---\nb: 2", "multi-document"));
    EXPECT_TRUE(read_fails_with("a: 1\n...", "multi-document"));
}

TEST(YamlReaderTest, LeadingDocumentStartAllowed) {
    auto doc = read_ok("---\na: 1\n");
    EXPECT_EQ(doc.root().find(R("a"))->as_integer(), 1);
}

TEST(YamlReaderTest, RejectsTabIndentation) {
    EXPECT_TRUE(read_fails_with("a:\n\tb: 1", "tab"));
}

TEST(YamlReaderTest, RejectsNestedMappingValueOnSameLine) {
    EXPECT_TRUE(read_fails_with("key: a: b", "mapping value not allowed"));
}

TEST(YamlReaderTest, RejectsMultiLinePlainScalar) {
    EXPECT_TRUE(read_fails_with("key:\n  a\n  b", "multi-line plain scalars"));
}

TEST(YamlReaderTest, RejectsDashInsideMapping) {
    EXPECT_TRUE(read_fails_with("a: 1\n- x", "sequence entry"));
}

// ============================================================================
// BOM / CRLF / 无尾换行
// ============================================================================

TEST(YamlReaderTest, BomIsSkipped) {
    auto doc = read_ok("\xEF\xBB\xBFkey: 1");
    EXPECT_EQ(doc.root().find(R("key"))->as_integer(), 1);
}

TEST(YamlReaderTest, CrlfDocument) {
    auto doc = read_ok("a: 1\r\nlist:\r\n  - x\r\nv: |\r\n  text\r\nb: 2\r\n");
    EXPECT_EQ(doc.root().find(R("a"))->as_integer(), 1);
    EXPECT_EQ(doc.root().find(R("list"))->size(), 1u);
    EXPECT_EQ(doc.root().find(R("v"))->as_string(), R("text\n"));
    EXPECT_EQ(doc.root().find(R("b"))->as_integer(), 2);
}

TEST(YamlReaderTest, NoTrailingNewline) {
    auto doc = read_ok("a: 1\nb: 2");
    EXPECT_EQ(doc.root().find(R("b"))->as_integer(), 2);
}

// ============================================================================
// 错误位置
// ============================================================================

TEST(YamlReaderTest, ErrorLocationIsPopulated) {
    auto result = YamlReader::read(R("a: 1\nb: [1,\n"));
    ASSERT_TRUE(result.is_err());
    const auto err = std::move(result).unwrap_err();
    EXPECT_EQ(err.location.line, 2u);
    EXPECT_GT(err.location.column, 1u);
}

// \xXX 是码点转义：XX >= 0x80 此前按原始单字节追加产生非法 UTF-8，
// 经 arena intern 静默变空串（字符串无声丢失）。现按码点编码为两字节 UTF-8。
TEST(YamlReaderTest, HighHexEscapeEncodesCodePoint) {
    auto doc = read_ok("a: \"\\xE9\"");
    EXPECT_EQ(doc.root().find(R("a"))->as_string(), R("\xC3\xA9"));  // e with acute
}
