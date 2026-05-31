#include <gtest/gtest.h>
#include <libca/str/conversion.hpp>

namespace ca::str {

// ============================================================================
// Utf8String ↔ CString
// ============================================================================

TEST(ConversionTest, Utf8ToCString) {
    Utf8String u8("Hello 世界");
    auto cs = toCString(u8.ref());
    EXPECT_EQ(cs.length(), u8.byteLength());
    EXPECT_STREQ(cs.cStr(), "Hello 世界");
}

TEST(ConversionTest, CStringToUtf8) {
    CString cs("ABC");
    auto u8 = toUtf8String(cs.ref());
    EXPECT_EQ(u8.length(), 3);
    EXPECT_EQ(u8.byteLength(), 3);
    EXPECT_EQ(u8.codePointAt(0), 'A');
}

TEST(ConversionTest, Utf8CStringRoundtrip) {
    Utf8String orig("Hello 世界 🌍");
    auto cs = toCString(orig.ref());
    auto back = toUtf8String(cs.ref());
    EXPECT_EQ(orig, back);
}

// ============================================================================
// Utf8String ↔ WString
// ============================================================================

TEST(ConversionTest, Utf8ToWString) {
    Utf8String u8("Hello");
    auto ws = toWString(u8.ref());
    EXPECT_EQ(ws.length(), 5);
}

TEST(ConversionTest, WStringToUtf8) {
    WString ws(L"ABC");
    auto u8 = toUtf8String(ws.ref());
    EXPECT_EQ(u8.length(), 3);
    EXPECT_EQ(u8.codePointAt(0), 'A');
}

TEST(ConversionTest, Utf8WStringRoundtrip) {
    Utf8String orig("Hello World");
    auto ws = toWString(orig.ref());
    auto back = toUtf8String(ws.ref());
    EXPECT_EQ(orig, back);
}

TEST(ConversionTest, Utf8WStringUnicodeRoundtrip) {
    Utf8String orig("你好世界 😀");
    auto ws = toWString(orig.ref());
    auto back = toUtf8String(ws.ref());
    EXPECT_EQ(orig, back);
}

// ============================================================================
// CString ↔ WString
// ============================================================================

TEST(ConversionTest, CStringToWString) {
    CString cs("ABC");
    auto ws = toWString(cs.ref());
    EXPECT_EQ(ws.length(), 3);
}

TEST(ConversionTest, WStringToCString) {
    WString ws(L"Hello");
    auto cs = toCString(ws.ref());
    EXPECT_EQ(cs.length(), 5);
    EXPECT_STREQ(cs.cStr(), "Hello");
}

TEST(ConversionTest, CStringWStringRoundtrip) {
    CString orig("Hello World");
    auto ws = toWString(orig.ref());
    auto back = toCString(ws.ref());
    EXPECT_TRUE(orig == back);
}

// ============================================================================
// UTF-8 ↔ raw UTF-16 工具
// ============================================================================

TEST(ConversionTest, Utf8ToUtf16Length) {
    // "AB" → 2 u16
    const u8 ascii[] = {0x41, 0x42};
    EXPECT_EQ(utf8ToUtf16Length(ascii, 2), 2);

    // '😀' (4字节) → 2 u16 (代理对)
    const u8 emoji[] = {0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(utf8ToUtf16Length(emoji, 4), 2);

    // "A😀" → 1 + 2 = 3 u16
    const u8 mix[] = {0x41, 0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(utf8ToUtf16Length(mix, 5), 3);
}

TEST(ConversionTest, Utf8ToUtf16) {
    const u8 input[] = {0x41, 0xF0, 0x9F, 0x98, 0x80};  // "A😀"
    u16 output[4] = {};
    auto n = utf8ToUtf16(input, 5, output);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(output[0], 0x41);
    EXPECT_EQ(output[1], 0xD83D);  // 😀 高位代理
    EXPECT_EQ(output[2], 0xDE00);  // 😀 低位代理
}

TEST(ConversionTest, Utf16ToUtf8Length) {
    // "AB" → 2 bytes
    const u16 ascii[] = {0x41, 0x42};
    EXPECT_EQ(utf16ToUtf8Length(ascii, 2), 2);

    // 代理对 → 4 bytes
    const u16 surrogates[] = {0xD83D, 0xDE00};
    EXPECT_EQ(utf16ToUtf8Length(surrogates, 2), 4);
}

TEST(ConversionTest, Utf16ToUtf8) {
    const u16 input[] = {0x41, 0xD83D, 0xDE00};  // "A😀"
    u8 output[8] = {};
    auto n = utf16ToUtf8(input, 3, output);
    EXPECT_EQ(n, 5);
    EXPECT_EQ(output[0], 0x41);
    // bytes 1-4 should be 😀
    EXPECT_EQ(output[1], 0xF0);
    EXPECT_EQ(output[2], 0x9F);
    EXPECT_EQ(output[3], 0x98);
    EXPECT_EQ(output[4], 0x80);
}

TEST(ConversionTest, Utf16Utf8Roundtrip) {
    const u8 orig[] = {0x41, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80};
    // UTF-8 → UTF-16
    u16 utf16[8] = {};
    auto n16 = utf8ToUtf16(orig, 8, utf16);
    EXPECT_GT(n16, 0);
    // UTF-16 → UTF-8
    u8 utf8[12] = {};
    auto n8 = utf16ToUtf8(utf16, n16, utf8);
    EXPECT_EQ(n8, 8);
    EXPECT_TRUE(std::memcmp(orig, utf8, 8) == 0);
}

// ============================================================================
// 空字符串转换
// ============================================================================

TEST(ConversionTest, EmptyUtf8ToCString) {
    Utf8String empty;
    auto cs = toCString(empty.ref());
    EXPECT_TRUE(cs.isEmpty());
}

TEST(ConversionTest, EmptyCStringToUtf8) {
    CString cs("");
    auto u8 = toUtf8String(cs.ref());
    EXPECT_TRUE(u8.isEmpty());
}

TEST(ConversionTest, EmptyWStringToUtf8) {
    WString ws(L"");
    auto u8 = toUtf8String(ws.ref());
    EXPECT_TRUE(u8.isEmpty());
}

}  // namespace ca::str
