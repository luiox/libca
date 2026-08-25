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

// 解析并断言成功；返回 JsonDocument（移动取出）。
JsonDocument read_ok(const char* text, JsonReaderOptions opts = {}) {
    auto result = JsonReader::read(R(text), opts);
    EXPECT_TRUE(result.is_ok()) << "expected parse success for: " << text;
    return std::move(result).unwrap();
}
}  // namespace

// ============================================================================
// 字面量解析
// ============================================================================

TEST(JsonReaderTest, ParsesNull) {
    auto doc = read_ok("null");
    ASSERT_TRUE(doc.root().is_null());
    EXPECT_EQ(doc.root().type(), JsonType::Null);
}

TEST(JsonReaderTest, ParsesBool) {
    EXPECT_TRUE(read_ok("true").root().as_bool());
    EXPECT_FALSE(read_ok("false").root().as_bool());
}

TEST(JsonReaderTest, ParsesInt) {
    auto doc = read_ok("42");
    ASSERT_TRUE(doc.root().is_int());
    EXPECT_EQ(doc.root().as_int(), 42);
}

TEST(JsonReaderTest, ParsesNegativeInt) {
    auto doc = read_ok("-1");
    ASSERT_TRUE(doc.root().is_int());
    EXPECT_EQ(doc.root().as_int(), -1);
}

TEST(JsonReaderTest, ParsesMaxI64) {
    auto doc = read_ok("9223372036854775807");
    ASSERT_TRUE(doc.root().is_int());
    EXPECT_EQ(doc.root().as_int(), 9223372036854775807LL);
}

TEST(JsonReaderTest, ParsesMinI64) {
    auto doc = read_ok("-9223372036854775808");
    ASSERT_TRUE(doc.root().is_int());
    EXPECT_EQ(doc.root().as_int(), (-9223372036854775807LL - 1));
}

TEST(JsonReaderTest, I64OverflowDowngradesToFloat) {
    // 超过 i64 上限，应降级为 float
    auto doc = read_ok("9223372036854775808");  // i64::MAX + 1
    ASSERT_TRUE(doc.root().is_float()) << "expected float for overflowing integer";
    EXPECT_GT(doc.root().as_float(), 9.2e18);
}

TEST(JsonReaderTest, ParsesFloat) {
    auto doc = read_ok("3.14");
    ASSERT_TRUE(doc.root().is_float());
    EXPECT_DOUBLE_EQ(doc.root().as_float(), 3.14);
}

TEST(JsonReaderTest, ParsesFloatWithExponent) {
    EXPECT_DOUBLE_EQ(read_ok("1e10").root().as_float(), 1e10);
    EXPECT_DOUBLE_EQ(read_ok("1.5E-3").root().as_float(), 1.5e-3);
    EXPECT_DOUBLE_EQ(read_ok("-0.0").root().as_float(), -0.0);
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
    auto doc = read_ok("\"\"");
    ASSERT_TRUE(doc.root().is_string());
    EXPECT_TRUE(doc.root().as_string().is_empty());
}

TEST(JsonReaderTest, ParsesAsciiString) {
    auto doc = read_ok("\"hello\"");
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("hello"));
}

TEST(JsonReaderTest, ParsesUtf8String) {
    // 中文 + emoji（emoji 在 JSON 里通常以 surrogate pair 写，但合法 UTF-8 字节也直接接受）
    auto doc = read_ok("\"\\u4e2d\\u6587\"");
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("中文"));
}

TEST(JsonReaderTest, ParsesSurrogatePair) {
    // U+1F600 😀 = \uD83D\uDE00
    auto doc = read_ok("\"\\uD83D\\uDE00\"");
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("\xF0\x9F\x98\x80"));
}

TEST(JsonReaderTest, RejectsDanglingHighSurrogate) {
    EXPECT_TRUE(JsonReader::read(R("\"\\uD83D\"")).is_err());
}

TEST(JsonReaderTest, RejectsUnexpectedLowSurrogate) {
    EXPECT_TRUE(JsonReader::read(R("\"\\uDE00\"")).is_err());
}

TEST(JsonReaderTest, ParsesAllEscapes) {
    auto doc = read_ok("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"");
    // 期望解码后：" \ / backspace formfeed newline cr tab
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("\"\\/\b\f\n\r\t"));
}

