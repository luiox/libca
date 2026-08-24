#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "libca/str/string_util.hpp"

using namespace ca::str;

// ============================================================
// 大小写转换
// ============================================================

TEST(StringUtilTest, to_lower_case) {
    EXPECT_EQ(StringUtil::to_lower_case("Hello World"), "hello world");
    EXPECT_EQ(StringUtil::to_lower_case("ABC"), "abc");
    EXPECT_EQ(StringUtil::to_lower_case(""), "");
}

TEST(StringUtilTest, to_upper_case) {
    EXPECT_EQ(StringUtil::to_upper_case("Hello World"), "HELLO WORLD");
    EXPECT_EQ(StringUtil::to_upper_case("abc"), "ABC");
    EXPECT_EQ(StringUtil::to_upper_case(""), "");
}

TEST(StringUtilTest, capitalize) {
    EXPECT_EQ(StringUtil::capitalize("hello"), "Hello");
    EXPECT_EQ(StringUtil::capitalize("Hello"), "Hello");
    EXPECT_EQ(StringUtil::capitalize(""), "");
    EXPECT_EQ(StringUtil::capitalize("h"), "H");
}

// ============================================================
// 类型转换
// ============================================================

TEST(StringUtilTest, to_int) {
    EXPECT_EQ(StringUtil::to_int("42"), 42);
    EXPECT_EQ(StringUtil::to_int("-1"), -1);
    EXPECT_EQ(StringUtil::to_int("0"), 0);
}

TEST(StringUtilTest, to_double) {
    EXPECT_DOUBLE_EQ(StringUtil::to_double("3.14"), 3.14);
    EXPECT_DOUBLE_EQ(StringUtil::to_double("0.0"), 0.0);
}

TEST(StringUtilTest, ToString) {
    EXPECT_EQ(StringUtil::to_string(42), "42");
    EXPECT_EQ(StringUtil::to_string(3.14f), "3.140000");
    EXPECT_EQ(StringUtil::to_string('A'), "A");
}

// ============================================================
// 修剪
// ============================================================

TEST(StringUtilTest, trim) {
    EXPECT_EQ(StringUtil::trim("  hello  "), "hello");
    EXPECT_EQ(StringUtil::trim("\r\nhello\n\r"), "hello");
    EXPECT_EQ(StringUtil::trim("hello"), "hello");
    EXPECT_EQ(StringUtil::trim(""), "");
}

TEST(StringUtilTest, trim_start) {
    EXPECT_EQ(StringUtil::trim_start("  hello"), "hello");
    EXPECT_EQ(StringUtil::trim_start("hello"), "hello");
}

TEST(StringUtilTest, trim_end) {
    EXPECT_EQ(StringUtil::trim_end("hello  "), "hello");
    EXPECT_EQ(StringUtil::trim_end("hello"), "hello");
}

TEST(StringUtilTest, trimCustom) {
    EXPECT_EQ(StringUtil::trim("***hello***", '*'), "hello");
    EXPECT_EQ(StringUtil::trim_start("***hello***", '*'), "hello***");
    EXPECT_EQ(StringUtil::trim_end("***hello***", '*'), "***hello");
}

// ============================================================
// 拆分与合并
// ============================================================

TEST(StringUtilTest, splitByWhitespace) {
    std::vector<std::string> result;
    StringUtil::split(result, "hello world test");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
    EXPECT_EQ(result[2], "test");
}

