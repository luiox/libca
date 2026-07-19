#include "libca/json/json.hpp"

#include <gtest/gtest.h>

#include <utility>

using namespace ca;
using namespace ca::json;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {
// 把字符串字面量安全转成 Utf8StringRef（O(n) 算码点数，测试用无所谓）。
Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// 解析并断言成功；返回 root（移动取出）。
JsonValue read_ok(const char* text, JsonReaderOptions opts = {}) {
    auto result = JsonReader::read(R(text), opts);
    EXPECT_TRUE(result.is_ok()) << "expected parse success for: " << text;
    return std::move(result).unwrap();
}
}  // namespace

// ============================================================================
// 字面量解析
// ============================================================================

TEST(JsonReaderTest, ParsesNull) {
    auto v = read_ok("null");
    ASSERT_TRUE(v.is_null());
    EXPECT_EQ(v.type(), JsonType::Null);
}

TEST(JsonReaderTest, ParsesBool) {
    EXPECT_TRUE(read_ok("true").as_bool());
    EXPECT_FALSE(read_ok("false").as_bool());
}

TEST(JsonReaderTest, ParsesInt) {
    auto v = read_ok("42");
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(JsonReaderTest, ParsesNegativeInt) {
    auto v = read_ok("-1");
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -1);
}

TEST(JsonReaderTest, ParsesMaxI64) {
    auto v = read_ok("9223372036854775807");
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 9223372036854775807LL);
}

TEST(JsonReaderTest, ParsesMinI64) {
    auto v = read_ok("-9223372036854775808");
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), (-9223372036854775807LL - 1));
}

TEST(JsonReaderTest, I64OverflowDowngradesToFloat) {
    // 超过 i64 上限，应降级为 float
    auto v = read_ok("9223372036854775808");  // i64::MAX + 1
    ASSERT_TRUE(v.is_float()) << "expected float for overflowing integer";
    EXPECT_GT(v.as_float(), 9.2e18);
}

TEST(JsonReaderTest, ParsesFloat) {
    auto v = read_ok("3.14");
    ASSERT_TRUE(v.is_float());
    EXPECT_DOUBLE_EQ(v.as_float(), 3.14);
}

TEST(JsonReaderTest, ParsesFloatWithExponent) {
    EXPECT_DOUBLE_EQ(read_ok("1e10").as_float(), 1e10);
    EXPECT_DOUBLE_EQ(read_ok("1.5E-3").as_float(), 1.5e-3);
    EXPECT_DOUBLE_EQ(read_ok("-0.0").as_float(), -0.0);
}

TEST(JsonReaderTest, RejectsLeadingZeros) {
    EXPECT_TRUE(JsonReader::read(R("01")).is_err());
    EXPECT_TRUE(JsonReader::read(R("00")).is_err());
}

TEST(JsonReaderTest, RejectsTrailingDotOrPrefixDot) {
    EXPECT_TRUE(JsonReader::read(R("1.")).is_err());
    EXPECT_TRUE(JsonReader::read(R(".5")).is_err());
}

TEST(JsonReaderTest, RejectsBadLiterals) {
    EXPECT_TRUE(JsonReader::read(R("tru")).is_err());
    EXPECT_TRUE(JsonReader::read(R("fals")).is_err());
    EXPECT_TRUE(JsonReader::read(R("nul")).is_err());
    EXPECT_TRUE(JsonReader::read(R("truex")).is_err());
}

TEST(JsonReaderTest, RejectsDanglingMinus) {
    EXPECT_TRUE(JsonReader::read(R("-")).is_err());
    EXPECT_TRUE(JsonReader::read(R("--1")).is_err());
}

// ============================================================================
// 字符串解析
// ============================================================================

TEST(JsonReaderTest, ParsesEmptyString) {
    auto v = read_ok("\"\"");
    ASSERT_TRUE(v.is_string());
    EXPECT_TRUE(v.as_string().is_empty());
}

TEST(JsonReaderTest, ParsesAsciiString) {
    auto v = read_ok("\"hello\"");
    EXPECT_EQ(v.as_string(), Utf8String::from_cstr("hello"));
}

TEST(JsonReaderTest, ParsesUtf8String) {
    // 中文 + emoji（emoji 在 JSON 里通常以 surrogate pair 写，但合法 UTF-8 字节也直接接受）
    auto v = read_ok("\"\\u4e2d\\u6587\"");
    EXPECT_EQ(v.as_string(), Utf8String::from_cstr("中文"));
}

TEST(JsonReaderTest, ParsesSurrogatePair) {
    // U+1F600 😀 = \uD83D\uDE00
    auto v = read_ok("\"\\uD83D\\uDE00\"");
    EXPECT_EQ(v.as_string(), Utf8String::from_cstr("\xF0\x9F\x98\x80"));
}

TEST(JsonReaderTest, RejectsDanglingHighSurrogate) {
    EXPECT_TRUE(JsonReader::read(R("\"\\uD83D\"")).is_err());
}