TEST(JsonReaderTest, ParsesHexEscape) {
    auto doc = read_ok("\"\\u0041\"");  // 'A'
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("A"));
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

// 无转义字符串走快路径（直接从输入缓冲 intern 切片），须与原文一致。
TEST(JsonReaderTest, FastPathPlainString) {
    auto doc = read_ok("\"hello world 你好\"");
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("hello world 你好"));
}

// 转义位于中段：前缀走快扫描、遇 '\\' 转慢路径，前缀须被完整保留。
TEST(JsonReaderTest, SlowPathPreservesPrefixBeforeEscape) {
    auto doc = read_ok("\"abc\\ndef\"");
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("abc\ndef"));
}

// 慢路径中转义之后的普通字节段（成段追加）须完整保留。
TEST(JsonReaderTest, SlowPathPreservesRunAfterEscape) {
    auto doc = read_ok("\"\\tlong_tail_after_escape\"");
    EXPECT_EQ(doc.root().as_string(), Utf8String::from_cstr("\tlong_tail_after_escape"));
}

// 重复 key：解析保序、全部保留，find() 返回首个（DOM 装配不再逐个 set 去重）。
TEST(JsonReaderTest, DuplicateKeysPreservedFindReturnsFirst) {
    auto doc = read_ok("{\"k\":1,\"k\":2}");
    ASSERT_TRUE(doc.root().is_object());
    EXPECT_EQ(doc.root().as_object().size(), 2u);
    ASSERT_NE(doc.root().find(R("k")), nullptr);
    EXPECT_EQ(doc.root().find(R("k"))->as_int(), 1);
}

// ============================================================================
// 数组与对象
// ============================================================================

TEST(JsonReaderTest, ParsesEmptyArray) {
    auto doc = read_ok("[]");
    ASSERT_TRUE(doc.root().is_array());
    EXPECT_EQ(doc.root().size(), 0u);
}

TEST(JsonReaderTest, ParsesEmptyObject) {
    auto doc = read_ok("{}");
    ASSERT_TRUE(doc.root().is_object());
    EXPECT_EQ(doc.root().as_object().size(), 0u);
}

TEST(JsonReaderTest, ParsesMixedArray) {
    auto doc = read_ok("[1, \"two\", true, null, 3.5]");
    const auto& v = doc.root();
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ(v.size(), 5u);
    EXPECT_EQ(v.at(0).as_int(), 1);
    EXPECT_EQ(v.at(1).as_string(), Utf8String::from_cstr("two"));
    EXPECT_TRUE(v.at(2).as_bool());
    EXPECT_TRUE(v.at(3).is_null());
    EXPECT_DOUBLE_EQ(v.at(4).as_float(), 3.5);
}

TEST(JsonReaderTest, ParsesObject) {
    auto doc = read_ok("{\"name\": \"Alice\", \"age\": 30}");
    const auto& v = doc.root();
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ(v.as_object().size(), 2u);
    ASSERT_NE(v.find(R("name")), nullptr);
    EXPECT_EQ(v.find(R("name"))->as_string(), Utf8String::from_cstr("Alice"));
    ASSERT_NE(v.find(R("age")), nullptr);
    EXPECT_EQ(v.find(R("age"))->as_int(), 30);
    EXPECT_EQ(v.find(R("missing")), nullptr);
}

TEST(JsonReaderTest, ParsesNested) {
    auto doc = read_ok("{\"a\": [1, 2, {\"b\": [3, 4]}]}");
    const auto& v = doc.root();
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
    auto doc = read_ok("  {  \"k\"  :  1  }  ");
    ASSERT_TRUE(doc.root().is_object());
    EXPECT_EQ(doc.root().find(R("k"))->as_int(), 1);
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
    EXPECT_TRUE(read_ok("[1, 2,]", opts).root().is_array());
    EXPECT_TRUE(read_ok("{\"a\":1,}", opts).root().is_object());
}

TEST(JsonReaderTest, RejectsCommentsByDefault) {
    EXPECT_TRUE(JsonReader::read(R("// comment\n1")).is_err());
    EXPECT_TRUE(JsonReader::read(R("/* c */1")).is_err());
}