TEST(StringUtilTest, splitByChar) {
    std::vector<std::string> result;
    StringUtil::split(result, "a,b,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(StringUtilTest, splitByMultipleSeparators) {
    std::vector<std::string> result;
    StringUtil::split(result, "a,b;c|d", ",;|");
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
    EXPECT_EQ(result[3], "d");
}

TEST(StringUtilTest, joinDefault) {
    std::vector<std::string> input = {"hello", "world"};
    EXPECT_EQ(StringUtil::join(input), "hello world");
}

TEST(StringUtilTest, joinWithSeparator) {
    std::vector<std::string> input = {"a", "b", "c"};
    EXPECT_EQ(StringUtil::join(input, ','), "a,b,c");
    EXPECT_EQ(StringUtil::join(input, ", "), "a, b, c");
}

TEST(StringUtilTest, joinEmpty) {
    std::vector<std::string> input;
    EXPECT_EQ(StringUtil::join(input), "");
}

// ============================================================
// 比较
// ============================================================

TEST(StringUtilTest, compare) {
    EXPECT_EQ(StringUtil::compare("abc", "abc"), 0);
    EXPECT_TRUE(StringUtil::compare("abc", "ABC", true) == 0);
    EXPECT_TRUE(StringUtil::compare("abc", "ABC") != 0);
    EXPECT_TRUE(StringUtil::compare("abc", "abd") < 0);
}

// ============================================================
// 判断
// ============================================================

TEST(StringUtilTest, is_numeric) {
    EXPECT_TRUE(StringUtil::is_numeric("123"));
    EXPECT_TRUE(StringUtil::is_numeric("0"));
    EXPECT_TRUE(StringUtil::is_numeric("-42"));
    EXPECT_FALSE(StringUtil::is_numeric("abc"));
    EXPECT_FALSE(StringUtil::is_numeric("12a3"));
    EXPECT_FALSE(StringUtil::is_numeric(""));
}

TEST(StringUtilTest, asciiClassificationAndCase) {
    EXPECT_TRUE(StringUtil::is_ascii_lower('a'));
    EXPECT_FALSE(StringUtil::is_ascii_lower('A'));
    EXPECT_TRUE(StringUtil::is_ascii_upper('Z'));
    EXPECT_FALSE(StringUtil::is_ascii_upper('z'));
    EXPECT_TRUE(StringUtil::is_ascii_alpha('Q'));
    EXPECT_TRUE(StringUtil::is_ascii_digit('7'));
    EXPECT_TRUE(StringUtil::is_ascii_alnum('x'));
    EXPECT_FALSE(StringUtil::is_ascii_alnum('-'));

    EXPECT_EQ(StringUtil::ascii_to_lower('A'), 'a');
    EXPECT_EQ(StringUtil::ascii_to_lower('!'), '!');
    EXPECT_EQ(StringUtil::ascii_to_upper('z'), 'Z');
    EXPECT_EQ(StringUtil::ascii_to_upper('8'), '8');
}

TEST(StringUtilTest, urlUnreservedClassification) {
    EXPECT_TRUE(StringUtil::is_unreserved_url_char('A'));
    EXPECT_TRUE(StringUtil::is_unreserved_url_char('9'));
    EXPECT_TRUE(StringUtil::is_unreserved_url_char('-'));
    EXPECT_TRUE(StringUtil::is_unreserved_url_char('.'));
    EXPECT_TRUE(StringUtil::is_unreserved_url_char('_'));
    EXPECT_TRUE(StringUtil::is_unreserved_url_char('~'));
    EXPECT_FALSE(StringUtil::is_unreserved_url_char(' '));
    EXPECT_FALSE(StringUtil::is_unreserved_url_char('/'));
}

// ============================================================
// URL / percent 编码
// ============================================================

TEST(StringUtilTest, percentEncodeUsesUppercaseHex) {
    EXPECT_EQ(StringUtil::percent_encode("abcXYZ-._~09"), "abcXYZ-._~09");
    EXPECT_EQ(StringUtil::percent_encode("a b/c?d"), "a%20b%2Fc%3Fd");
    EXPECT_EQ(StringUtil::percent_encode("你好"), "%E4%BD%A0%E5%A5%BD");
}

TEST(StringUtilTest, percentDecodeRoundTrip) {
    auto decoded = StringUtil::percent_decode("a%20b%2Fc%3Fd");
    ASSERT_TRUE(decoded.is_ok()) << decoded.unwrap_err();
    EXPECT_EQ(decoded.unwrap(), "a b/c?d");

    auto utf8 = StringUtil::percent_decode("%E4%BD%A0%E5%A5%BD");
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err();
    EXPECT_EQ(utf8.unwrap(), "你好");
}

TEST(StringUtilTest, percentDecodeRejectsInvalidEscapes) {
    EXPECT_TRUE(StringUtil::percent_decode("%").is_err());
    EXPECT_TRUE(StringUtil::percent_decode("%2").is_err());
    EXPECT_TRUE(StringUtil::percent_decode("%GG").is_err());
}

TEST(StringUtilTest, urlComponentUsesPlusForSpace) {
    EXPECT_EQ(StringUtil::url_encode_component("a b+c"), "a+b%2Bc");

    auto decoded = StringUtil::url_decode_component("a+b%2Bc");
    ASSERT_TRUE(decoded.is_ok()) << decoded.unwrap_err();
    EXPECT_EQ(decoded.unwrap(), "a b+c");
}

TEST(StringUtilTest, base64UrlEncodeDecodeWithoutPadding) {
    EXPECT_EQ(StringUtil::base64UrlEncode(""), "");
    EXPECT_EQ(StringUtil::base64UrlEncode("f"), "Zg");
    EXPECT_EQ(StringUtil::base64UrlEncode("fo"), "Zm8");
    EXPECT_EQ(StringUtil::base64UrlEncode("foo"), "Zm9v");
    EXPECT_EQ(StringUtil::base64UrlEncode("\xfb\xff", false), "-_8");

    auto decoded = StringUtil::base64UrlDecode("-_8");
    ASSERT_TRUE(decoded.is_ok()) << decoded.unwrap_err();
    EXPECT_EQ(decoded.unwrap(), std::string("\xfb\xff", 2));
}

TEST(StringUtilTest, base64UrlSupportsPaddingAndRejectsInvalidInput) {
    EXPECT_EQ(StringUtil::base64UrlEncode("f", true), "Zg==");

    auto padded = StringUtil::base64UrlDecode("Zg==");
    ASSERT_TRUE(padded.is_ok()) << padded.unwrap_err();
    EXPECT_EQ(padded.unwrap(), "f");

    EXPECT_TRUE(StringUtil::base64UrlDecode("Z").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zh").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zm9").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zg=").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zg===").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zg=A").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zm=v").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zm9v!").is_err());
}

