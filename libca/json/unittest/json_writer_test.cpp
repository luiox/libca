#include "libca/json/json.hpp"

#include <gtest/gtest.h>

#include <filesystem>

#include <cmath>
#include <limits>
#include <string>

using namespace ca::json;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// 读入成功并取走 document。
JsonDocument read_ok(const char* text) {
    auto result = JsonReader::read(R(text));
    EXPECT_TRUE(result.is_ok());
    return std::move(result).unwrap();
}

std::string to_std(const Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

// 把 JsonValue 包成临时 JsonDocument 供 writer 使用。
// 注意：返回的 document 必须由调用方持有，writer 期间不可销毁。
JsonDocument wrap(JsonValue v) {
    JsonDocument doc;
    doc.root() = std::move(v);
    return doc;
}

// 递归比较两个 JsonValue 是否结构相等（用于 round-trip 验证）。
bool equal(const JsonValue& a, const JsonValue& b) {
    if (a.type() != b.type()) return false;
    switch (a.type()) {
        case JsonType::Null:   return true;
        case JsonType::Bool:   return a.as_bool() == b.as_bool();
        case JsonType::Int:    return a.as_int() == b.as_int();
        case JsonType::Float:  return a.as_float() == b.as_float();
        case JsonType::String: return a.as_string() == b.as_string();
        case JsonType::Array: {
            if (a.size() != b.size()) return false;
            for (ca::usize i = 0; i < a.size(); ++i) {
                if (!equal(a.at(i), b.at(i))) return false;
            }
            return true;
        }
        case JsonType::Object: {
            const auto& oa = a.as_object();
            const auto& ob = b.as_object();
            if (oa.size() != ob.size()) return false;
            for (ca::usize i = 0; i < oa.size(); ++i) {
                // 保序：同位置 key+value 必须一致
                if (oa[i].first != ob[i].first) return false;
                if (!equal(oa[i].second, ob[i].second)) return false;
            }
            return true;
        }
    }
    return false;
}

}  // namespace

// ============================================================================
// 基本序列化
// ============================================================================

TEST(JsonWriterTest, WritesScalars) {
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_null()))), "null");
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_bool(true)))), "true");
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_bool(false)))), "false");
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_int(42)))), "42");
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_int(-1)))), "-1");
}

TEST(JsonWriterTest, SerializesNaNAndInfinityAsNull) {
    // RFC 8259 不允许 NaN/Infinity；序列化为 null 保证输出仍是合法 JSON。
    EXPECT_EQ(to_std(JsonWriter::write(
        wrap(JsonValue::make_float(std::numeric_limits<ca::f64>::quiet_NaN())))), "null");
    EXPECT_EQ(to_std(JsonWriter::write(
        wrap(JsonValue::make_float(std::numeric_limits<ca::f64>::infinity())))), "null");
    EXPECT_EQ(to_std(JsonWriter::write(
        wrap(JsonValue::make_float(-std::numeric_limits<ca::f64>::infinity())))), "null");
}

TEST(JsonWriterTest, WritesString) {
    JsonDocument doc;
    auto v = JsonValue::make_string(doc.arena().intern(Utf8String::from_cstr("hello")));
    EXPECT_EQ(to_std(JsonWriter::write(wrap(std::move(v)))), "\"hello\"");
}

TEST(JsonWriterTest, EscapesStringSpecialChars) {
    JsonDocument doc;
    // " 和 \ 必须转义
    auto v = JsonValue::make_string(doc.arena().intern(Utf8String::from_cstr("a\"b\\c")));
    EXPECT_EQ(to_std(JsonWriter::write(wrap(std::move(v)))), "\"a\\\"b\\\\c\"");
}

TEST(JsonWriterTest, EscapesControlCharsAsUnicode) {
    JsonDocument doc;
    // 换行、tab 转义
    auto v = JsonValue::make_string(doc.arena().intern(Utf8String::from_cstr("\n\t")));
    EXPECT_EQ(to_std(JsonWriter::write(wrap(std::move(v)))), "\"\\n\\t\"");
}

TEST(JsonWriterTest, PreservesUtf8ByDefault) {
    JsonDocument doc;
    // 非 ASCII 默认原样输出（已是合法 UTF-8 字节）
    auto v = JsonValue::make_string(doc.arena().intern(Utf8String::from_cstr("\xE4\xB8\xAD")));  // "中"
    EXPECT_EQ(to_std(JsonWriter::write(wrap(std::move(v)))), "\"\xE4\xB8\xAD\"");
}

TEST(JsonWriterTest, EscapesNonAsciiWhenOptionSet) {
    JsonWriterOptions opts;
    opts.ensure_ascii = true;
    JsonDocument doc;
    auto v = JsonValue::make_string(doc.arena().intern(Utf8String::from_cstr("\xE4\xB8\xAD")));  // "中" = U+4E2D
    EXPECT_EQ(to_std(JsonWriter::write(wrap(std::move(v)), opts)), "\"\\u4e2d\"");
}

TEST(JsonWriterTest, EscapesAstralPlaneAsSurrogatePair) {
    JsonWriterOptions opts;
    opts.ensure_ascii = true;
    JsonDocument doc;
    auto v = JsonValue::make_string(doc.arena().intern(Utf8String::from_cstr("\xF0\x9F\x98\x80")));  // 😀 U+1F600
    EXPECT_EQ(to_std(JsonWriter::write(wrap(std::move(v)), opts)), "\"\\ud83d\\ude00\"");
}

