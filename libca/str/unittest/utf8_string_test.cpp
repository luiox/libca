#include <gtest/gtest.h>
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_util.hpp"

#include <string>
#include <cstring>

namespace ca::str {

// ============================================================================
// 工具函数测试
// ============================================================================

TEST(Utf8UtilTest, CodePointBytes_Ascii) {
    EXPECT_EQ(utf8CodePointBytes(0x00), 1);
    EXPECT_EQ(utf8CodePointBytes(0x41), 1);  // 'A'
    EXPECT_EQ(utf8CodePointBytes(0x7F), 1);
}

TEST(Utf8UtilTest, CodePointBytes_MultiByte) {
    EXPECT_EQ(utf8CodePointBytes(0xC0), 2);
    EXPECT_EQ(utf8CodePointBytes(0xDF), 2);
    EXPECT_EQ(utf8CodePointBytes(0xE0), 3);
    EXPECT_EQ(utf8CodePointBytes(0xEF), 3);
    EXPECT_EQ(utf8CodePointBytes(0xF0), 4);
    EXPECT_EQ(utf8CodePointBytes(0xF7), 4);
}

TEST(Utf8UtilTest, CodePointBytes_Invalid) {
    EXPECT_EQ(utf8CodePointBytes(0xF8), 0);  // 11111xxx 非法
    EXPECT_EQ(utf8CodePointBytes(0xFC), 0);
    EXPECT_EQ(utf8CodePointBytes(0xFE), 0);
    EXPECT_EQ(utf8CodePointBytes(0xFF), 0);
}

TEST(Utf8UtilTest, DecodeCodePoint) {
    // ASCII: 'A' = U+0041
    u8 ascii[] = {0x41};
    EXPECT_EQ(utf8DecodeCodePoint(ascii), 0x41);

    // 2 字节: U+00A9 (©)
    u8 two[] = {0xC2, 0xA9};
    EXPECT_EQ(utf8DecodeCodePoint(two), 0xA9);

    // 3 字节: U+4E2D (中)
    u8 three[] = {0xE4, 0xB8, 0xAD};
    EXPECT_EQ(utf8DecodeCodePoint(three), 0x4E2D);

    // 4 字节: U+1F600 (😀)
    u8 four[] = {0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(utf8DecodeCodePoint(four), 0x1F600);
}

TEST(Utf8UtilTest, EncodeCodePoint_Ascii) {
    u8 buf[4] = {};
    auto len = utf8EncodeCodePoint(0x41, buf);
    EXPECT_EQ(len, 1);
    EXPECT_EQ(buf[0], 0x41);
}

TEST(Utf8UtilTest, EncodeCodePoint_MultiByte) {
    u8 buf[4] = {};

    // U+00A9 → 0xC2 0xA9
    auto len = utf8EncodeCodePoint(0xA9, buf);
    EXPECT_EQ(len, 2);
    EXPECT_EQ(buf[0], 0xC2);
    EXPECT_EQ(buf[1], 0xA9);

    // U+4E2D → 0xE4 0xB8 0xAD
    len = utf8EncodeCodePoint(0x4E2D, buf);
    EXPECT_EQ(len, 3);
    EXPECT_EQ(buf[0], 0xE4);
    EXPECT_EQ(buf[1], 0xB8);
    EXPECT_EQ(buf[2], 0xAD);

    // U+1F600 → 0xF0 0x9F 0x98 0x80
    len = utf8EncodeCodePoint(0x1F600, buf);
    EXPECT_EQ(len, 4);
    EXPECT_EQ(buf[0], 0xF0);
    EXPECT_EQ(buf[1], 0x9F);
    EXPECT_EQ(buf[2], 0x98);
    EXPECT_EQ(buf[3], 0x80);
}

TEST(Utf8UtilTest, EncodeCodePoint_Invalid) {
    u8 buf[4] = {};

    // 代理项非法
    EXPECT_EQ(utf8EncodeCodePoint(0xD800, buf), 0);
    EXPECT_EQ(utf8EncodeCodePoint(0xDFFF, buf), 0);

    // 超出范围
    EXPECT_EQ(utf8EncodeCodePoint(0x110000, buf), 0);
}

TEST(Utf8UtilTest, CountCodePoints) {
    // "A中😀" = 0x41 + 3 + 4 = 8 bytes, 3 code points
    u8 utf8[] = {0x41, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80};
    usize invalidPos = 999;
    EXPECT_EQ(utf8CountCodePoints(utf8, 8, &invalidPos), 3);
    EXPECT_EQ(invalidPos, 999);  // 未修改
}

TEST(Utf8UtilTest, CountCodePoints_Invalid) {
    // 非法续字节
    u8 bad[] = {0xE4, 0x00, 0xAD};
    usize invalidPos = 999;
    EXPECT_EQ(utf8CountCodePoints(bad, 3, &invalidPos), 0);
    EXPECT_EQ(invalidPos, 1);  // 第二个字节非法
}

TEST(Utf8UtilTest, IsValid) {
    u8 valid[] = {0x41, 0xE4, 0xB8, 0xAD};
    EXPECT_TRUE(utf8IsValid(valid, 4));

    u8 invalid[] = {0xE4, 0x00, 0xAD};
    EXPECT_FALSE(utf8IsValid(invalid, 3));
}

// ============================================================================
// Utf8StringRef 测试
// ============================================================================

TEST(Utf8StringRefTest, DefaultConstructor) {
    Utf8StringRef ref;
    EXPECT_TRUE(ref.isEmpty());
    EXPECT_EQ(ref.length(), 0);
    EXPECT_EQ(ref.byteLength(), 0);
    EXPECT_EQ(ref.data(), nullptr);
}

TEST(Utf8StringRefTest, ConstructFromData) {
    u8 data[] = {0x41, 0x42, 0x43};
    Utf8StringRef ref(data, 3, 3);
    EXPECT_EQ(ref.length(), 3);
    EXPECT_EQ(ref.byteLength(), 3);
    EXPECT_FALSE(ref.isEmpty());
    EXPECT_EQ(ref.data(), data);
}

TEST(Utf8StringRefTest, LiteralSuffixRef) {
    using namespace ca::str::literals;

    auto ref = "tesst"_utf8_ref;
    EXPECT_EQ(ref.length(), 5);
    EXPECT_EQ(ref.byteLength(), 5);
    EXPECT_EQ(ref.codePointAt(0), 0x74);
    EXPECT_EQ(ref.codePointAt(4), 0x74);
}

TEST(Utf8StringRefTest, ByteAt) {
    u8 data[] = {0x41, 0xE4, 0xB8, 0xAD};
    Utf8StringRef ref(data, 4, 2);
    EXPECT_EQ(ref.byteAt(0), 0x41);
    EXPECT_EQ(ref.byteAt(1), 0xE4);
}

TEST(Utf8StringRefTest, CodePointAt) {
    // "A中" = 0x41 (1B) + 0xE4 0xB8 0xAD (3B)
    u8 data[] = {0x41, 0xE4, 0xB8, 0xAD};
    Utf8StringRef ref(data, 4, 2);
    EXPECT_EQ(ref.codePointAt(0), 0x41);       // 'A'
    EXPECT_EQ(ref.codePointAt(1), 0x4E2D);     // '中'
}

TEST(Utf8StringRefTest, SliceByBytes) {
    // "AB中" = 0x41 0x42 0xE4 0xB8 0xAD
    u8 data[] = {0x41, 0x42, 0xE4, 0xB8, 0xAD};
    Utf8StringRef ref(data, 5, 3);

    // 取字节 [0..2) → "AB"
    auto s1 = ref.slice(0, 2);
    EXPECT_EQ(s1.byteLength(), 2);
    EXPECT_EQ(s1.length(), 2);
    EXPECT_EQ(s1.byteAt(0), 0x41);
    EXPECT_EQ(s1.byteAt(1), 0x42);

    // 取字节 [2..5) → "中"
    auto s2 = ref.slice(2, 5);
    EXPECT_EQ(s2.byteLength(), 3);
    EXPECT_EQ(s2.length(), 1);
    EXPECT_EQ(s2.codePointAt(0), 0x4E2D);
}

TEST(Utf8StringRefTest, SliceByCp) {
    u8 data[] = {0x41, 0x42, 0xE4, 0xB8, 0xAD};  // "AB中"
    Utf8StringRef ref(data, 5, 3);

    // 取第 1~2 码点 → "B中"
    auto s = ref.sliceByCp(1, 2);
    EXPECT_EQ(s.byteLength(), 4);
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.codePointAt(0), 0x42);
    EXPECT_EQ(s.codePointAt(1), 0x4E2D);
}

TEST(Utf8StringRefTest, Compare) {
    u8 abc[] = {0x41, 0x42, 0x43};  // "ABC"
    u8 abd[] = {0x41, 0x42, 0x44};  // "ABD"

    Utf8StringRef r1(abc, 3, 3);
    Utf8StringRef r2(abd, 3, 3);

    EXPECT_LT(r1.compare(r2), 0);
    EXPECT_GT(r2.compare(r1), 0);
    EXPECT_EQ(r1.compare(r1), 0);
}

TEST(Utf8StringRefTest, Equals) {
    u8 a[] = {0x41, 0x42};
    u8 b[] = {0x41, 0x42};
    u8 c[] = {0x41, 0x43};

    Utf8StringRef r1(a, 2, 2);
    Utf8StringRef r2(b, 2, 2);
    Utf8StringRef r3(c, 2, 2);

    EXPECT_TRUE(r1.equals(r2));
    EXPECT_FALSE(r1.equals(r3));
    EXPECT_TRUE(r1 == r2);
    EXPECT_TRUE(r1 != r3);
}


// ============================================================================
// Utf8String 测试
// ============================================================================

TEST(Utf8StringTest, DefaultConstructor) {
    Utf8String s;
    EXPECT_TRUE(s.isEmpty());
    EXPECT_EQ(s.length(), 0);
    EXPECT_EQ(s.byteLength(), 0);
    EXPECT_NE(s.data(), nullptr);   // 内部有 '\0'
    EXPECT_EQ(s.cStr()[0], '\0');
}

TEST(Utf8StringTest, ConstructFromBytes) {
    u8 hello[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};  // "你好"
    Utf8String s(hello, 6);
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.byteLength(), 6);
    EXPECT_FALSE(s.isEmpty());
}