// ============================================================
// 前缀/后缀/包含
// ============================================================

TEST(StringUtilTest, starts_with) {
    EXPECT_TRUE(StringUtil::starts_with("hello world", "hello"));
    EXPECT_TRUE(StringUtil::starts_with("hello", "hello"));
    EXPECT_FALSE(StringUtil::starts_with("hello world", "world"));
    EXPECT_FALSE(StringUtil::starts_with("hel", "hello"));
    EXPECT_FALSE(StringUtil::starts_with("", "hello"));
    EXPECT_TRUE(StringUtil::starts_with("hello", ""));
}

TEST(StringUtilTest, ends_with) {
    EXPECT_TRUE(StringUtil::ends_with("hello world", "world"));
    EXPECT_TRUE(StringUtil::ends_with("hello", "hello"));
    EXPECT_FALSE(StringUtil::ends_with("hello world", "hello"));
    EXPECT_FALSE(StringUtil::ends_with("hel", "hello"));
    EXPECT_FALSE(StringUtil::ends_with("", "hello"));
    EXPECT_TRUE(StringUtil::ends_with("hello", ""));
}

TEST(StringUtilTest, contains) {
    EXPECT_TRUE(StringUtil::contains("hello world", "lo wo"));
    EXPECT_TRUE(StringUtil::contains("hello world", "hello"));
    EXPECT_TRUE(StringUtil::contains("hello world", "world"));
    EXPECT_FALSE(StringUtil::contains("hello world", "xyz"));
    EXPECT_TRUE(StringUtil::contains("hello", ""));
    EXPECT_FALSE(StringUtil::contains("", "hello"));
}

// to_lower_case/to_upper_case 曾把有符号 char 直接传 ::tolower（高位字节为负值，
// 违反 ctype 前置条件，UB/MSVC debug 断言）；非 ASCII 字节应原样保留。
TEST(StringUtilTest, CaseConversionPreservesHighBytes) {
    const std::string utf8 = "\xE4\xB8\xAD";  // "中"
    EXPECT_EQ(StringUtil::to_lower_case(utf8), utf8);
    EXPECT_EQ(StringUtil::to_upper_case(utf8), utf8);
    // ASCII 部分照常转换
    EXPECT_EQ(StringUtil::to_lower_case("ABC"), "abc");
    EXPECT_EQ(StringUtil::to_upper_case("abc"), "ABC");
}
