#include <gtest/gtest.h>
#include <libca/str/char_util.hpp>

namespace ca::str {

// ============================================================================
// Utf8Char — 构造与编解码
// ============================================================================

TEST(Utf8CharTest, FromRawAscii) {
    auto ch = Utf8Char::fromRaw("A");
    EXPECT_EQ(ch.codePoint(), 0x41);
}

TEST(Utf8CharTest, FromRawMultiByte) {
    // '中' (U+4E2D) = 0xE4 0xB8 0xAD
    const u8 utf8[] = {0xE4, 0xB8, 0xAD};
    auto ch = Utf8Char::fromRaw(utf8);
    EXPECT_EQ(ch.codePoint(), 0x4E2D);
}

TEST(Utf8CharTest, FromRawEmoji) {
    // '😀' (U+1F600) = 0xF0 0x9F 0x98 0x80
    const u8 utf8[] = {0xF0, 0x9F, 0x98, 0x80};
    auto ch = Utf8Char::fromRaw(utf8);
    EXPECT_EQ(ch.codePoint(), 0x1F600);
}

TEST(Utf8CharTest, EncodeAscii) {
    Utf8Char ch(0x41);
    u8 buf[4] = {};
    auto n = ch.encode(buf);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(buf[0], 0x41);
}

TEST(Utf8CharTest, EncodeMultiByte) {
    Utf8Char ch(0x4E2D);  // '中'
    u8 buf[4] = {};
    auto n = ch.encode(buf);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(buf[0], 0xE4);
    EXPECT_EQ(buf[1], 0xB8);
    EXPECT_EQ(buf[2], 0xAD);
}

TEST(Utf8CharTest, EncodeEmoji) {
    Utf8Char ch(0x1F600);
    u8 buf[4] = {};
    auto n = ch.encode(buf);
    EXPECT_EQ(n, 4);
    EXPECT_EQ(buf[0], 0xF0);
    EXPECT_EQ(buf[3], 0x80);
}

TEST(Utf8CharTest, Equality) {
    Utf8Char a(0x41), b(0x41), c(0x42);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}

// ============================================================================
// Utf8Char — 字符分类（ASCII）
// ============================================================================

TEST(Utf8CharTest, IsAlphaAscii) {
    EXPECT_TRUE(Utf8Char('A').isAlpha());
    EXPECT_TRUE(Utf8Char('z').isAlpha());
    EXPECT_FALSE(Utf8Char('0').isAlpha());
    EXPECT_FALSE(Utf8Char(' ').isAlpha());
}

TEST(Utf8CharTest, IsDigitAscii) {
    EXPECT_TRUE(Utf8Char('0').isDigit());
    EXPECT_TRUE(Utf8Char('9').isDigit());
    EXPECT_FALSE(Utf8Char('A').isDigit());
}

TEST(Utf8CharTest, IsAlnumAscii) {
    EXPECT_TRUE(Utf8Char('A').isAlnum());
    EXPECT_TRUE(Utf8Char('5').isAlnum());
    EXPECT_FALSE(Utf8Char(' ').isAlnum());
}

TEST(Utf8CharTest, IsSpaceAscii) {
    EXPECT_TRUE(Utf8Char(' ').isSpace());
    EXPECT_TRUE(Utf8Char('\t').isSpace());
    EXPECT_TRUE(Utf8Char('\n').isSpace());
    EXPECT_FALSE(Utf8Char('A').isSpace());
}

TEST(Utf8CharTest, IsLowerUpperAscii) {
    EXPECT_TRUE(Utf8Char('a').isLower());
    EXPECT_FALSE(Utf8Char('A').isLower());
    EXPECT_TRUE(Utf8Char('Z').isUpper());
    EXPECT_FALSE(Utf8Char('z').isUpper());
}

TEST(Utf8CharTest, IsPunctAscii) {
    EXPECT_TRUE(Utf8Char('.').isPunct());
    EXPECT_TRUE(Utf8Char(',').isPunct());
    EXPECT_TRUE(Utf8Char('!').isPunct());
    EXPECT_FALSE(Utf8Char('A').isPunct());
}

TEST(Utf8CharTest, IsPrintAscii) {
    EXPECT_TRUE(Utf8Char('A').isPrint());
    EXPECT_FALSE(Utf8Char(0x00).isPrint());  // NUL
    EXPECT_FALSE(Utf8Char(0x1F).isPrint());  // 控制字符
}

TEST(Utf8CharTest, IsCntrlAscii) {
    EXPECT_TRUE(Utf8Char(0x00).isCntrl());
    EXPECT_TRUE(Utf8Char(0x7F).isCntrl());  // DEL
    EXPECT_FALSE(Utf8Char('A').isCntrl());
}

TEST(Utf8CharTest, IsXDigitAscii) {
    EXPECT_TRUE(Utf8Char('0').isXDigit());
    EXPECT_TRUE(Utf8Char('F').isXDigit());
    EXPECT_TRUE(Utf8Char('f').isXDigit());
    EXPECT_FALSE(Utf8Char('G').isXDigit());
}

// ============================================================================
// Utf8Char — 字符分类（非 ASCII）
// ============================================================================

TEST(Utf8CharTest, IsAlphaCjk) {
    EXPECT_TRUE(Utf8Char(0x4E2D).isAlpha());  // '中'
    EXPECT_TRUE(Utf8Char(0x4E16).isAlpha());  // '世'
    EXPECT_TRUE(Utf8Char(0x3042).isAlpha());  // 平假名 'あ'
    EXPECT_TRUE(Utf8Char(0x30A2).isAlpha());  // 片假名 'ア'
}

TEST(Utf8CharTest, IsAlphaExtendedLatin) {
    // é = U+00E9
    EXPECT_TRUE(Utf8Char(0x00E9).isAlpha());
    // ü = U+00FC
    EXPECT_TRUE(Utf8Char(0x00FC).isAlpha());
}

TEST(Utf8CharTest, IsDigitFullwidth) {
    // 全角数字 ０ = U+FF10
    EXPECT_TRUE(Utf8Char(0xFF10).isDigit());
    // 全角数字 ９ = U+FF19
    EXPECT_TRUE(Utf8Char(0xFF19).isDigit());
}

TEST(Utf8CharTest, IsPrintNonAscii) {
    // CJK 是可打印的
    EXPECT_TRUE(Utf8Char(0x4E2D).isPrint());
    // C1 控制字符不是
    EXPECT_FALSE(Utf8Char(0x80).isPrint());
}

TEST(Utf8CharTest, IsCntrlNonAscii) {
    // C1 控制字符
    EXPECT_TRUE(Utf8Char(0x80).isCntrl());
    EXPECT_FALSE(Utf8Char(0x4E2D).isCntrl());
}

// ============================================================================
// Utf8Char — 大小写转换
// ============================================================================

TEST(Utf8CharTest, ToLowerUpperAscii) {
    EXPECT_EQ(Utf8Char('A').toLower().codePoint(), 'a');
    EXPECT_EQ(Utf8Char('z').toUpper().codePoint(), 'Z');
}

// ToLower/ToUpper 对非 ASCII 字符的行为依赖当前 locale
// 在默认 "C" locale 下仅 ASCII 范围会被转换

// ============================================================================
// Utf16Char
// ============================================================================

TEST(Utf16CharTest, SurrogateDetection) {
    Utf16Char lead(0xD83D);  // 😀 的高位代理
    EXPECT_TRUE(lead.isSurrogate());
    EXPECT_TRUE(lead.isLeadSurrogate());
    EXPECT_FALSE(lead.isTrailSurrogate());

    Utf16Char trail(0xDE00);
    EXPECT_TRUE(trail.isSurrogate());
    EXPECT_TRUE(trail.isTrailSurrogate());
    EXPECT_FALSE(trail.isLeadSurrogate());

    Utf16Char bmp('A');
    EXPECT_FALSE(bmp.isSurrogate());
    EXPECT_TRUE(bmp.isBmp());
}

TEST(Utf16CharTest, DecodePair) {
    // U+1F600 😀 = 0xD83D 0xDE00
    auto cp = Utf16Char::decodePair(Utf16Char(0xD83D), Utf16Char(0xDE00));
    EXPECT_EQ(cp, 0x1F600);
}

TEST(Utf16CharTest, EncodePair) {
    Utf16Char high, low;
    EXPECT_TRUE(Utf16Char::encodePair(0x1F600, high, low));
    EXPECT_EQ(high.unit(), 0xD83D);
    EXPECT_EQ(low.unit(), 0xDE00);

    // BMP 不应被编码为代理对
    EXPECT_FALSE(Utf16Char::encodePair(0x4E2D, high, low));
}

// ============================================================================
// 自由函数便捷接口
// ============================================================================

TEST(CharUtilFreeTest, FreeFunctions) {
    EXPECT_TRUE(isAlpha('A'));
    EXPECT_TRUE(isDigit('5'));
    EXPECT_TRUE(isAlnum('B'));
    EXPECT_TRUE(isSpace(' '));
    EXPECT_TRUE(isLower('a'));
    EXPECT_TRUE(isUpper('Z'));
    EXPECT_FALSE(isAlpha(' '));

    // 非 ASCII
    EXPECT_TRUE(isAlpha(0x4E2D));  // '中'
    EXPECT_TRUE(isDigit(0xFF10));  // 全角 ０

    // 大小写转换
    EXPECT_EQ(toLower('A'), 'a');
    EXPECT_EQ(toUpper('z'), 'Z');
}

}  // namespace ca::str
