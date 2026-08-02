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

std::string S(const Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.byte_length());
}

// 递归比较两棵 DOM（浮点 NaN 视为相等）。
bool dom_equal(const YamlValue& a, const YamlValue& b) {
    if (a.type() != b.type()) return false;
    switch (a.type()) {
        case YamlType::Null:    return true;
        case YamlType::Boolean: return a.as_boolean() == b.as_boolean();
        case YamlType::Integer: return a.as_integer() == b.as_integer();
        case YamlType::Float: {
            const f64 x = a.as_float(), y = b.as_float();
            if (std::isnan(x) && std::isnan(y)) return true;
            return x == y;
        }
        case YamlType::String:  return a.as_string() == b.as_string();
        case YamlType::Sequence: {
            if (a.size() != b.size()) return false;
            for (usize i = 0; i < a.size(); ++i)
                if (!dom_equal(a.at(i), b.at(i))) return false;
            return true;
        }
        case YamlType::Mapping: {
            const auto& am = a.as_mapping();
            const auto& bm = b.as_mapping();
            if (am.size() != bm.size()) return false;
            for (const auto& m : am) {
                const auto* bv = b.find(m.first);
                if (bv == nullptr || !dom_equal(m.second, *bv)) return false;
            }
            return true;
        }
    }
    return false;
}

// write(read(text)) 后再 read，验证 DOM 与首次解析相等。
void expect_roundtrip(const char* text) {
    auto first = YamlReader::read(R(text));
    ASSERT_TRUE(first.is_ok()) << "initial parse failed";
    auto doc1 = std::move(first).unwrap();
    Utf8String out = YamlWriter::write(doc1);
    auto second = YamlReader::read(out.ref());
    ASSERT_TRUE(second.is_ok()) << "re-parse failed for output:\n" << S(out);
    auto doc2 = std::move(second).unwrap();
    EXPECT_TRUE(dom_equal(doc1.root(), doc2.root())) << "roundtrip mismatch. output:\n" << S(out);
}

}  // namespace

// ============================================================================
// 标量格式化
// ============================================================================

TEST(YamlWriterTest, ScalarFormatting) {
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    auto& m = doc.root();
    m.set(doc.arena().intern("i"), YamlValue::make_integer(42));
    m.set(doc.arena().intern("f"), YamlValue::make_float(3.5));
    m.set(doc.arena().intern("b"), YamlValue::make_boolean(true));
    m.set(doc.arena().intern("n"), YamlValue::make_null());
    m.set(doc.arena().intern("inf"), YamlValue::make_float(std::numeric_limits<f64>::infinity()));
    m.set(doc.arena().intern("nan"), YamlValue::make_float(std::numeric_limits<f64>::quiet_NaN()));

    const std::string out = S(YamlWriter::write(doc));
    EXPECT_NE(out.find("i: 42"), std::string::npos);
    EXPECT_NE(out.find("b: true"), std::string::npos);
    EXPECT_NE(out.find("n: null"), std::string::npos);
    EXPECT_NE(out.find("inf: .inf"), std::string::npos);
    EXPECT_NE(out.find("nan: .nan"), std::string::npos);
}

TEST(YamlWriterTest, FloatKeepsDecimalForm) {
    YamlDocument doc;
    doc.root() = YamlValue::make_float(1.0);
    const std::string out = S(YamlWriter::write(doc));
    // 必须含 '.' 或 'e'，否则读回会变整数。
    EXPECT_TRUE(out.find('.') != std::string::npos || out.find('e') != std::string::npos);
}

// ============================================================================
// 字符串加引号判定
// ============================================================================

TEST(YamlWriterTest, StringsThatNeedQuoting) {
    // 这些字符串不加引号会被读成别的类型或破坏结构。
    const char* need[] = {"true", "false", "null", "~", "123", "1.5", "1e3",
                          ".inf", ".nan", "0x10", "", " x", "x ", "-a", "a: b", "a #b", "#c"};
    for (const char* s : need) {
        YamlDocument doc;
        doc.root() = YamlValue::make_mapping();
        doc.root().set(doc.arena().intern("k"), YamlValue::make_string(doc.arena().intern(s)));
        Utf8String out = YamlWriter::write(doc);
        // 读回后 k 必须仍是字符串且内容相等。
        auto back = YamlReader::read(out.ref());
        ASSERT_TRUE(back.is_ok()) << "reparse failed for [" << s << "] output: " << S(out);
        auto bdoc = std::move(back).unwrap();
        const auto* v = bdoc.root().find(R("k"));
        ASSERT_NE(v, nullptr) << s;
        ASSERT_TRUE(v->is_string()) << "[" << s << "] became non-string: " << S(out);
        EXPECT_EQ(v->as_string(), R(s)) << s;
    }
}

TEST(YamlWriterTest, StringsThatStayPlain) {
    const char* plain[] = {"hello", "a#b", "a,b", "12:30", "hello world", "under_score"};
    for (const char* s : plain) {
        YamlDocument doc;
        doc.root() = YamlValue::make_mapping();
        doc.root().set(doc.arena().intern("k"), YamlValue::make_string(doc.arena().intern(s)));
        const std::string out = S(YamlWriter::write(doc));
        // 不应带引号。
        EXPECT_EQ(out.find('\''), std::string::npos) << "[" << s << "] got quoted: " << out;
        EXPECT_EQ(out.find('"'), std::string::npos) << "[" << s << "] got quoted: " << out;
    }
}

TEST(YamlWriterTest, ControlCharForcesDoubleQuote) {
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("k"), YamlValue::make_string(doc.arena().intern("a\tb")));
    const std::string out = S(YamlWriter::write(doc));
    EXPECT_NE(out.find('"'), std::string::npos);
    EXPECT_NE(out.find("\\t"), std::string::npos);
}