TEST(Utf8StringTest, ConstructFromCStr) {
    Utf8String s("你好");
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.byteLength(), 6);
    EXPECT_STREQ(s.cStr(), "你好");
}

TEST(Utf8StringTest, ConstructFromCStr_Null) {
    Utf8String s(static_cast<const char*>(nullptr));
    EXPECT_TRUE(s.isEmpty());
}

TEST(Utf8StringTest, Clone) {
    Utf8String s1("Hello");
    Utf8String s2 = s1.clone();
    EXPECT_EQ(s2.length(), 5);
    EXPECT_EQ(s2.byteLength(), 5);
    EXPECT_STREQ(s2.cStr(), "Hello");
    // 不同缓冲区
    EXPECT_NE(s1.data(), s2.data());
}

TEST(Utf8StringTest, MoveConstructor) {
    Utf8String s1("ABC");
    auto* originalData = s1.data();
    Utf8String s2(std::move(s1));
    EXPECT_EQ(s2.length(), 3);
    EXPECT_EQ(s2.data(), originalData);
    EXPECT_TRUE(s1.isEmpty());   // 源被清空
}

TEST(Utf8StringTest, CloneAssignment) {
    Utf8String s1("Hello");
    Utf8String s2 = s1.clone();
    EXPECT_STREQ(s2.cStr(), "Hello");
    EXPECT_NE(s1.data(), s2.data());
}

