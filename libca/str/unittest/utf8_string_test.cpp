#include <gtest/gtest.h>
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_util.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <cstring>

namespace ca::str {

// ============================================================================
// 工具函数测试
// ============================================================================

TEST(Utf8UtilTest, CodePointBytes_Ascii) {
    EXPECT_EQ(utf8_code_point_bytes(0x00), 1);
    EXPECT_EQ(utf8_code_point_bytes(0x41), 1);  // 'A'
    EXPECT_EQ(utf8_code_point_bytes(0x7F), 1);
}

TEST(Utf8UtilTest, CodePointBytes_MultiByte) {
    EXPECT_EQ(utf8_code_point_bytes(0xC0), 2);
    EXPECT_EQ(utf8_code_point_bytes(0xDF), 2);
    EXPECT_EQ(utf8_code_point_bytes(0xE0), 3);
    EXPECT_EQ(utf8_code_point_bytes(0xEF), 3);
    EXPECT_EQ(utf8_code_point_bytes(0xF0), 4);
    EXPECT_EQ(utf8_code_point_bytes(0xF7), 4);
}

TEST(Utf8UtilTest, CodePointBytes_Invalid) {
    EXPECT_EQ(utf8_code_point_bytes(0xF8), 0);  // 11111xxx 非法
    EXPECT_EQ(utf8_code_point_bytes(0xFC), 0);
    EXPECT_EQ(utf8_code_point_bytes(0xFE), 0);
    EXPECT_EQ(utf8_code_point_bytes(0xFF), 0);
}

TEST(Utf8UtilTest, DecodeCodePoint) {
    // ASCII: 'A' = U+0041
    u8 ascii[] = {0x41};
    EXPECT_EQ(utf8_decode_code_point(ascii), 0x41);

    // 2 字节: U+00A9 (©)
    u8 two[] = {0xC2, 0xA9};
    EXPECT_EQ(utf8_decode_code_point(two), 0xA9);

    // 3 字节: U+4E2D (中)
    u8 three[] = {0xE4, 0xB8, 0xAD};
    EXPECT_EQ(utf8_decode_code_point(three), 0x4E2D);

    // 4 字节: U+1F600 (😀)
    u8 four[] = {0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(utf8_decode_code_point(four), 0x1F600);
}

TEST(Utf8UtilTest, EncodeCodePoint_Ascii) {
    u8 buf[4] = {};
    auto len = utf8_encode_code_point(0x41, buf);
    EXPECT_EQ(len, 1);
    EXPECT_EQ(buf[0], 0x41);
}

TEST(Utf8UtilTest, EncodeCodePoint_MultiByte) {
    u8 buf[4] = {};

    // U+00A9 → 0xC2 0xA9
    auto len = utf8_encode_code_point(0xA9, buf);
    EXPECT_EQ(len, 2);
    EXPECT_EQ(buf[0], 0xC2);
    EXPECT_EQ(buf[1], 0xA9);

    // U+4E2D → 0xE4 0xB8 0xAD
    len = utf8_encode_code_point(0x4E2D, buf);
    EXPECT_EQ(len, 3);
    EXPECT_EQ(buf[0], 0xE4);
    EXPECT_EQ(buf[1], 0xB8);
    EXPECT_EQ(buf[2], 0xAD);

    // U+1F600 → 0xF0 0x9F 0x98 0x80
    len = utf8_encode_code_point(0x1F600, buf);
    EXPECT_EQ(len, 4);
    EXPECT_EQ(buf[0], 0xF0);
    EXPECT_EQ(buf[1], 0x9F);
    EXPECT_EQ(buf[2], 0x98);
    EXPECT_EQ(buf[3], 0x80);
}

TEST(Utf8UtilTest, EncodeCodePoint_Invalid) {
    u8 buf[4] = {};

    // 代理项非法
    EXPECT_EQ(utf8_encode_code_point(0xD800, buf), 0);
    EXPECT_EQ(utf8_encode_code_point(0xDFFF, buf), 0);

    // 超出范围
    EXPECT_EQ(utf8_encode_code_point(0x110000, buf), 0);
}

TEST(Utf8UtilTest, CountCodePoints) {
    // "A中😀" = 0x41 + 3 + 4 = 8 bytes, 3 code points
    u8 utf8[] = {0x41, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80};
    usize invalid_pos = 999;
    EXPECT_EQ(utf8_count_code_points(utf8, 8, &invalid_pos), 3);
    EXPECT_EQ(invalid_pos, 999);  // 未修改
}

TEST(Utf8UtilTest, CountCodePoints_Invalid) {
    // 非法续字节
    u8 bad[] = {0xE4, 0x00, 0xAD};
    usize invalid_pos = 999;
    EXPECT_EQ(utf8_count_code_points(bad, 3, &invalid_pos), 0);
    EXPECT_EQ(invalid_pos, 1);  // 第二个字节非法
}

TEST(Utf8UtilTest, IsValid) {
    u8 valid[] = {0x41, 0xE4, 0xB8, 0xAD};
    EXPECT_TRUE(utf8_is_valid(valid, 4));

    u8 invalid[] = {0xE4, 0x00, 0xAD};
    EXPECT_FALSE(utf8_is_valid(invalid, 3));
}

// ============================================================================
// Utf8StringRef 测试
// ============================================================================

TEST(Utf8StringRefTest, DefaultConstructor) {
    Utf8StringRef ref;
    EXPECT_TRUE(ref.is_empty());
    EXPECT_EQ(ref.length(), 0);
    EXPECT_EQ(ref.byte_length(), 0);
    EXPECT_EQ(ref.data(), nullptr);
}

TEST(Utf8StringRefTest, ConstructFromData) {
    u8 data[] = {0x41, 0x42, 0x43};
    Utf8StringRef ref(data, 3, 3);
    EXPECT_EQ(ref.length(), 3);
    EXPECT_EQ(ref.byte_length(), 3);
    EXPECT_FALSE(ref.is_empty());
    EXPECT_EQ(ref.data(), data);
}

TEST(Utf8StringRefTest, ByteAt) {
    u8 data[] = {0x41, 0xE4, 0xB8, 0xAD};
    Utf8StringRef ref(data, 4, 2);
    EXPECT_EQ(ref.byte_at(0), 0x41);
    EXPECT_EQ(ref.byte_at(1), 0xE4);
}

TEST(Utf8StringRefTest, CodePointAt) {
    // "A中" = 0x41 (1B) + 0xE4 0xB8 0xAD (3B)
    u8 data[] = {0x41, 0xE4, 0xB8, 0xAD};
    Utf8StringRef ref(data, 4, 2);
    EXPECT_EQ(ref.code_point_at(0), 0x41);       // 'A'
    EXPECT_EQ(ref.code_point_at(1), 0x4E2D);     // '中'
}

TEST(Utf8StringRefTest, SliceByBytes) {
    // "AB中" = 0x41 0x42 0xE4 0xB8 0xAD
    u8 data[] = {0x41, 0x42, 0xE4, 0xB8, 0xAD};
    Utf8StringRef ref(data, 5, 3);

    // 取字节 [0..2) → "AB"
    auto s1 = ref.slice(0, 2);
    EXPECT_EQ(s1.byte_length(), 2);
    EXPECT_EQ(s1.length(), 2);
    EXPECT_EQ(s1.byte_at(0), 0x41);
    EXPECT_EQ(s1.byte_at(1), 0x42);

    // 取字节 [2..5) → "中"
    auto s2 = ref.slice(2, 5);
    EXPECT_EQ(s2.byte_length(), 3);
    EXPECT_EQ(s2.length(), 1);
    EXPECT_EQ(s2.code_point_at(0), 0x4E2D);
}

TEST(Utf8StringRefTest, SliceByCp) {
    u8 data[] = {0x41, 0x42, 0xE4, 0xB8, 0xAD};  // "AB中"
    Utf8StringRef ref(data, 5, 3);

    // 取第 1~2 码点 → "B中"
    auto s = ref.slice_by_cp(1, 2);
    EXPECT_EQ(s.byte_length(), 4);
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.code_point_at(0), 0x42);
    EXPECT_EQ(s.code_point_at(1), 0x4E2D);
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

TEST(Utf8StringRefTest, OrderingOperators) {
    auto abc = Utf8StringRef::from_cstr("ABC");
    auto abd = Utf8StringRef::from_cstr("ABD");
    auto longer = Utf8StringRef::from_cstr("ABCD");

    EXPECT_TRUE(abc < abd);
    EXPECT_TRUE(abd > abc);
    EXPECT_TRUE(abc <= abd);
    EXPECT_TRUE(abc <= abc);
    EXPECT_TRUE(abd >= abc);
    EXPECT_TRUE(abc < longer);

    EXPECT_TRUE(abc < "ABD");
    EXPECT_TRUE(abc <= "ABC");
    EXPECT_TRUE(abd > "ABC");
    EXPECT_TRUE(abd >= "ABD");
    EXPECT_TRUE("ABC" < abd);
    EXPECT_TRUE("ABD" > abc);
    EXPECT_TRUE("ABC" <= abc);
    EXPECT_TRUE("ABD" >= abd);

    std::vector<Utf8StringRef> items{abd, longer, abc};
    std::sort(items.begin(), items.end());
    EXPECT_TRUE(items[0] == "ABC");
    EXPECT_TRUE(items[1] == "ABCD");
    EXPECT_TRUE(items[2] == "ABD");

    std::map<Utf8StringRef, int> map;
    map[abd] = 2;
    map[abc] = 1;
    EXPECT_EQ(map.begin()->second, 1);
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

TEST(Utf8StringRefTest, CompareWithCStrWithoutOwningString) {
    auto ref = Utf8StringRef::from_cstr("你好");
    EXPECT_EQ(ref.compare("你好"), 0);
    EXPECT_LT(ref.compare("你好啊"), 0);
    EXPECT_GT(ref.compare("你"), 0);
}

TEST(Utf8StringRefTest, EqualityWithCStrWithoutOwningString) {
    auto ref = Utf8StringRef::from_cstr("你好");
    EXPECT_TRUE(ref.equals("你好"));
    EXPECT_TRUE(ref == "你好");
    EXPECT_TRUE("你好" == ref);
    EXPECT_FALSE(ref == "你好啊");
    EXPECT_TRUE(ref != "你好啊");
    EXPECT_TRUE("你好啊" != ref);

    Utf8StringRef empty;
    EXPECT_TRUE(empty == nullptr);
    EXPECT_TRUE(nullptr == empty);
    EXPECT_FALSE(ref == nullptr);
}


// ============================================================================
// Utf8String 测试
// ============================================================================

TEST(Utf8StringTest, DefaultConstructor) {
    Utf8String s;
    EXPECT_TRUE(s.is_empty());
    EXPECT_EQ(s.length(), 0);
    EXPECT_EQ(s.byte_length(), 0);
    EXPECT_NE(s.data(), nullptr);   // 内部有 '\0'
    EXPECT_EQ(s.c_str()[0], '\0');
}

TEST(Utf8StringTest, ConstructFromBytes) {
    u8 hello[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};  // "你好"
    Utf8String s(hello, 6);
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.byte_length(), 6);
    EXPECT_FALSE(s.is_empty());
}

TEST(Utf8StringTest, ConstructFromCStr) {
    Utf8String s("你好");
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.byte_length(), 6);
    EXPECT_STREQ(s.c_str(), "你好");
}

TEST(Utf8StringTest, ConstructFromCStr_Null) {
    Utf8String s(static_cast<const char*>(nullptr));
    EXPECT_TRUE(s.is_empty());
}

TEST(Utf8StringTest, Clone) {
    Utf8String s1("Hello");
    Utf8String s2 = s1.clone();
    EXPECT_EQ(s2.length(), 5);
    EXPECT_EQ(s2.byte_length(), 5);
    EXPECT_STREQ(s2.c_str(), "Hello");
    // 不同缓冲区
    EXPECT_NE(s1.data(), s2.data());
}

TEST(Utf8StringTest, MoveConstructor) {
    Utf8String s1("ABC");
    auto* original_data = s1.data();
    Utf8String s2(std::move(s1));
    EXPECT_EQ(s2.length(), 3);
    EXPECT_EQ(s2.data(), original_data);
    EXPECT_TRUE(s1.is_empty());   // 源被清空
}

TEST(Utf8StringTest, CloneAssignment) {
    Utf8String s1("Hello");
    Utf8String s2 = s1.clone();
    EXPECT_STREQ(s2.c_str(), "Hello");
    EXPECT_NE(s1.data(), s2.data());
}

TEST(Utf8StringTest, MoveAssignment) {
    Utf8String s1("Hello");
    Utf8String s2;
    auto* original_data = s1.data();
    s2 = std::move(s1);
    EXPECT_EQ(s2.data(), original_data);
    EXPECT_TRUE(s1.is_empty());
}

TEST(Utf8StringTest, MoveSelfAssignment) {
    Utf8String s("Test");
    s = std::move(s);
    EXPECT_STREQ(s.c_str(), "Test");
}

TEST(Utf8StringTest, FromCodePoint) {
    auto s = Utf8String::from_code_point(0x4E2D);  // '中'
    EXPECT_EQ(s.length(), 1);
    EXPECT_EQ(s.byte_length(), 3);
    EXPECT_STREQ(s.c_str(), "中");
}

TEST(Utf8StringTest, FromCodePoint_Invalid) {
    EXPECT_THROW(Utf8String::from_code_point(0xD800), std::runtime_error);
    EXPECT_THROW(Utf8String::from_code_point(0x110000), std::runtime_error);
}

TEST(Utf8StringTest, FromData) {
    u8 data[] = {0xE4, 0xB8, 0xAD, 0xE5, 0x9B, 0xBD};  // "中国"
    auto s = Utf8String::from_data(data, 6);
    EXPECT_EQ(s.length(), 2);
    EXPECT_STREQ(s.c_str(), "中国");
}

TEST(Utf8StringTest, FromCStrFactory) {
    auto s = Utf8String::from_cstr("你好");
    EXPECT_EQ(s.length(), 2);
    EXPECT_EQ(s.byte_length(), 6);
    EXPECT_STREQ(s.c_str(), "你好");
}

TEST(Utf8StringTest, FromCStrFactoryNull) {
    auto s = Utf8String::from_cstr(nullptr);
    EXPECT_TRUE(s.is_empty());
    EXPECT_STREQ(s.c_str(), "");
}

TEST(Utf8StringTest, ByteAt) {
    Utf8String s("ABC");
    EXPECT_EQ(s.byte_at(0), 0x41);
    EXPECT_EQ(s.byte_at(1), 0x42);
    EXPECT_EQ(s.byte_at(2), 0x43);
}

TEST(Utf8StringTest, CodePointAt) {
    Utf8String s("A中😀");
    EXPECT_EQ(s.code_point_at(0), 0x41);     // 'A'
    EXPECT_EQ(s.code_point_at(1), 0x4E2D);   // '中'
    EXPECT_EQ(s.code_point_at(2), 0x1F600);  // '😀'
}

TEST(Utf8StringTest, CStr_NullTerminated) {
    Utf8String s("Hello");
    EXPECT_STREQ(s.c_str(), "Hello");
    // 验证 c_str() 的返回值确实指向内部缓冲区
    EXPECT_EQ(reinterpret_cast<const u8*>(s.c_str()), s.data());
}

TEST(Utf8StringTest, Ref) {
    Utf8String s("Test");
    auto ref = s.ref();
    EXPECT_EQ(ref.length(), s.length());
    EXPECT_EQ(ref.byte_length(), s.byte_length());
    EXPECT_EQ(ref.data(), s.data());
}

TEST(Utf8StringTest, Slice) {
    // "AB中" → slice(2,5) → "中"
    Utf8String s("AB中");
    auto ref = s.slice(2, 5);
    EXPECT_EQ(ref.length(), 1);
    EXPECT_EQ(ref.byte_length(), 3);
    EXPECT_EQ(ref.code_point_at(0), 0x4E2D);
}

TEST(Utf8StringTest, SliceByCp) {
    Utf8String s("AB中");
    // 取第 0~1 码点 → "AB"
    auto ref = s.slice_by_cp(0, 2);
    EXPECT_EQ(ref.length(), 2);
    EXPECT_EQ(ref.byte_length(), 2);

    // 取第 2~3 码点 → "中"
    ref = s.slice_by_cp(2, 1);
    EXPECT_EQ(ref.length(), 1);
    EXPECT_EQ(ref.code_point_at(0), 0x4E2D);
}

TEST(Utf8StringTest, Substr) {
    Utf8String s("Hello世界");
    // 取后 2 个码点 → "世界"
    auto sub = s.substr(5, 2);
    EXPECT_EQ(sub.length(), 2);
    EXPECT_EQ(sub.byte_length(), 6);
    EXPECT_STREQ(sub.c_str(), "世界");
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

TEST(Utf8StringTest, OrderingOperators) {
    Utf8String abc("ABC");
    Utf8String abd("ABD");
    auto ref_abd = abd.ref();

    EXPECT_TRUE(abc < abd);
    EXPECT_TRUE(abd > abc);
    EXPECT_TRUE(abc <= abd);
    EXPECT_TRUE(abc <= abc);
    EXPECT_TRUE(abd >= abc);

    EXPECT_TRUE(abc < ref_abd);
    EXPECT_TRUE(ref_abd > abc);
    EXPECT_TRUE(abc < "ABD");
    EXPECT_TRUE(abd > "ABC");
    EXPECT_TRUE("ABC" < abd);
    EXPECT_TRUE("ABD" > abc);
    EXPECT_TRUE("ABC" <= abc);
    EXPECT_TRUE("ABD" >= abd);
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

TEST(Utf8StringTest, CompareWithCStrWithoutTemporaryString) {
    Utf8String s("你好");
    EXPECT_EQ(s.compare("你好"), 0);
    EXPECT_LT(s.compare("你好啊"), 0);
    EXPECT_GT(s.compare("你"), 0);
}

TEST(Utf8StringTest, EqualityWithCStrWithoutTemporaryString) {
    Utf8String s("你好");
    EXPECT_TRUE(s.equals("你好"));
    EXPECT_TRUE(s == "你好");
    EXPECT_TRUE("你好" == s);
    EXPECT_FALSE(s == "你好啊");
    EXPECT_TRUE(s != "你好啊");
    EXPECT_TRUE("你好啊" != s);

    Utf8String empty;
    EXPECT_TRUE(empty == nullptr);
    EXPECT_TRUE(nullptr == empty);
    EXPECT_FALSE(s == nullptr);
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
    EXPECT_EQ(s.byte_length(), 10);
    EXPECT_EQ(s.code_point_at(0), 0x48);
    EXPECT_EQ(s.code_point_at(1), 0xE9);
    EXPECT_EQ(s.code_point_at(2), 0x4E16);
    EXPECT_EQ(s.code_point_at(3), 0x1F30D);
}

TEST(Utf8StringTest, EmptyStringConstructors) {
    Utf8String s1;
    EXPECT_TRUE(s1.is_empty());

    Utf8String s2(static_cast<const u8*>(nullptr), 0);
    EXPECT_TRUE(s2.is_empty());

    Utf8String s3("");
    EXPECT_TRUE(s3.is_empty());
}

TEST(Utf8StringTest, NullDataWithNonZeroLengthConstructsEmpty) {
    Utf8String s(static_cast<const u8*>(nullptr), 3);
    EXPECT_TRUE(s.is_empty());
    EXPECT_STREQ(s.c_str(), "");
}

TEST(Utf8StringTest, LargeString) {
    // 构造一个较长的字符串，确保无内存问题
    std::string large;
    for (int i = 0; i < 1000; ++i) {
        large += "中";
    }
    Utf8String s(large.c_str());
    EXPECT_EQ(s.length(), 1000);
    EXPECT_EQ(s.byte_length(), 3000);
    EXPECT_EQ(s.code_point_at(0), 0x4E2D);
    EXPECT_EQ(s.code_point_at(999), 0x4E2D);
}

// ============================================================================
// Utf8Iterator 测试
// ============================================================================

TEST(Utf8IteratorTest, RangeForLoop) {
    Utf8String s("A中😀");
    u32 expected[] = {0x41, 0x4E2D, 0x1F600};
    usize idx = 0;
    for (u32 cp : s) {
        EXPECT_EQ(cp, expected[idx]);
        ++idx;
    }
    EXPECT_EQ(idx, 3);
}

TEST(Utf8IteratorTest, EmptyString) {
    Utf8String empty;
    usize count = 0;
    for (u32 cp : empty) {
        (void)cp;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(Utf8IteratorTest, RefRangeForLoop) {
    u8 data[] = {0x41, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80};
    Utf8StringRef ref(data, 8, 3);
    u32 expected[] = {0x41, 0x4E2D, 0x1F600};
    usize idx = 0;
    for (u32 cp : ref) {
        EXPECT_EQ(cp, expected[idx]);
        ++idx;
    }
    EXPECT_EQ(idx, 3);
}

TEST(Utf8IteratorTest, IteratorComparison) {
    Utf8String s("ABC");
    auto it = s.begin();
    auto end = s.end();
    EXPECT_NE(it, end);
    EXPECT_EQ(*it, 0x41);
    ++it;
    EXPECT_EQ(*it, 0x42);
    ++it;
    EXPECT_EQ(*it, 0x43);
    ++it;
    EXPECT_EQ(it, end);
}

TEST(Utf8IteratorTest, BytePtr) {
    Utf8String s("AB");
    auto it = s.begin();
    EXPECT_NE(it.byte_ptr(), nullptr);
    EXPECT_EQ(*it.byte_ptr(), 0x41);
}

TEST(Utf8IteratorTest, UnicodeMixedWithBuilder) {
    // for-range 配合 Utf8StringBuilder 使用
    Utf8String s("Hello世界");
    Utf8StringBuilder b;
    for (u32 cp : s) {
        b.append_code_point(cp);
    }
    auto rebuilt = b.build();
    EXPECT_EQ(rebuilt, s);
}

// ============================================================================
// index_of / contains 测试
// ============================================================================

TEST(Utf8StringRefTest, IndexOfSubstring) {
    auto ref = Utf8StringRef::from_cstr("Hello世界");
    EXPECT_EQ(ref.index_of(Utf8StringRef::from_cstr("世界")), 5);
    EXPECT_EQ(ref.index_of(Utf8StringRef::from_cstr("Hello")), 0);
    EXPECT_EQ(ref.index_of(Utf8StringRef::from_cstr("foo")), Utf8StringRef::npos);
}

TEST(Utf8StringRefTest, IndexOfWithStart) {
    auto ref = Utf8StringRef::from_cstr("aaabaa");
    // 第一个 'a' 在 0
    EXPECT_EQ(ref.index_of(Utf8StringRef::from_cstr("a"), 0), 0);
    // 从第 3 个码点开始找 'a' → 位置 4（第 3 个码点是 'b'）
    EXPECT_EQ(ref.index_of(Utf8StringRef::from_cstr("a"), 3), 4);
    // 超出范围
    EXPECT_EQ(ref.index_of(Utf8StringRef::from_cstr("a"), 99), Utf8StringRef::npos);
}

TEST(Utf8StringRefTest, IndexOfCodePoint) {
    auto ref = Utf8StringRef::from_cstr("A中😀");
    EXPECT_EQ(ref.index_of(0x41), 0);       // 'A'
    EXPECT_EQ(ref.index_of(0x4E2D), 1);     // '中'
    EXPECT_EQ(ref.index_of(0x1F600), 2);    // '😀'
    EXPECT_EQ(ref.index_of(0x9999), Utf8StringRef::npos);
}

TEST(Utf8StringRefTest, IndexOfCodePointWithStart) {
    auto ref = Utf8StringRef::from_cstr("ABA");
    EXPECT_EQ(ref.index_of(0x41, 0), 0);    // 第一个 'A'
    EXPECT_EQ(ref.index_of(0x41, 1), 2);    // 从码点 1 开始找 'A'
}

TEST(Utf8StringRefTest, Contains) {
    auto ref = Utf8StringRef::from_cstr("Hello世界");
    EXPECT_TRUE(ref.contains(Utf8StringRef::from_cstr("世界")));
    EXPECT_TRUE(ref.contains(Utf8StringRef::from_cstr("Hello")));
    EXPECT_FALSE(ref.contains(Utf8StringRef::from_cstr("world")));
}

// Utf8String 版本（委托至 Utf8StringRef）
TEST(Utf8StringTest, IndexOfSubstring) {
    Utf8String s("Hello世界");
    EXPECT_EQ(s.index_of(Utf8StringRef::from_cstr("世界")), 5);
    EXPECT_EQ(s.index_of(Utf8StringRef::from_cstr("Hello")), 0);
}

TEST(Utf8StringTest, Contains) {
    Utf8String s("Hello世界");
    EXPECT_TRUE(s.contains(Utf8StringRef::from_cstr("世界")));
    EXPECT_FALSE(s.contains(Utf8StringRef::from_cstr("World")));
}

// ============================================================================
// from_cstr 测试
// ============================================================================

TEST(Utf8StringRefTest, FromCStr) {
    auto ref = Utf8StringRef::from_cstr("ABC");
    EXPECT_EQ(ref.length(), 3);
    EXPECT_EQ(ref.byte_length(), 3);
    EXPECT_EQ(ref.code_point_at(0), 0x41);

    // 中文
    auto ref2 = Utf8StringRef::from_cstr("你好");
    EXPECT_EQ(ref2.length(), 2);
    EXPECT_EQ(ref2.byte_length(), 6);

    // null 安全
    auto ref3 = Utf8StringRef::from_cstr(nullptr);
    EXPECT_TRUE(ref3.is_empty());
}

// ============================================================================
// size() / empty() STL 别名测试
// ============================================================================

TEST(Utf8StringTest, SizeAlias) {
    Utf8String s("Hello");
    EXPECT_EQ(s.size(), s.length());
    EXPECT_TRUE(s.size() > 0);

    Utf8String empty;
    EXPECT_EQ(empty.size(), 0);
}

TEST(Utf8StringTest, EmptyAlias) {
    Utf8String s;
    EXPECT_TRUE(s.empty());

    Utf8String s2("Hi");
    EXPECT_FALSE(s2.empty());
}

// ============================================================================
// build_or_empty 测试
// ============================================================================

TEST(Utf8StringBuilderTest, BuildOrEmpty_Valid) {
    Utf8StringBuilder b;
    b.append("Hello");
    auto s = b.build_or_empty();
    EXPECT_EQ(s.length(), 5);
    EXPECT_STREQ(s.c_str(), "Hello");
}

TEST(Utf8StringBuilderTest, BuildOrEmpty_Invalid) {
    // 通过 append raw bytes 放入非法序列
    u8 bad[] = {0xFF, 0xFE};
    Utf8StringBuilder b;
    b.append(bad, 2);
    auto s = b.build_or_empty();
    EXPECT_TRUE(s.is_empty());
}

// ============================================================================
// operator<< 测试
// ============================================================================

TEST(Utf8StringTest, StreamOutput) {
    Utf8String s("Hello世界");
    std::ostringstream oss;
    oss << s;
    EXPECT_EQ(oss.str(), "Hello世界");
}

TEST(Utf8StringRefTest, StreamOutput) {
    auto ref = Utf8StringRef::from_cstr("Test");
    std::ostringstream oss;
    oss << ref;
    EXPECT_EQ(oss.str(), "Test");
}

// ============================================================================
// to_std_string 测试
// ============================================================================

TEST(Utf8StringRefTest, ToStdString) {
    auto ref = Utf8StringRef::from_cstr("Hello");
    auto std_str = ref.to_std_string();
    EXPECT_EQ(std_str, "Hello");
    EXPECT_EQ(std_str.size(), 5);
}

TEST(Utf8StringRefTest, ToStdString_Unicode) {
    auto ref = Utf8StringRef::from_cstr("你好😀");
    auto std_str = ref.to_std_string();
    EXPECT_EQ(std_str, "你好😀");
    EXPECT_EQ(std_str.size(), 10);
}

TEST(Utf8StringRefTest, ToStdString_Empty) {
    Utf8StringRef ref;
    auto std_str = ref.to_std_string();
    EXPECT_TRUE(std_str.empty());
}

TEST(Utf8StringTest, ToStdString) {
    Utf8String s("Hello");
    auto std_str = s.to_std_string();
    EXPECT_EQ(std_str, "Hello");
    EXPECT_EQ(std_str.size(), 5);
}

TEST(Utf8StringTest, ToStdString_Unicode) {
    Utf8String s("你好😀");
    auto std_str = s.to_std_string();
    EXPECT_EQ(std_str, "你好😀");
    EXPECT_EQ(std_str.size(), 10);
}

TEST(Utf8StringTest, ToStdString_Empty) {
    Utf8String s;
    auto std_str = s.to_std_string();
    EXPECT_TRUE(std_str.empty());
}

TEST(Utf8StringTest, ToStdString_FromCStrRoundtrip) {
    Utf8String s("Hello世界");
    auto std_str = s.to_std_string();
    EXPECT_EQ(std_str, "Hello世界");
    // 验证可重新构造 Utf8String
    Utf8String roundtrip(std_str.c_str());
    EXPECT_EQ(roundtrip, s);
}

TEST(ZUtf8StringRef, SimpleTest)
{
    ZUtf8StringRef CONSTANT_RAW_STR = ZUtf8StringRef::from_static("CONSTANT_RAW_STR");
    EXPECT_EQ(CONSTANT_RAW_STR.c_str(), "CONSTANT_RAW_STR");

    // ...
}

TEST(ZUtf8StringRef, FromStdStringCountsUtf8CodePoints)
{
    std::string value = "你好😀";
    auto ref = ZUtf8StringRef::from_std_string(value);
    EXPECT_EQ(ref.byte_length(), value.size());
    EXPECT_EQ(ref.length(), 3);
    EXPECT_EQ(ref.c_str(), value.c_str());
}

TEST(ZUtf8StringRef, ConvertsToUtf8StringRef)
{
    auto z = ZUtf8StringRef::from_static("你好");
    Utf8StringRef ref = z;
    EXPECT_EQ(ref.byte_length(), 6);
    EXPECT_EQ(ref.length(), 2);
    EXPECT_TRUE(ref == "你好");
}

TEST(ZUtf8StringRef, EqualityWithCStrWithoutOwningString)
{
    auto z = ZUtf8StringRef::from_static("你好");
    EXPECT_EQ(z.compare("你好"), 0);
    EXPECT_TRUE(z.equals("你好"));
    EXPECT_TRUE(z == "你好");
    EXPECT_TRUE("你好" == z);
    EXPECT_TRUE(z != "你好啊");
    EXPECT_TRUE("你好啊" != z);

    Utf8StringRef ref = Utf8StringRef::from_cstr("你好");
    EXPECT_TRUE(z == ref);
}

}  // namespace ca::str
