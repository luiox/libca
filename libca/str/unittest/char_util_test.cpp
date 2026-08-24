#include <gtest/gtest.h>
#include <libca/str/char_util.hpp>

namespace ca::str {

// ============================================================================
// Utf8Char — 构造与编解码
// ============================================================================

TEST(Utf8CharTest, FromRawAscii) {
    auto ch = Utf8Char::from_raw("A");
    EXPECT_EQ(ch.code_point(), 0x41);
}

TEST(Utf8CharTest, FromRawMultiByte) {
    // '中' (U+4E2D) = 0xE4 0xB8 0xAD
    const u8 utf8[] = {0xE4, 0xB8, 0xAD};
    auto ch = Utf8Char::from_raw(utf8);
    EXPECT_EQ(ch.code_point(), 0x4E2D);
}

TEST(Utf8CharTest, FromRawEmoji) {
    // '😀' (U+1F600) = 0xF0 0x9F 0x98 0x80
    const u8 utf8[] = {0xF0, 0x9F, 0x98, 0x80};
    auto ch = Utf8Char::from_raw(utf8);
    EXPECT_EQ(ch.code_point(), 0x1F600);
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
    EXPECT_TRUE(Utf8Char('A').is_alpha());
    EXPECT_TRUE(Utf8Char('z').is_alpha());
    EXPECT_FALSE(Utf8Char('0').is_alpha());
    EXPECT_FALSE(Utf8Char(' ').is_alpha());
}

TEST(Utf8CharTest, IsDigitAscii) {
    EXPECT_TRUE(Utf8Char('0').is_digit());
    EXPECT_TRUE(Utf8Char('9').is_digit());
    EXPECT_FALSE(Utf8Char('A').is_digit());
}

TEST(Utf8CharTest, IsAlnumAscii) {
    EXPECT_TRUE(Utf8Char('A').is_alnum());
    EXPECT_TRUE(Utf8Char('5').is_alnum());
    EXPECT_FALSE(Utf8Char(' ').is_alnum());
}

TEST(Utf8CharTest, IsSpaceAscii) {
    EXPECT_TRUE(Utf8Char(' ').is_space());
    EXPECT_TRUE(Utf8Char('\t').is_space());
    EXPECT_TRUE(Utf8Char('\n').is_space());
    EXPECT_FALSE(Utf8Char('A').is_space());
}

TEST(Utf8CharTest, IsLowerUpperAscii) {
    EXPECT_TRUE(Utf8Char('a').is_lower());
    EXPECT_FALSE(Utf8Char('A').is_lower());
    EXPECT_TRUE(Utf8Char('Z').is_upper());
    EXPECT_FALSE(Utf8Char('z').is_upper());
}

TEST(Utf8CharTest, IsPunctAscii) {
    EXPECT_TRUE(Utf8Char('.').is_punct());
    EXPECT_TRUE(Utf8Char(',').is_punct());
    EXPECT_TRUE(Utf8Char('!').is_punct());
    EXPECT_FALSE(Utf8Char('A').is_punct());
}

TEST(Utf8CharTest, IsPrintAscii) {
    EXPECT_TRUE(Utf8Char('A').is_print());
    EXPECT_FALSE(Utf8Char(0x00).is_print());  // NUL
    EXPECT_FALSE(Utf8Char(0x1F).is_print());  // 控制字符
}

TEST(Utf8CharTest, IsCntrlAscii) {
    EXPECT_TRUE(Utf8Char(0x00).is_cntrl());
    EXPECT_TRUE(Utf8Char(0x7F).is_cntrl());  // DEL
    EXPECT_FALSE(Utf8Char('A').is_cntrl());
}

TEST(Utf8CharTest, IsXDigitAscii) {
    EXPECT_TRUE(Utf8Char('0').is_xdigit());
    EXPECT_TRUE(Utf8Char('F').is_xdigit());
    EXPECT_TRUE(Utf8Char('f').is_xdigit());
    EXPECT_FALSE(Utf8Char('G').is_xdigit());
}

// ============================================================================
// Utf8Char — 字符分类（非 ASCII）
// ============================================================================

TEST(Utf8CharTest, IsAlphaCjk) {
    EXPECT_TRUE(Utf8Char(0x4E2D).is_alpha());  // '中'
    EXPECT_TRUE(Utf8Char(0x4E16).is_alpha());  // '世'
    EXPECT_TRUE(Utf8Char(0x3042).is_alpha());  // 平假名 'あ'
    EXPECT_TRUE(Utf8Char(0x30A2).is_alpha());  // 片假名 'ア'
}

TEST(Utf8CharTest, IsAlphaExtendedLatin) {
    // é = U+00E9
    EXPECT_TRUE(Utf8Char(0x00E9).is_alpha());
    // ü = U+00FC
    EXPECT_TRUE(Utf8Char(0x00FC).is_alpha());
    // Latin Extended-C: Ⱡ = U+2C60
    EXPECT_TRUE(Utf8Char(0x2C60).is_alpha());
}

TEST(Utf8CharTest, IsAlphaInvalidCodePointDoesNotTruncateToAscii) {
    EXPECT_FALSE(Utf8Char(0x110041).is_alpha());
}

TEST(Utf8CharTest, IsDigitFullwidth) {
    // 全角数字 ０ = U+FF10
    EXPECT_TRUE(Utf8Char(0xFF10).is_digit());
    // 全角数字 ９ = U+FF19
    EXPECT_TRUE(Utf8Char(0xFF19).is_digit());
}

TEST(Utf8CharTest, IsPrintNonAscii) {
    // CJK 是可打印的
    EXPECT_TRUE(Utf8Char(0x4E2D).is_print());
    // C1 控制字符不是
    EXPECT_FALSE(Utf8Char(0x80).is_print());
}

TEST(Utf8CharTest, IsCntrlNonAscii) {
    // C1 控制字符
    EXPECT_TRUE(Utf8Char(0x80).is_cntrl());
    EXPECT_FALSE(Utf8Char(0x4E2D).is_cntrl());
}

// ============================================================================
// Utf8Char — 大小写转换
// ============================================================================

TEST(Utf8CharTest, ToLowerUpperAscii) {
    EXPECT_EQ(Utf8Char('A').to_lower().code_point(), 'a');
    EXPECT_EQ(Utf8Char('z').to_upper().code_point(), 'Z');
}

// ToLower/ToUpper 对非 ASCII 字符的行为依赖当前 locale
// 在默认 "C" locale 下仅 ASCII 范围会被转换

// ============================================================================
// Utf16Char
// ============================================================================


// wint_t 为 16 位（Windows）时增补平面码点截断：分类函数曾把 U+2000A 截断成
// U+000A（is_space 误报 true）、转换函数曾把 U+10061 截断成 'a' 后 to_upper
// 返回 'A'（数据损坏）。全部按「截断不可信」处理：分类 false、转换不变。
TEST(Utf8CharTest, AstralCodePointsNotTruncatedByWideCtype) {
    // 分类：一律 false（而非按截断值分类）
    EXPECT_FALSE(Utf8Char(0x2000A).is_space());   // 截断值 U+000A 是空白
    EXPECT_FALSE(Utf8Char(0x10061).is_lower());   // 截断值 'a' 是小写
    EXPECT_FALSE(Utf8Char(0x10041).is_upper());   // 截断值 'A' 是大写
    EXPECT_FALSE(Utf8Char(0x10030).is_digit());   // 截断值 '0' 是数字
    EXPECT_FALSE(Utf8Char(0x10041).is_xdigit());  // 截断值 'A' 是十六进制
    EXPECT_FALSE(Utf8Char(0x10061).is_punct());
    EXPECT_FALSE(Utf8Char(0x10000).is_cntrl());

    // 转换：保持原码点（而非产出截断映射的 ASCII）
    EXPECT_EQ(Utf8Char(0x10061).to_upper().code_point(), 0x10061u);
    EXPECT_EQ(Utf8Char(0x10041).to_lower().code_point(), 0x10041u);

    // 对照：ASCII 路径映射不受影响（BMP 非_ascii 映射依赖 locale，不在断言范围）
    EXPECT_EQ(Utf8Char(u'a').to_upper().code_point(), u'A');
}

TEST(Utf16CharTest, SurrogateDetection) {
    Utf16Char lead(0xD83D);  // 😀 的高位代理
    EXPECT_TRUE(lead.is_surrogate());
    EXPECT_TRUE(lead.is_lead_surrogate());
    EXPECT_FALSE(lead.is_trail_surrogate());

    Utf16Char trail(0xDE00);
    EXPECT_TRUE(trail.is_surrogate());
    EXPECT_TRUE(trail.is_trail_surrogate());
    EXPECT_FALSE(trail.is_lead_surrogate());

    Utf16Char bmp('A');
    EXPECT_FALSE(bmp.is_surrogate());
    EXPECT_TRUE(bmp.is_bmp());
}

TEST(Utf16CharTest, DecodePair) {
    // U+1F600 😀 = 0xD83D 0xDE00
    auto cp = Utf16Char::decode_pair(Utf16Char(0xD83D), Utf16Char(0xDE00));
    EXPECT_EQ(cp, 0x1F600);
}

TEST(Utf16CharTest, EncodePair) {
    Utf16Char high, low;
    EXPECT_TRUE(Utf16Char::encode_pair(0x1F600, high, low));
    EXPECT_EQ(high.unit(), 0xD83D);
    EXPECT_EQ(low.unit(), 0xDE00);

    // BMP 不应被编码为代理对
    EXPECT_FALSE(Utf16Char::encode_pair(0x4E2D, high, low));
}

// ============================================================================
// 自由函数便捷接口
// ============================================================================

TEST(CharUtilFreeTest, FreeFunctions) {
    EXPECT_TRUE(is_alpha('A'));
    EXPECT_TRUE(is_digit('5'));
    EXPECT_TRUE(is_alnum('B'));
    EXPECT_TRUE(is_space(' '));
    EXPECT_TRUE(is_lower('a'));
    EXPECT_TRUE(is_upper('Z'));
    EXPECT_FALSE(is_alpha(' '));

    // 非 ASCII
    EXPECT_TRUE(is_alpha(0x4E2D));  // '中'
    EXPECT_TRUE(is_digit(0xFF10));  // 全角 ０

    // 大小写转换
    EXPECT_EQ(to_lower('A'), 'a');
    EXPECT_EQ(to_upper('z'), 'Z');
}

}  // namespace ca::str
