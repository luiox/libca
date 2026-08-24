#include "libca/toml/toml.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <utility>

using namespace ca;
using namespace ca::toml;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// 解析并断言成功；返回 document（移动取出）。
TomlDocument read_ok(const char* text, TomlReaderOptions opts = {}) {
    auto result = TomlReader::read(R(text), opts);
    EXPECT_TRUE(result.is_ok()) << "expected parse success for: " << text
                                << "; message: "
                                << (result.is_err() ? std::move(result).unwrap_err().message.c_str() : "");
    return std::move(result).unwrap();
}

// 解析并断言失败；返回错误（移动取出 message 避免拷贝警告）。
bool read_fails(const char* text) {
    auto result = TomlReader::read(R(text));
    return result.is_err();
}

}  // namespace

// ============================================================================
// 标量字面量
// ============================================================================

TEST(TomlReaderTest, ParsesStringBasic) {
    auto doc = read_ok("key = \"value\"");
    auto* v = doc.root().find(R("key"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_string());
    EXPECT_EQ(v->as_string(), R("value"));
}

TEST(TomlReaderTest, ParsesStringLiteral) {
    auto doc = read_ok("path = 'C:\\\\raw'");
    auto* v = doc.root().find(R("path"));
    ASSERT_NE(v, nullptr);
    // literal string 无转义：原样 4 个反斜杠
    EXPECT_EQ(v->as_string(), R("C:\\\\raw"));
}

TEST(TomlReaderTest, ParsesStringBasicEscapes) {
    auto doc = read_ok("s = \"a\\tb\\nc\"");
    auto* v = doc.root().find(R("s"));
    ASSERT_NE(v, nullptr);
    // a<TAB>b<LF>c
    EXPECT_EQ(v->as_string().byte_length(), 5u);
}

TEST(TomlReaderTest, ParsesStringUnicodeEscape) {
    auto doc = read_ok("s = \"\\u00e9\"");  // é
    auto* v = doc.root().find(R("s"));
    ASSERT_NE(v, nullptr);
    // é 是 2 字节 UTF-8 (0xC3 0xA9)
    EXPECT_EQ(v->as_string().byte_length(), 2u);
}

TEST(TomlReaderTest, ParsesStringMultilineBasic) {
    auto doc = read_ok("s = \"\"\"\nline1\nline2\"\"\"");
    auto* v = doc.root().find(R("s"));
    ASSERT_NE(v, nullptr);
    // 开头紧跟换行被吃掉；剩 "line1\nline2"
    EXPECT_EQ(v->as_string(), R("line1\nline2"));
}

TEST(TomlReaderTest, ParsesStringMultilineLineContinuation) {
    // 行尾反斜杠：吃掉反斜杠到下一个非空白字符之间的所有空白（含换行）。
    // 这里第二行末尾的反斜杠吃掉 "\n   "，剩下 "kept"。
    auto doc = read_ok("s = \"\"\"\nline1   \\\n   kept\"\"\"");
    auto* v = doc.root().find(R("s"));
    ASSERT_NE(v, nullptr);
    // 结果：line1 紧接 kept（反斜杠续行吃掉了换行和前导空白）
    EXPECT_EQ(v->as_string(), R("line1   kept"));
}

TEST(TomlReaderTest, ParsesStringMultilineLiteral) {
    auto doc = read_ok("s = '''\nfirst\nsecond'''");
    auto* v = doc.root().find(R("s"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_string(), R("first\nsecond"));
}

TEST(TomlReaderTest, ParsesInteger) {
    auto doc = read_ok("x = 42");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_integer());
    EXPECT_EQ(v->as_integer(), 42);
}

TEST(TomlReaderTest, ParsesNegativeInteger) {
    auto doc = read_ok("x = -17");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), -17);
}

TEST(TomlReaderTest, ParsesIntegerUnderscore) {
    auto doc = read_ok("x = 1_000_000");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 1000000);
}

TEST(TomlReaderTest, ParsesIntegerHex) {
    auto doc = read_ok("x = 0xDEAD");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 0xDEAD);
    auto doc2 = read_ok("y = 0xdead");
    EXPECT_EQ(doc2.root().find(R("y"))->as_integer(), 0xdead);
}

TEST(TomlReaderTest, ParsesIntegerOct) {
    auto doc = read_ok("x = 0o17");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 017);
}

TEST(TomlReaderTest, ParsesIntegerBin) {
    auto doc = read_ok("x = 0b1011");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 11);
}

TEST(TomlReaderTest, ParsesIntegerOverflowFails) {
    EXPECT_TRUE(read_fails("x = 99999999999999999999999"));
}