TEST(Utf8StringTest, MoveAssignment) {
    Utf8String s1("Hello");
    Utf8String s2;
    auto* originalData = s1.data();
    s2 = std::move(s1);
    EXPECT_EQ(s2.data(), originalData);
    EXPECT_TRUE(s1.isEmpty());
}

TEST(Utf8StringTest, MoveSelfAssignment) {
    Utf8String s("Test");
    s = std::move(s);
    EXPECT_STREQ(s.cStr(), "Test");
}

TEST(Utf8StringTest, FromCodePoint) {
    auto s = Utf8String::fromCodePoint(0x4E2D);  // '中'
    EXPECT_EQ(s.length(), 1);
    EXPECT_EQ(s.byteLength(), 3);
    EXPECT_STREQ(s.cStr(), "中");
}

TEST(Utf8StringTest, FromCodePoint_Invalid) {
    EXPECT_THROW(Utf8String::fromCodePoint(0xD800), std::runtime_error);
    EXPECT_THROW(Utf8String::fromCodePoint(0x110000), std::runtime_error);
}

TEST(Utf8StringTest, FromUtf8) {
    u8 data[] = {0xE4, 0xB8, 0xAD, 0xE5, 0x9B, 0xBD};  // "中国"
    auto s = Utf8String::fromUtf8(data, 6);
    EXPECT_EQ(s.length(), 2);
    EXPECT_STREQ(s.cStr(), "中国");
}

TEST(Utf8StringTest, ByteAt) {
    Utf8String s("ABC");
    EXPECT_EQ(s.byteAt(0), 0x41);
    EXPECT_EQ(s.byteAt(1), 0x42);
    EXPECT_EQ(s.byteAt(2), 0x43);
}

TEST(Utf8StringTest, CodePointAt) {
    Utf8String s("A中😀");
    EXPECT_EQ(s.codePointAt(0), 0x41);     // 'A'
    EXPECT_EQ(s.codePointAt(1), 0x4E2D);   // '中'
    EXPECT_EQ(s.codePointAt(2), 0x1F600);  // '😀'
}

TEST(Utf8StringTest, CStr_NullTerminated) {
    Utf8String s("Hello");
    EXPECT_STREQ(s.cStr(), "Hello");
    // 验证 cStr() 的返回值确实指向内部缓冲区
    EXPECT_EQ(reinterpret_cast<const u8*>(s.cStr()), s.data());
}