TEST(JsonReaderTest, RejectsUnexpectedLowSurrogate) {
    EXPECT_TRUE(JsonReader::read(R("\"\\uDE00\"")).is_err());
}

TEST(JsonReaderTest, ParsesAllEscapes) {
    auto v = read_ok("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"");
    // 期望解码后：" \ / backspace formfeed newline cr tab
    EXPECT_EQ(v.as_string(), Utf8String::from_cstr("\"\\/\b\f\n\r\t"));
}

TEST(JsonReaderTest, ParsesHexEscape) {
    auto v = read_ok("\"\\u0041\"");  // 'A'
    EXPECT_EQ(v.as_string(), Utf8String::from_cstr("A"));
}

TEST(JsonReaderTest, RejectsUnterminatedString) {
    EXPECT_TRUE(JsonReader::read(R("\"hello")).is_err());
}

TEST(JsonReaderTest, RejectsUnescapedControlChar) {
    // 字符串内裸露的换行（实际换行，非 \n 转义）必须报错
    EXPECT_TRUE(JsonReader::read(R("\"a\nb\"")).is_err());
}

TEST(JsonReaderTest, RejectsBadEscape) {
    EXPECT_TRUE(JsonReader::read(R("\"\\x\"")).is_err());
}

TEST(JsonReaderTest, RejectsIncompleteUnicodeEscape) {
    EXPECT_TRUE(JsonReader::read(R("\"\\u12\"")).is_err());
}

// ============================================================================
// 数组与对象
// ============================================================================

TEST(JsonReaderTest, ParsesEmptyArray) {
    auto v = read_ok("[]");
    ASSERT_TRUE(v.is_array());
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonReaderTest, ParsesEmptyObject) {
    auto v = read_ok("{}");
    ASSERT_TRUE(v.is_object());
    EXPECT_EQ(v.as_object().size(), 0u);
}

TEST(JsonReaderTest, ParsesMixedArray) {
    auto v = read_ok("[1, \"two\", true, null, 3.5]");
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ(v.size(), 5u);
    EXPECT_EQ(v.at(0).as_int(), 1);
    EXPECT_EQ(v.at(1).as_string(), Utf8String::from_cstr("two"));
    EXPECT_TRUE(v.at(2).as_bool());
    EXPECT_TRUE(v.at(3).is_null());
    EXPECT_DOUBLE_EQ(v.at(4).as_float(), 3.5);
}

TEST(JsonReaderTest, ParsesObject) {
    auto v = read_ok("{\"name\": \"Alice\", \"age\": 30}");
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ(v.as_object().size(), 2u);
    ASSERT_NE(v.find(R("name")), nullptr);
    EXPECT_EQ(v.find(R("name"))->as_string(), Utf8String::from_cstr("Alice"));
    ASSERT_NE(v.find(R("age")), nullptr);
    EXPECT_EQ(v.find(R("age"))->as_int(), 30);
    EXPECT_EQ(v.find(R("missing")), nullptr);
}

TEST(JsonReaderTest, ParsesNested) {
    auto v = read_ok("{\"a\": [1, 2, {\"b\": [3, 4]}]}");
    ASSERT_TRUE(v.is_object());
    auto* a = v.find(R("a"));
    ASSERT_NE(a, nullptr);
    ASSERT_TRUE(a->is_array());
    ASSERT_EQ(a->size(), 3u);
    auto* inner = &a->at(2);
    ASSERT_TRUE(inner->is_object());
    ASSERT_NE(inner->find(R("b")), nullptr);
    EXPECT_EQ(inner->find(R("b"))->at(1).as_int(), 4);
}

TEST(JsonReaderTest, IgnoresWhitespace) {
    auto v = read_ok("  {  \"k\"  :  1  }  ");
    ASSERT_TRUE(v.is_object());
    EXPECT_EQ(v.find(R("k"))->as_int(), 1);
}

TEST(JsonReaderTest, RejectsMissingCommaInArray) {
    EXPECT_TRUE(JsonReader::read(R("[1 2]")).is_err());
}

TEST(JsonReaderTest, RejectsMissingColon) {
    EXPECT_TRUE(JsonReader::read(R("{\"k\" 1}")).is_err());
}

TEST(JsonReaderTest, RejectsNonStringKey) {
    EXPECT_TRUE(JsonReader::read(R("{1: 2}")).is_err());
}

TEST(JsonReaderTest, RejectsTrailingCommaByDefault) {
    EXPECT_TRUE(JsonReader::read(R("[1, 2,]")).is_err());
    EXPECT_TRUE(JsonReader::read(R("{\"a\":1,}")).is_err());
}

TEST(JsonReaderTest, AllowsTrailingCommaWhenOptionSet) {
    JsonReaderOptions opts;
    opts.allow_trailing_comma = true;
    EXPECT_TRUE(read_ok("[1, 2,]", opts).is_array());
    EXPECT_TRUE(read_ok("{\"a\":1,}", opts).is_object());
}

TEST(JsonReaderTest, RejectsCommentsByDefault) {
    EXPECT_TRUE(JsonReader::read(R("// comment\n1")).is_err());
    EXPECT_TRUE(JsonReader::read(R("/* c */1")).is_err());
}

TEST(JsonReaderTest, AllowsCommentsWhenOptionSet) {
    JsonReaderOptions opts;
    opts.allow_comments = true;
    EXPECT_EQ(read_ok("// hi\n42", opts).as_int(), 42);
    EXPECT_EQ(read_ok("/* hi */42", opts).as_int(), 42);
}

TEST(JsonReaderTest, RejectsEmptyInput) {
    EXPECT_TRUE(JsonReader::read(R("")).is_err());
    EXPECT_TRUE(JsonReader::read(R("   ")).is_err());
}

TEST(JsonReaderTest, RejectsTrailingChars) {
    EXPECT_TRUE(JsonReader::read(R("1 2")).is_err());
    EXPECT_TRUE(JsonReader::read(R("{}x")).is_err());
}

TEST(JsonReaderTest, RejectsBom) {
    auto result = JsonReader::read(Utf8StringRef::from_data(
        reinterpret_cast<const u8*>("\xEF\xBB\xBF{}"), 5));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// DOM 编辑
// ============================================================================

TEST(JsonValueTest, FactoryAndTypeQueries) {
    EXPECT_TRUE(JsonValue::make_null().is_null());
    EXPECT_TRUE(JsonValue::make_bool(true).is_bool());
    EXPECT_TRUE(JsonValue::make_int(5).is_int());
    EXPECT_TRUE(JsonValue::make_float(1.5).is_float());
    EXPECT_TRUE(JsonValue::make_string(Utf8String::from_cstr("x")).is_string());
    EXPECT_TRUE(JsonValue::make_array().is_array());
    EXPECT_TRUE(JsonValue::make_object().is_object());
    EXPECT_TRUE(JsonValue::make_int(5).is_number());
    EXPECT_TRUE(JsonValue::make_float(1.5).is_number());
    EXPECT_FALSE(JsonValue::make_null().is_number());
}

TEST(JsonValueTest, NumericConversions) {
    EXPECT_EQ(JsonValue::make_int(42).as_float_or(0.0), 42.0);
    EXPECT_EQ(JsonValue::make_float(3.7).as_int_or(0), 3);
    EXPECT_EQ(JsonValue::make_null().as_float_or(-1.0), -1.0);
    EXPECT_EQ(JsonValue::make_null().as_int_or(-1), -1);
}

TEST(JsonValueTest, ArrayAppendAndAt) {
    JsonValue arr = JsonValue::make_array();
    arr.append(JsonValue::make_int(1));
    arr.append(JsonValue::make_int(2));
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_EQ(arr.at(0).as_int(), 1);
    EXPECT_EQ(arr.at(1).as_int(), 2);
}

TEST(JsonValueTest, ObjectSetOverwritesSameKey) {
    JsonValue obj = JsonValue::make_object();
    obj.set(Utf8String::from_cstr("k"), JsonValue::make_int(1));
    obj.set(Utf8String::from_cstr("k"), JsonValue::make_int(2));
    EXPECT_EQ(obj.as_object().size(), 1u);
    EXPECT_EQ(obj.find(R("k"))->as_int(), 2);
}

TEST(JsonValueTest, ObjectPreservesInsertionOrder) {
    JsonValue obj = JsonValue::make_object();
    obj.set(Utf8String::from_cstr("c"), JsonValue::make_int(3));
    obj.set(Utf8String::from_cstr("a"), JsonValue::make_int(1));
    obj.set(Utf8String::from_cstr("b"), JsonValue::make_int(2));
    ASSERT_EQ(obj.as_object().size(), 3u);
    EXPECT_EQ(obj.as_object()[0].first, Utf8String::from_cstr("c"));
    EXPECT_EQ(obj.as_object()[1].first, Utf8String::from_cstr("a"));
    EXPECT_EQ(obj.as_object()[2].first, Utf8String::from_cstr("b"));
}

TEST(JsonValueTest, ObjectRemove) {
    JsonValue obj = JsonValue::make_object();
    obj.set(Utf8String::from_cstr("k"), JsonValue::make_int(1));
    EXPECT_TRUE(obj.remove(R("k")));
    EXPECT_FALSE(obj.remove(R("k")));
    EXPECT_EQ(obj.as_object().size(), 0u);
}

TEST(JsonValueTest, CloneDeepCopies) {
    JsonValue arr = JsonValue::make_array();
    arr.append(JsonValue::make_int(1));
    JsonValue copy = arr.clone();
    copy.at(0) = JsonValue::make_int(99);
    EXPECT_EQ(arr.at(0).as_int(), 1);       // 原对象不受影响
    EXPECT_EQ(copy.at(0).as_int(), 99);
}