// ============================================================================
// 多行字符串 → 块标量
// ============================================================================

TEST(YamlWriterTest, MultilineStringAsBlockScalar) {
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("text"),
                   YamlValue::make_string(doc.arena().intern("line1\nline2\n")));
    const std::string out = S(YamlWriter::write(doc));
    EXPECT_NE(out.find("text: |"), std::string::npos) << out;
    expect_roundtrip("text: |\n  line1\n  line2\n");
}

TEST(YamlWriterTest, MultilineChompingSelection) {
    // strip（无尾换行）→ |-, keep（多尾换行）→ |+
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("a"), YamlValue::make_string(doc.arena().intern("x\ny")));
    doc.root().set(doc.arena().intern("b"), YamlValue::make_string(doc.arena().intern("x\n\n")));
    const std::string out = S(YamlWriter::write(doc));
    EXPECT_NE(out.find("a: |-"), std::string::npos) << out;
    EXPECT_NE(out.find("b: |+"), std::string::npos) << out;
}

TEST(YamlWriterTest, LeadingSpaceLineFallsBackToDoubleQuote) {
    // 首行以空格开头的多行串块标量不可保真 → 回退双引号。
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("k"),
                   YamlValue::make_string(doc.arena().intern("  indented\nnext")));
    const std::string out = S(YamlWriter::write(doc));
    EXPECT_NE(out.find('"'), std::string::npos) << out;
    EXPECT_EQ(out.find("k: |"), std::string::npos) << out;
}

// ============================================================================
// 嵌套布局
// ============================================================================

TEST(YamlWriterTest, NestedLayoutGolden) {
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    auto server = YamlValue::make_mapping();
    server.set(doc.arena().intern("host"), YamlValue::make_string(doc.arena().intern("localhost")));
    server.set(doc.arena().intern("port"), YamlValue::make_integer(8080));
    doc.root().set(doc.arena().intern("server"), std::move(server));

    const std::string out = S(YamlWriter::write(doc));
    EXPECT_EQ(out,
              "server:\n"
              "  host: localhost\n"
              "  port: 8080\n");
}

TEST(YamlWriterTest, SequenceOfMappingsCompact) {
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    auto seq = YamlValue::make_sequence();
    auto e0 = YamlValue::make_mapping();
    e0.set(doc.arena().intern("name"), YamlValue::make_string(doc.arena().intern("a")));
    e0.set(doc.arena().intern("port"), YamlValue::make_integer(1));
    seq.append(std::move(e0));
    doc.root().set(doc.arena().intern("items"), std::move(seq));

    const std::string out = S(YamlWriter::write(doc));
    EXPECT_EQ(out,
              "items:\n"
              "  - name: a\n"
              "    port: 1\n");
}

TEST(YamlWriterTest, EmptyCollections) {
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("s"), YamlValue::make_sequence());
    doc.root().set(doc.arena().intern("m"), YamlValue::make_mapping());
    const std::string out = S(YamlWriter::write(doc));
    EXPECT_NE(out.find("s: []"), std::string::npos) << out;
    EXPECT_NE(out.find("m: {}"), std::string::npos) << out;
}

// ============================================================================
// write_file + read_file
// ============================================================================

TEST(YamlWriterTest, WriteFileReadFileRoundtrip) {
    const std::string path = "build/libca_yaml_roundtrip_test.yaml";
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("key"), YamlValue::make_string(doc.arena().intern("value")));

    ASSERT_TRUE(YamlWriter::write_file(R(path.c_str()), doc).is_ok());
    auto loaded = YamlReader::read_file(R(path.c_str()));
    ASSERT_TRUE(loaded.is_ok());
    auto ldoc = std::move(loaded).unwrap();
    EXPECT_EQ(ldoc.root().find(R("key"))->as_string(), R("value"));
    std::remove(path.c_str());
}

// ============================================================================
// write → read → dom_equal 往返
// ============================================================================

TEST(YamlWriterTest, RoundtripMixedDocument) {
    expect_roundtrip(
        "name: libca\n"
        "version: 3\n"
        "enabled: true\n"
        "ratio: 0.75\n"
        "tags:\n"
        "  - core\n"
        "  - yaml\n"
        "server:\n"
        "  host: localhost\n"
        "  port: 8080\n"
        "  aliases: [a, b, c]\n");
}

TEST(YamlWriterTest, RoundtripAllScalarTypes) {
    expect_roundtrip(
        "i: 42\n"
        "neg: -7\n"
        "f: 3.14\n"
        "big: 1e10\n"
        "t: true\n"
        "f2: false\n"
        "nothing: null\n"
        "s: hello world\n"
        "inf: .inf\n");
}

TEST(YamlWriterTest, RoundtripAdversarialStrings) {
    // 这些字符串写出时必须加引号才能读回原值。
    expect_roundtrip(
        "a: 'true'\n"
        "b: '123'\n"
        "c: '~'\n"
        "d: 'a: b'\n"
        "e: '#hash'\n"
        "f: ' spaced '\n"
        "g: 'x #y'\n"
        "h: plain\n");
}

TEST(YamlWriterTest, RoundtripMultilineString) {
    expect_roundtrip("text: |\n  first\n  second\n  third\n");
}

TEST(YamlWriterTest, RoundtripNestedSequences) {
    expect_roundtrip(
        "matrix:\n"
        "  - [1, 2, 3]\n"
        "  - [4, 5, 6]\n"
        "nested:\n"
        "  -\n"
        "    - a\n"
        "    - b\n");
}