TEST(Utf8StringTest, Ref) {
    Utf8String s("Test");
    auto ref = s.ref();
    EXPECT_EQ(ref.length(), s.length());
    EXPECT_EQ(ref.byteLength(), s.byteLength());
    EXPECT_EQ(ref.data(), s.data());
}

TEST(Utf8StringTest, Slice) {
    // "AB中" → slice(2,5) → "中"
    Utf8String s("AB中");
    auto ref = s.slice(2, 5);
    EXPECT_EQ(ref.length(), 1);
    EXPECT_EQ(ref.byteLength(), 3);
    EXPECT_EQ(ref.codePointAt(0), 0x4E2D);
}

TEST(Utf8StringTest, SliceByCp) {
    Utf8String s("AB中");
    // 取第 0~1 码点 → "AB"
    auto ref = s.sliceByCp(0, 2);
    EXPECT_EQ(ref.length(), 2);
    EXPECT_EQ(ref.byteLength(), 2);

    // 取第 2~3 码点 → "中"
    ref = s.sliceByCp(2, 1);
    EXPECT_EQ(ref.length(), 1);
    EXPECT_EQ(ref.codePointAt(0), 0x4E2D);
}

TEST(Utf8StringTest, Substr) {
    Utf8String s("Hello世界");
    // 取后 2 个码点 → "世界"
    auto sub = s.substr(5, 2);
    EXPECT_EQ(sub.length(), 2);
    EXPECT_EQ(sub.byteLength(), 6);
    EXPECT_STREQ(sub.cStr(), "世界");
}

TEST(Utf8StringTest, Compare) {
    Utf8String a("ABC");
    Utf8String b("ABD");

    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(a), 0);

    // 与 StringRef 比较
    Utf8StringRef ref = b.ref();
    EXPECT_LT(a.compare(ref), 0);
}

TEST(Utf8StringTest, Equals) {
    Utf8String a("Hello");
    Utf8String b("Hello");
    Utf8String c("World");

    EXPECT_TRUE(a.equals(b.ref()));
    EXPECT_FALSE(a.equals(c.ref()));
}

TEST(Utf8StringTest, EqualityOperators) {
    Utf8String a("Hi");
    Utf8String b("Hi");
    Utf8String c("Bye");

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a != b);

    // Utf8String vs Utf8StringRef
    Utf8StringRef ref(reinterpret_cast<const u8*>("Hi"), 2, 2);
    EXPECT_TRUE(a == ref);
    EXPECT_TRUE(ref == a);
}

TEST(Utf8StringTest, InvalidUtf8_Throws) {
    // 非法的首字节 0xFF
    u8 bad[] = {0xFF, 0x41, 0x42};
    EXPECT_THROW(Utf8String(bad, 3), std::runtime_error);

    // 非法续字节
    u8 bad2[] = {0xE4, 0x00, 0xAD};
    EXPECT_THROW(Utf8String(bad2, 3), std::runtime_error);

    // 截断的序列
    u8 bad3[] = {0xF0, 0x9F, 0x98};  // 4 字节序列缺 1 字节
    EXPECT_THROW(Utf8String(bad3, 3), std::runtime_error);
}

TEST(Utf8StringTest, UnicodeMixed) {
    // 混合 ASCII、2 字节、3 字节、4 字节
    const u8 mixed[] = {
        0x48,                               // 'H' (1B)
        0xC3, 0xA9,                         // 'é' (2B) U+00E9
        0xE4, 0xB8, 0x96,                   // '世' (3B) U+4E16
        0xF0, 0x9F, 0x8C, 0x8D             // '🌍' (4B) U+1F30D
    };
    Utf8String s(mixed, 10);
    EXPECT_EQ(s.length(), 4);
    EXPECT_EQ(s.byteLength(), 10);
    EXPECT_EQ(s.codePointAt(0), 0x48);
    EXPECT_EQ(s.codePointAt(1), 0xE9);
    EXPECT_EQ(s.codePointAt(2), 0x4E16);
    EXPECT_EQ(s.codePointAt(3), 0x1F30D);
}

TEST(Utf8StringTest, EmptyStringConstructors) {
    Utf8String s1;
    EXPECT_TRUE(s1.isEmpty());

    Utf8String s2(static_cast<const u8*>(nullptr), 0);
    EXPECT_TRUE(s2.isEmpty());

    Utf8String s3("");
    EXPECT_TRUE(s3.isEmpty());
}

TEST(Utf8StringTest, LargeString) {
    // 构造一个较长的字符串，确保无内存问题
    std::string large;
    for (int i = 0; i < 1000; ++i) {
        large += "中";
    }
    Utf8String s(large.c_str());
    EXPECT_EQ(s.length(), 1000);
    EXPECT_EQ(s.byteLength(), 3000);
    EXPECT_EQ(s.codePointAt(0), 0x4E2D);
    EXPECT_EQ(s.codePointAt(999), 0x4E2D);
}

}  // namespace ca::str
