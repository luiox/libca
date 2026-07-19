#include "libca/json/json.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace ca::json;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// 读入成功并取走 root。
JsonValue read_ok(const char* text) {
    auto result = JsonReader::read(R(text));
    EXPECT_TRUE(result.is_ok());
    return std::move(result).unwrap();
}

std::string to_std(const Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
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
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_null())), "null");
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_bool(true))), "true");
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_bool(false))), "false");
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_int(42))), "42");
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_int(-1))), "-1");
}

TEST(JsonWriterTest, WritesString) {
    EXPECT_EQ(to_std(JsonWriter::write(
        JsonValue::make_string(Utf8String::from_cstr("hello")))), "\"hello\"");
}

TEST(JsonWriterTest, EscapesStringSpecialChars) {
    // " 和 \ 必须转义
    auto v = JsonValue::make_string(Utf8String::from_cstr("a\"b\\c"));
    EXPECT_EQ(to_std(JsonWriter::write(v)), "\"a\\\"b\\\\c\"");
}

TEST(JsonWriterTest, EscapesControlCharsAsUnicode) {
    // 换行、tab 转义
    auto v = JsonValue::make_string(Utf8String::from_cstr("\n\t"));
    EXPECT_EQ(to_std(JsonWriter::write(v)), "\"\\n\\t\"");
}

TEST(JsonWriterTest, PreservesUtf8ByDefault) {
    // 非 ASCII 默认原样输出（已是合法 UTF-8 字节）
    auto v = JsonValue::make_string(Utf8String::from_cstr("\xE4\xB8\xAD"));  // "中"
    EXPECT_EQ(to_std(JsonWriter::write(v)), "\"\xE4\xB8\xAD\"");
}

TEST(JsonWriterTest, EscapesNonAsciiWhenOptionSet) {
    JsonWriterOptions opts;
    opts.ensure_ascii = true;
    auto v = JsonValue::make_string(Utf8String::from_cstr("\xE4\xB8\xAD"));  // "中" = U+4E2D
    EXPECT_EQ(to_std(JsonWriter::write(v, opts)), "\"\\u4e2d\"");
}

TEST(JsonWriterTest, EscapesAstralPlaneAsSurrogatePair) {
    JsonWriterOptions opts;
    opts.ensure_ascii = true;
    auto v = JsonValue::make_string(Utf8String::from_cstr("\xF0\x9F\x98\x80"));  // 😀 U+1F600
    EXPECT_EQ(to_std(JsonWriter::write(v, opts)), "\"\\ud83d\\ude00\"");
}

TEST(JsonWriterTest, WritesEmptyArrayAndObject) {
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_array())), "[]");
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_object())), "{}");
}

TEST(JsonWriterTest, WritesNestedCompact) {
    JsonValue obj = JsonValue::make_object();
    obj.set(Utf8String::from_cstr("arr"), JsonValue::make_array());
    obj.find(R("arr"))->append(JsonValue::make_int(1));
    obj.find(R("arr"))->append(JsonValue::make_int(2));
    obj.set(Utf8String::from_cstr("k"), JsonValue::make_string(Utf8String::from_cstr("v")));
    EXPECT_EQ(to_std(JsonWriter::write(obj)), "{\"arr\":[1,2],\"k\":\"v\"}");
}

// ============================================================================
// pretty 输出
// ============================================================================

TEST(JsonWriterTest, PrettyIndents) {
    JsonValue obj = JsonValue::make_object();
    obj.set(Utf8String::from_cstr("a"), JsonValue::make_int(1));
    obj.set(Utf8String::from_cstr("b"), JsonValue::make_int(2));
    JsonWriterOptions opts;
    opts.pretty = true;
    opts.indent = 2;
    const std::string expected =
        "{\n"
        "  \"a\": 1,\n"
        "  \"b\": 2\n"
        "}";
    EXPECT_EQ(to_std(JsonWriter::write(obj, opts)), expected);
}

TEST(JsonWriterTest, PrettyEmptyContainersStayCompact) {
    JsonWriterOptions opts;
    opts.pretty = true;
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_array(), opts)), "[]");
    EXPECT_EQ(to_std(JsonWriter::write(JsonValue::make_object(), opts)), "{}");
}

// ============================================================================
// round-trip
// ============================================================================

TEST(JsonWriterTest, RoundTripScalars) {
    const char* cases[] = {"null", "true", "false", "42", "-7", "3.14", "\"hi\"", "[]", "{}"};
    for (const char* c : cases) {
        auto v = read_ok(c);
        std::string written = to_std(JsonWriter::write(v));
        auto v2 = read_ok(written.c_str());
        EXPECT_TRUE(equal(v, v2)) << "round-trip failed for: " << c;
    }
}

TEST(JsonWriterTest, RoundTripComplex) {
    const char* src =
        "{\"name\":\"Alice\",\"age\":30,\"tags\":[\"a\",\"b\"],\"active\":true,\"score\":9.5}";
    auto v1 = read_ok(src);
    std::string written = to_std(JsonWriter::write(v1));
    auto v2 = read_ok(written.c_str());
    EXPECT_TRUE(equal(v1, v2));
}

TEST(JsonWriterTest, RoundTripPreservesNumbers) {
    // 整数仍是整数，浮点仍是浮点
    auto vi = read_ok("12345");
    EXPECT_EQ(to_std(JsonWriter::write(vi)), "12345");
    auto vf = read_ok("1.5");
    // 浮点经 %.17g 输出后可能形如 "1.5"，round-trip 仍应是 float
    auto vf2 = read_ok(to_std(JsonWriter::write(vf)).c_str());
    EXPECT_TRUE(vf2.is_float());
}