TEST(JsonWriterTest, WritesEmptyArrayAndObject) {
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_array()))), "[]");
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_object()))), "{}");
}

TEST(JsonWriterTest, WritesNestedCompact) {
    JsonDocument doc;
    auto& arena = doc.arena();
    JsonValue obj = JsonValue::make_object();
    obj.set(arena.intern(Utf8String::from_cstr("arr")), JsonValue::make_array());
    obj.find(R("arr"))->append(JsonValue::make_int(1));
    obj.find(R("arr"))->append(JsonValue::make_int(2));
    obj.set(arena.intern(Utf8String::from_cstr("k")),
           JsonValue::make_string(arena.intern(Utf8String::from_cstr("v"))));
    doc.root() = std::move(obj);
    EXPECT_EQ(to_std(JsonWriter::write(doc)), "{\"arr\":[1,2],\"k\":\"v\"}");
}

// ============================================================================
// pretty 输出
// ============================================================================

TEST(JsonWriterTest, PrettyIndents) {
    JsonDocument doc;
    auto& arena = doc.arena();
    JsonValue obj = JsonValue::make_object();
    obj.set(arena.intern(Utf8String::from_cstr("a")), JsonValue::make_int(1));
    obj.set(arena.intern(Utf8String::from_cstr("b")), JsonValue::make_int(2));
    doc.root() = std::move(obj);
    JsonWriterOptions opts;
    opts.pretty = true;
    opts.indent = 2;
    const std::string expected =
        "{\n"
        "  \"a\": 1,\n"
        "  \"b\": 2\n"
        "}";
    EXPECT_EQ(to_std(JsonWriter::write(doc, opts)), expected);
}

TEST(JsonWriterTest, PrettyEmptyContainersStayCompact) {
    JsonWriterOptions opts;
    opts.pretty = true;
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_array()), opts)), "[]");
    EXPECT_EQ(to_std(JsonWriter::write(wrap(JsonValue::make_object()), opts)), "{}");
}

// ============================================================================
// round-trip
// ============================================================================

TEST(JsonWriterTest, RoundTripScalars) {
    const char* cases[] = {"null", "true", "false", "42", "-7", "3.14", "\"hi\"", "[]", "{}"};
    for (const char* c : cases) {
        auto doc = read_ok(c);
        std::string written = to_std(JsonWriter::write(doc));
        auto doc2 = read_ok(written.c_str());
        EXPECT_TRUE(equal(doc.root(), doc2.root())) << "round-trip failed for: " << c;
    }
}

TEST(JsonWriterTest, RoundTripComplex) {
    const char* src =
        "{\"name\":\"Alice\",\"age\":30,\"tags\":[\"a\",\"b\"],\"active\":true,\"score\":9.5}";
    auto doc1 = read_ok(src);
    std::string written = to_std(JsonWriter::write(doc1));
    auto doc2 = read_ok(written.c_str());
    EXPECT_TRUE(equal(doc1.root(), doc2.root()));
}

TEST(JsonWriterTest, RoundTripPreservesNumbers) {
    // 整数仍是整数，浮点仍是浮点
    auto doc = read_ok("12345");
    EXPECT_EQ(to_std(JsonWriter::write(doc)), "12345");
    auto doci = read_ok("1.5");
    // 浮点经 %.17g 输出后可能形如 "1.5"，round-trip 仍应是 float
    auto doc2 = read_ok(to_std(JsonWriter::write(doci)).c_str());
    EXPECT_TRUE(doc2.root().is_float());
}

// 整数形态的 float（2.0）此前经 %.17g 写成 "2"，读回变 Int；
// -0.0 写成 "-0" 读回变 Int 0（符号与类型双丢）。两者都需补 ".0" 保持 Float。
TEST(JsonWriterTest, RoundTripPreservesFloatTypeForIntegralValuedFloats) {
    auto doc = read_ok("2.0");
    const std::string out = to_std(JsonWriter::write(doc));
    EXPECT_EQ(out, "2.0");
    auto doc2 = read_ok(out.c_str());
    ASSERT_TRUE(doc2.root().is_float());
    EXPECT_DOUBLE_EQ(doc2.root().as_float(), 2.0);

    auto docn = read_ok("-0.0");
    const std::string outn = to_std(JsonWriter::write(docn));
    EXPECT_EQ(outn, "-0.0");
    auto docn2 = read_ok(outn.c_str());
    ASSERT_TRUE(docn2.root().is_float());
    EXPECT_TRUE(std::signbit(docn2.root().as_float()));
}

// read_file/write_file 经 u8path 处理路径：非 ASCII（UTF-8）路径在 Windows ACP
// 环境下按窄字符打开会落到错误文件名，这里做真实文件往返回归。
TEST(JsonWriterTest, WriteFileReadFileUtf8PathRoundtrip) {
    const std::string path = "build/libca_json_utf8路径回归.json";
    JsonDocument doc = read_ok("{\"key\": \"value\"}");
    ASSERT_TRUE(JsonWriter::write_file(R(path.c_str()), doc).is_ok());
    auto loaded = JsonReader::read_file(R(path.c_str()));
    ASSERT_TRUE(loaded.is_ok());
    JsonDocument reloaded = std::move(loaded).unwrap();
    EXPECT_TRUE(equal(doc.root(), reloaded.root()));
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(path), ec);
}
