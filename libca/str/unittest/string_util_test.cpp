#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "libca/str/string_util.hpp"

using namespace ca::str;

// ============================================================
// 大小写转换
// ============================================================

TEST(StringUtilTest, toLowerCase) {
    EXPECT_EQ(StringUtil::toLowerCase("Hello World"), "hello world");
    EXPECT_EQ(StringUtil::toLowerCase("ABC"), "abc");
    EXPECT_EQ(StringUtil::toLowerCase(""), "");
}

TEST(StringUtilTest, toUpperCase) {
    EXPECT_EQ(StringUtil::toUpperCase("Hello World"), "HELLO WORLD");
    EXPECT_EQ(StringUtil::toUpperCase("abc"), "ABC");
    EXPECT_EQ(StringUtil::toUpperCase(""), "");
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

TEST(StringUtilTest, toInt) {
    EXPECT_EQ(StringUtil::toInt("42"), 42);
    EXPECT_EQ(StringUtil::toInt("-1"), -1);
    EXPECT_EQ(StringUtil::toInt("0"), 0);
}

TEST(StringUtilTest, toDouble) {
    EXPECT_DOUBLE_EQ(StringUtil::toDouble("3.14"), 3.14);
    EXPECT_DOUBLE_EQ(StringUtil::toDouble("0.0"), 0.0);
}

TEST(StringUtilTest, toString) {
    EXPECT_EQ(StringUtil::toString(42), "42");
    EXPECT_EQ(StringUtil::toString(3.14f), "3.140000");
    EXPECT_EQ(StringUtil::toString('A'), "A");
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

TEST(StringUtilTest, trimStart) {
    EXPECT_EQ(StringUtil::trimStart("  hello"), "hello");
    EXPECT_EQ(StringUtil::trimStart("hello"), "hello");
}

TEST(StringUtilTest, trimEnd) {
    EXPECT_EQ(StringUtil::trimEnd("hello  "), "hello");
    EXPECT_EQ(StringUtil::trimEnd("hello"), "hello");
}

TEST(StringUtilTest, trimCustom) {
    EXPECT_EQ(StringUtil::trim("***hello***", '*'), "hello");
    EXPECT_EQ(StringUtil::trimStart("***hello***", '*'), "hello***");
    EXPECT_EQ(StringUtil::trimEnd("***hello***", '*'), "***hello");
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

TEST(StringUtilTest, isNumeric) {
    EXPECT_TRUE(StringUtil::isNumeric("123"));
    EXPECT_TRUE(StringUtil::isNumeric("0"));
    EXPECT_TRUE(StringUtil::isNumeric("-42"));
    EXPECT_FALSE(StringUtil::isNumeric("abc"));
    EXPECT_FALSE(StringUtil::isNumeric("12a3"));
    EXPECT_FALSE(StringUtil::isNumeric(""));
}

TEST(StringUtilTest, asciiClassificationAndCase) {
    EXPECT_TRUE(StringUtil::isAsciiLower('a'));
    EXPECT_FALSE(StringUtil::isAsciiLower('A'));
    EXPECT_TRUE(StringUtil::isAsciiUpper('Z'));
    EXPECT_FALSE(StringUtil::isAsciiUpper('z'));
    EXPECT_TRUE(StringUtil::isAsciiAlpha('Q'));
    EXPECT_TRUE(StringUtil::isAsciiDigit('7'));
    EXPECT_TRUE(StringUtil::isAsciiAlnum('x'));
    EXPECT_FALSE(StringUtil::isAsciiAlnum('-'));

    EXPECT_EQ(StringUtil::asciiToLower('A'), 'a');
    EXPECT_EQ(StringUtil::asciiToLower('!'), '!');
    EXPECT_EQ(StringUtil::asciiToUpper('z'), 'Z');
    EXPECT_EQ(StringUtil::asciiToUpper('8'), '8');
}

TEST(StringUtilTest, urlUnreservedClassification) {
    EXPECT_TRUE(StringUtil::isUnreservedUrlChar('A'));
    EXPECT_TRUE(StringUtil::isUnreservedUrlChar('9'));
    EXPECT_TRUE(StringUtil::isUnreservedUrlChar('-'));
    EXPECT_TRUE(StringUtil::isUnreservedUrlChar('.'));
    EXPECT_TRUE(StringUtil::isUnreservedUrlChar('_'));
    EXPECT_TRUE(StringUtil::isUnreservedUrlChar('~'));
    EXPECT_FALSE(StringUtil::isUnreservedUrlChar(' '));
    EXPECT_FALSE(StringUtil::isUnreservedUrlChar('/'));
}

// ============================================================
// URL / percent 编码
// ============================================================

TEST(StringUtilTest, percentEncodeUsesUppercaseHex) {
    EXPECT_EQ(StringUtil::percentEncode("abcXYZ-._~09"), "abcXYZ-._~09");
    EXPECT_EQ(StringUtil::percentEncode("a b/c?d"), "a%20b%2Fc%3Fd");
    EXPECT_EQ(StringUtil::percentEncode("你好"), "%E4%BD%A0%E5%A5%BD");
}

TEST(StringUtilTest, percentDecodeRoundTrip) {
    auto decoded = StringUtil::percentDecode("a%20b%2Fc%3Fd");
    ASSERT_TRUE(decoded.is_ok()) << decoded.unwrap_err();
    EXPECT_EQ(decoded.unwrap(), "a b/c?d");

    auto utf8 = StringUtil::percentDecode("%E4%BD%A0%E5%A5%BD");
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err();
    EXPECT_EQ(utf8.unwrap(), "你好");
}

TEST(StringUtilTest, percentDecodeRejectsInvalidEscapes) {
    EXPECT_TRUE(StringUtil::percentDecode("%").is_err());
    EXPECT_TRUE(StringUtil::percentDecode("%2").is_err());
    EXPECT_TRUE(StringUtil::percentDecode("%GG").is_err());
}

TEST(StringUtilTest, urlComponentUsesPlusForSpace) {
    EXPECT_EQ(StringUtil::urlEncodeComponent("a b+c"), "a+b%2Bc");

    auto decoded = StringUtil::urlDecodeComponent("a+b%2Bc");
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
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zg=").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zg===").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zg=A").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zm=v").is_err());
    EXPECT_TRUE(StringUtil::base64UrlDecode("Zm9v!").is_err());
}