TEST(JsonReaderTest, AllowsCommentsWhenOptionSet) {
    JsonReaderOptions opts;
    opts.allow_comments = true;
    EXPECT_EQ(read_ok("// hi\n42", opts).root().as_int(), 42);
    EXPECT_EQ(read_ok("/* hi */42", opts).root().as_int(), 42);
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

// 字符串值里的非法 UTF-8 此前经 arena intern 静默变空串、解析"成功"（RFC 8259
// 要求 JSON 文本为合法 UTF-8，模块声明严格模式）。入口整体校验后统一拒绝。
TEST(JsonReaderTest, RejectsInvalidUtf8) {
    // 快路径（无转义）：{"a":"<0xC3 0x28>"}
    const u8 fast[] = "{\"a\":\"\xC3\x28\"}";
    EXPECT_TRUE(JsonReader::read(
        Utf8StringRef::from_data(fast, sizeof(fast) - 1)).is_err());
    // 慢路径（含转义 + 非法尾字节）：["\n<0xFF>"]
    const u8 slow[] = "[\"\\n\xFF\"]";
    EXPECT_TRUE(JsonReader::read(
        Utf8StringRef::from_data(slow, sizeof(slow) - 1)).is_err());
    // 对照：合法多字节内容不受影响
    EXPECT_TRUE(JsonReader::read(R("{\"k\":\"中文\"}")).is_ok());
}

// ============================================================================
// DOM 编辑
// ============================================================================

TEST(JsonValueTest, FactoryAndTypeQueries) {
    JsonDocument doc;  // 为 make_string 提供入池 arena
    auto& arena = doc.arena();
    EXPECT_TRUE(JsonValue::make_null().is_null());
    EXPECT_TRUE(JsonValue::make_bool(true).is_bool());
    EXPECT_TRUE(JsonValue::make_int(5).is_int());
    EXPECT_TRUE(JsonValue::make_float(1.5).is_float());
    EXPECT_TRUE(JsonValue::make_string(arena.intern(Utf8String::from_cstr("x"))).is_string());
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
    JsonDocument doc;
    auto& arena = doc.arena();
    JsonValue obj = JsonValue::make_object();
    obj.set(arena.intern(Utf8String::from_cstr("k")), JsonValue::make_int(1));
    obj.set(arena.intern(Utf8String::from_cstr("k")), JsonValue::make_int(2));
    EXPECT_EQ(obj.as_object().size(), 1u);
    EXPECT_EQ(obj.find(R("k"))->as_int(), 2);
}

TEST(JsonValueTest, ObjectPreservesInsertionOrder) {
    JsonDocument doc;
    auto& arena = doc.arena();
    JsonValue obj = JsonValue::make_object();
    obj.set(arena.intern(Utf8String::from_cstr("c")), JsonValue::make_int(3));
    obj.set(arena.intern(Utf8String::from_cstr("a")), JsonValue::make_int(1));
    obj.set(arena.intern(Utf8String::from_cstr("b")), JsonValue::make_int(2));
    ASSERT_EQ(obj.as_object().size(), 3u);
    EXPECT_EQ(obj.as_object()[0].first, Utf8String::from_cstr("c"));
    EXPECT_EQ(obj.as_object()[1].first, Utf8String::from_cstr("a"));
    EXPECT_EQ(obj.as_object()[2].first, Utf8String::from_cstr("b"));
}

TEST(JsonValueTest, ObjectRemove) {
    JsonDocument doc;
    auto& arena = doc.arena();
    JsonValue obj = JsonValue::make_object();
    obj.set(arena.intern(Utf8String::from_cstr("k")), JsonValue::make_int(1));
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

// 深度守卫：默认 max_depth=1000，嵌套超限须报错而非栈溢出；适度嵌套正常通过。
TEST(JsonReaderTest, RejectsExcessiveNesting) {
    std::string deep;
    for (int i = 0; i < 1200; ++i) deep += "[";
    for (int i = 0; i < 1200; ++i) deep += "]";
    EXPECT_TRUE(JsonReader::read(Utf8StringRef::from_string_view(deep)).is_err());

    std::string moderate;
    for (int i = 0; i < 50; ++i) moderate += "[";
    for (int i = 0; i < 50; ++i) moderate += "]";
    EXPECT_TRUE(JsonReader::read(Utf8StringRef::from_string_view(moderate)).is_ok());
}