TEST(TomlReaderTest, ParsesFloat) {
    auto doc = read_ok("x = 3.14");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_float());
    EXPECT_DOUBLE_EQ(v->as_float(), 3.14);
}

TEST(TomlReaderTest, ParsesFloatExponent) {
    auto doc = read_ok("x = 6.626e-34");
    EXPECT_DOUBLE_EQ(doc.root().find(R("x"))->as_float(), 6.626e-34);
}

TEST(TomlReaderTest, ParsesFloatInf) {
    auto doc = read_ok("a = inf\nb = -inf\nc = +inf");
    EXPECT_EQ(doc.root().find(R("a"))->as_float(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(doc.root().find(R("b"))->as_float(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(doc.root().find(R("c"))->as_float(), std::numeric_limits<double>::infinity());
}

TEST(TomlReaderTest, ParsesFloatNan) {
    auto doc = read_ok("a = nan\nb = -nan");
    ASSERT_TRUE(doc.root().find(R("a"))->is_float());
    EXPECT_TRUE(std::isnan(doc.root().find(R("a"))->as_float()));
    EXPECT_TRUE(std::isnan(doc.root().find(R("b"))->as_float()));
}

TEST(TomlReaderTest, ParsesBoolean) {
    auto doc = read_ok("a = true\nb = false");
    EXPECT_TRUE(doc.root().find(R("a"))->as_boolean());
    EXPECT_FALSE(doc.root().find(R("b"))->as_boolean());
}

// ============================================================================
// datetime 4 变体
// ============================================================================

TEST(TomlReaderTest, ParsesLocalDate) {
    auto doc = read_ok("d = 1912-07-28");
    auto* v = doc.root().find(R("d"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_local_date());
    const auto& dt = v->as_local_date();
    EXPECT_EQ(dt.year, 1912);
    EXPECT_EQ(dt.month, 7);
    EXPECT_EQ(dt.day, 28);
}

TEST(TomlReaderTest, ParsesLocalTime) {
    auto doc = read_ok("t = 07:32:00");
    auto* v = doc.root().find(R("t"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_local_time());
    const auto& dt = v->as_local_time();
    EXPECT_EQ(dt.hour, 7);
    EXPECT_EQ(dt.minute, 32);
    EXPECT_EQ(dt.second, 0);
}

TEST(TomlReaderTest, ParsesLocalTimeFractional) {
    auto doc = read_ok("t = 00:30:00.001");
    auto* v = doc.root().find(R("t"));
    ASSERT_NE(v, nullptr);
    const auto& dt = v->as_local_time();
    EXPECT_EQ(dt.nanos, 1000000u);  // 0.001s = 1ms = 1_000_000 ns
}

TEST(TomlReaderTest, ParsesLocalDateTime) {
    auto doc = read_ok("dt = 1979-05-27T07:32:00");
    auto* v = doc.root().find(R("dt"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_local_datetime());
    const auto& dt = v->as_local_datetime();
    EXPECT_EQ(dt.year, 1979);
    EXPECT_EQ(dt.month, 5);
    EXPECT_EQ(dt.day, 27);
    EXPECT_EQ(dt.hour, 7);
    EXPECT_EQ(dt.minute, 32);
    EXPECT_EQ(dt.second, 0);
}

TEST(TomlReaderTest, ParsesLocalDateTimeSpaceSeparator) {
    auto doc = read_ok("dt = 1979-05-27 07:32:00");
    auto* v = doc.root().find(R("dt"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_local_datetime());
    EXPECT_EQ(v->as_local_datetime().hour, 7);
}

TEST(TomlReaderTest, ParsesOffsetDatetimeZ) {
    auto doc = read_ok("dt = 1979-05-27T07:32:00Z");
    auto* v = doc.root().find(R("dt"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_offset_datetime());
    const auto& dt = v->as_offset_datetime();
    EXPECT_TRUE(dt.has_tz);
    EXPECT_EQ(dt.tz_minutes, 0);
}

TEST(TomlReaderTest, ParsesOffsetDatetimeOffset) {
    auto doc = read_ok("dt = 1979-05-27T07:32:00-07:00");
    auto* v = doc.root().find(R("dt"));
    ASSERT_NE(v, nullptr);
    const auto& dt = v->as_offset_datetime();
    EXPECT_TRUE(dt.has_tz);
    EXPECT_EQ(dt.tz_minutes, -7 * 60);
}

// ============================================================================
// Array
// ============================================================================

TEST(TomlReaderTest, ParsesArray) {
    auto doc = read_ok("arr = [1, 2, 3]");
    auto* v = doc.root().find(R("arr"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_array());
    EXPECT_EQ(v->size(), 3u);
    EXPECT_EQ(v->at(0).as_integer(), 1);
    EXPECT_EQ(v->at(1).as_integer(), 2);
    EXPECT_EQ(v->at(2).as_integer(), 3);
}

TEST(TomlReaderTest, ParsesArrayMultiline) {
    auto doc = read_ok("arr = [\n  1,\n  2,\n  3,\n]");
    auto* v = doc.root().find(R("arr"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->size(), 3u);
}

TEST(TomlReaderTest, ParsesArrayMixedTypes) {
    auto doc = read_ok("arr = [1, \"two\", true, 3.14]");
    auto* v = doc.root().find(R("arr"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->size(), 4u);
    EXPECT_TRUE(v->at(0).is_integer());
    EXPECT_TRUE(v->at(1).is_string());
    EXPECT_TRUE(v->at(2).is_boolean());
    EXPECT_TRUE(v->at(3).is_float());
}

TEST(TomlReaderTest, ParsesNestedArray) {
    auto doc = read_ok("arr = [[1, 2], [3, 4]]");
    auto* v = doc.root().find(R("arr"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->at(0).at(0).as_integer(), 1);
    EXPECT_EQ(v->at(1).at(1).as_integer(), 4);
}

// ============================================================================
// Table
// ============================================================================

TEST(TomlReaderTest, ParsesStandardTable) {
    auto doc = read_ok("[server]\nhost = \"localhost\"\nport = 8080");
    auto* server = doc.root().find(R("server"));
    ASSERT_NE(server, nullptr);
    ASSERT_TRUE(server->is_table());
    EXPECT_EQ(server->find(R("host"))->as_string(), R("localhost"));
    EXPECT_EQ(server->find(R("port"))->as_integer(), 8080);
}

TEST(TomlReaderTest, ParsesDottedKey) {
    auto doc = read_ok("a.b.c = 1");
    auto* a = doc.root().find(R("a"));
    ASSERT_NE(a, nullptr);
    auto* b = a->find(R("b"));
    ASSERT_NE(b, nullptr);
    auto* c = b->find(R("c"));
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->as_integer(), 1);
}

TEST(TomlReaderTest, ParsesNestedTableHeader) {
    auto doc = read_ok("[a.b.c]\nx = 1");
    auto* c = doc.root().find(R("a"))->find(R("b"))->find(R("c"));
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->find(R("x"))->as_integer(), 1);
}

TEST(TomlReaderTest, ParsesInlineTable) {
    auto doc = read_ok("point = { x = 1, y = 2 }");
    auto* p = doc.root().find(R("point"));
    ASSERT_NE(p, nullptr);
    ASSERT_TRUE(p->is_table());
    EXPECT_EQ(p->find(R("x"))->as_integer(), 1);
    EXPECT_EQ(p->find(R("y"))->as_integer(), 2);
}

TEST(TomlReaderTest, ParsesInlineTableDottedKey) {
    auto doc = read_ok("a = { b.c = 1 }");
    auto* a = doc.root().find(R("a"));
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->find(R("b"))->find(R("c"))->as_integer(), 1);
}

// ============================================================================
// Array of Tables
// ============================================================================

TEST(TomlReaderTest, ParsesArrayOfTables) {
    auto doc = read_ok(
        "[[products]]\n"
        "name = \"A\"\n"
        "[[products]]\n"
        "name = \"B\"\n");
    auto* products = doc.root().find(R("products"));
    ASSERT_NE(products, nullptr);
    ASSERT_TRUE(products->is_array());
    EXPECT_EQ(products->size(), 2u);
    EXPECT_EQ(products->at(0).find(R("name"))->as_string(), R("A"));
    EXPECT_EQ(products->at(1).find(R("name"))->as_string(), R("B"));
}

TEST(TomlReaderTest, ParsesArrayOfTablesWithSubtable) {
    auto doc = read_ok(
        "[[fruits]]\n"
        "name = \"apple\"\n"
        "[fruits.physical]\n"
        "color = \"red\"\n"
        "[[fruits]]\n"
        "name = \"banana\"\n");
    auto* fruits = doc.root().find(R("fruits"));
    ASSERT_NE(fruits, nullptr);
    EXPECT_EQ(fruits->size(), 2u);
    // 第一个 fruits 元素带 physical.color = red
    EXPECT_EQ(fruits->at(0).find(R("physical"))->find(R("color"))->as_string(), R("red"));
    EXPECT_EQ(fruits->at(0).find(R("name"))->as_string(), R("apple"));
    EXPECT_EQ(fruits->at(1).find(R("name"))->as_string(), R("banana"));
}

// ============================================================================
// 错误处理 / 重复检测
// ============================================================================

TEST(TomlReaderTest, RejectsDuplicateKey) {
    EXPECT_TRUE(read_fails("a = 1\na = 2"));
}

TEST(TomlReaderTest, RejectsDuplicateTableHeader) {
    EXPECT_TRUE(read_fails("[a]\n[a]"));
}

// TOML 1.0 允许后置定义超表（spec: defining a super-table afterward is ok）；
// 旧实现把 header 的全部严格前缀登记为不可再命名而拒绝此合法输入。
TEST(TomlReaderTest, SuperTableDefinedAfterSubTableIsAccepted) {
    auto doc = read_ok("[a.b]\nx = 1\n\n[a]\ny = 2\n");
    // a.b.x 与 a.y 并存（a 的显式定义不清空隐式子表）
    const auto* a = doc.root().find(R("a"));
    ASSERT_NE(a, nullptr);
    const auto* ab = a->find(R("b"));
    ASSERT_NE(ab, nullptr);
    const auto* x = ab->find(R("x"));
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->as_integer(), 1);
    const auto* y = a->find(R("y"));
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->as_integer(), 2);
}

TEST(TomlReaderTest, RejectsRedefinedTable) {
    EXPECT_TRUE(read_fails("[a]\n[a]"));
    EXPECT_TRUE(read_fails("[a.b]\n[a.b]"));
}

// 进制整数不允许符号（ABNF：hex/oct/bin-int 无 sign；strtoll 曾连符号消费 0x-5）。
TEST(TomlReaderTest, RejectsSignInRadixInteger) {
    EXPECT_TRUE(read_fails("x = 0x-5"));
    EXPECT_TRUE(read_fails("x = 0o+7"));
    EXPECT_TRUE(read_fails("x = 0b-1"));
    EXPECT_TRUE(read_fails("x = 0x 5"));  // 前导空白同样拒绝
    EXPECT_TRUE(read_fails("x = 0x+5"));
    // 合法形态不受影响
    EXPECT_FALSE(read_fails("x = 0xFF"));
}

// 下划线必须夹在两个数字之间：1e_2 曾因 prev=='e' 被外层条件放行。
TEST(TomlReaderTest, RejectsUnderscoreAfterExponentMark) {
    EXPECT_TRUE(read_fails("x = 1e_2"));
    EXPECT_TRUE(read_fails("x = 1_E2"));
    EXPECT_TRUE(read_fails("x = _1"));
    EXPECT_TRUE(read_fails("x = 1_"));
    EXPECT_FALSE(read_fails("x = 1_0.5_2e1_0"));
}

TEST(TomlReaderTest, RejectsCommentOnlyMissingValue) {
    EXPECT_TRUE(read_fails("key = "));
}

TEST(TomlReaderTest, RejectsBareKeyStartingWithDot) {
    EXPECT_TRUE(read_fails(".x = 1"));
}

TEST(TomlReaderTest, AllowsComments) {
    auto doc = read_ok("# comment\nx = 1  # trailing\n");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 1);
}

TEST(TomlReaderTest, AllowsBom) {
    const char bom_text[] = "\xEF\xBB\xBFx = 1";
    auto result = TomlReader::read(R(bom_text));
    EXPECT_TRUE(result.is_ok());
}

TEST(TomlReaderTest, EmptyDocumentIsValid) {
    auto doc = read_ok("");
    EXPECT_TRUE(doc.root().is_table());
    EXPECT_EQ(doc.root().as_table().size(), 0u);
}

// ============================================================================
// 更多边界 / corner case
// ============================================================================

TEST(TomlReaderTest, RejectsArrayOfTablesRedefinesAsTable) {
    // [[fruits]] 后再用 [fruits] 普通表头定义同名 → 报错
    EXPECT_TRUE(read_fails("[[fruits]]\nname = \"a\"\n[fruits]\nx = 1"));
}

TEST(TomlReaderTest, RejectsTableRedefinesAsArrayOfTables) {
    // [fruits] 后再 [[fruits]] → 报错（普通表不能再定义为数组）
    EXPECT_TRUE(read_fails("[fruits]\nx = 1\n[[fruits]]\nname = \"a\""));
}

TEST(TomlReaderTest, RejectsDottedKeyRedefinesTable) {
    // 由 dotted key 隐式创建的表，再用 [header] 命名其自身 → 报错
    EXPECT_TRUE(read_fails("a.b = 1\n[a.b]"));
}

TEST(TomlReaderTest, AllowsHeaderOnDottedKeyParent) {
    // dotted key 隐式创建 fruit.apple，[fruit.apple.texture] 是子表，合法。
    auto doc = read_ok(
        "[fruit]\n"
        "apple.color = \"red\"\n"
        "[fruit.apple.texture]\n"
        "smooth = true\n");
    auto* tx = doc.root().find(R("fruit"))->find(R("apple"))->find(R("texture"));
    ASSERT_NE(tx, nullptr);
    EXPECT_TRUE(tx->find(R("smooth"))->as_boolean());
}

TEST(TomlReaderTest, ParsesQuotedKey) {
    auto doc = read_ok("\"my key\" = 1");
    auto* v = doc.root().find(R("my key"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 1);
}

TEST(TomlReaderTest, ParsesQuotedKeyWithSpecialChars) {
    auto doc = read_ok("\"127.0.0.1\" = \"localhost\"");
    auto* v = doc.root().find(R("127.0.0.1"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_string(), R("localhost"));
}

TEST(TomlReaderTest, RejectsIntLeadingZeros) {
    EXPECT_TRUE(read_fails("x = 01"));
}

TEST(TomlReaderTest, RejectsFloatLeadingZeros) {
    EXPECT_TRUE(read_fails("x = 00.1"));
}

TEST(TomlReaderTest, ParsesFloatUnderscore) {
    auto doc = read_ok("x = 1_000.5");
    EXPECT_DOUBLE_EQ(doc.root().find(R("x"))->as_float(), 1000.5);
}

TEST(TomlReaderTest, ParsesEmptyArray) {
    auto doc = read_ok("x = []");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_array());
    EXPECT_EQ(v->size(), 0u);
}

TEST(TomlReaderTest, ParsesEmptyInlineTable) {
    auto doc = read_ok("x = {}");
    auto* v = doc.root().find(R("x"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_table());
    EXPECT_EQ(v->as_table().size(), 0u);
}

TEST(TomlReaderTest, RejectsTrailingCommaInInlineTable) {
    EXPECT_TRUE(read_fails("x = { a = 1, }"));
}

TEST(TomlReaderTest, RejectsInlineTableMultiline) {
    // TOML 1.0 不允许 inline table 跨行
    EXPECT_TRUE(read_fails("x = { a = 1,\n b = 2 }"));
}

TEST(TomlReaderTest, ParsesInlineTableInArray) {
    auto doc = read_ok("points = [ { x = 1, y = 2 }, { x = 3, y = 4 } ]");
    auto* v = doc.root().find(R("points"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->size(), 2u);
    EXPECT_EQ(v->at(0).find(R("x"))->as_integer(), 1);
    EXPECT_EQ(v->at(1).find(R("y"))->as_integer(), 4);
}

TEST(TomlReaderTest, ParsesCrlfLineEndings) {
    const char* text = "a = 1\r\nb = 2\r\n";
    auto doc = read_ok(text);
    EXPECT_EQ(doc.root().find(R("a"))->as_integer(), 1);
    EXPECT_EQ(doc.root().find(R("b"))->as_integer(), 2);
}

TEST(TomlReaderTest, ParsesNestedArrayOfTables) {
    auto doc = read_ok(
        "[[fruits]]\n"
        "name = \"apple\"\n"
        "[[fruits.varieties]]\n"
        "name = \"red delicious\"\n"
        "[[fruits.varieties]]\n"
        "name = \"granny smith\"\n");
    auto* fruits = doc.root().find(R("fruits"));
    ASSERT_NE(fruits, nullptr);
    auto* varieties = fruits->at(0).find(R("varieties"));
    ASSERT_NE(varieties, nullptr);
    EXPECT_EQ(varieties->size(), 2u);
    EXPECT_EQ(varieties->at(0).find(R("name"))->as_string(), R("red delicious"));
    EXPECT_EQ(varieties->at(1).find(R("name"))->as_string(), R("granny smith"));
}

TEST(TomlReaderTest, DatetimeNanosPrecision) {
    auto doc = read_ok("t = 1979-05-27T00:00:00.123456789");
    auto* v = doc.root().find(R("t"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_local_datetime());
    EXPECT_EQ(v->as_local_datetime().nanos, 123456789u);
}

TEST(TomlReaderTest, DatetimeTruncatesExtraFractionDigits) {
    // 超过 9 位小数 → 截断到纳秒精度（不四舍五入）
    auto doc = read_ok("t = 1979-05-27T00:00:00.1234567899");
    auto* v = doc.root().find(R("t"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_local_datetime().nanos, 123456789u);
}