// ============================================================
// 前缀/后缀/包含
// ============================================================

TEST(StringUtilTest, startsWith) {
    EXPECT_TRUE(StringUtil::startsWith("hello world", "hello"));
    EXPECT_TRUE(StringUtil::startsWith("hello", "hello"));
    EXPECT_FALSE(StringUtil::startsWith("hello world", "world"));
    EXPECT_FALSE(StringUtil::startsWith("hel", "hello"));
    EXPECT_FALSE(StringUtil::startsWith("", "hello"));
    EXPECT_TRUE(StringUtil::startsWith("hello", ""));
}

TEST(StringUtilTest, endsWith) {
    EXPECT_TRUE(StringUtil::endsWith("hello world", "world"));
    EXPECT_TRUE(StringUtil::endsWith("hello", "hello"));
    EXPECT_FALSE(StringUtil::endsWith("hello world", "hello"));
    EXPECT_FALSE(StringUtil::endsWith("hel", "hello"));
    EXPECT_FALSE(StringUtil::endsWith("", "hello"));
    EXPECT_TRUE(StringUtil::endsWith("hello", ""));
}

TEST(StringUtilTest, contains) {
    EXPECT_TRUE(StringUtil::contains("hello world", "lo wo"));
    EXPECT_TRUE(StringUtil::contains("hello world", "hello"));
    EXPECT_TRUE(StringUtil::contains("hello world", "world"));
    EXPECT_FALSE(StringUtil::contains("hello world", "xyz"));
    EXPECT_TRUE(StringUtil::contains("hello", ""));
    EXPECT_FALSE(StringUtil::contains("", "hello"));
}
