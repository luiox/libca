#include "libca/str/utf16_string.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <unordered_map>

namespace ca::str {

TEST(Char16Test, ValueTypeComparisonAndHash)
{
    constexpr Char16 a(0x0041);
    constexpr Char16 b(0x0042);
    constexpr Char16 lead(0xD83D);
    constexpr Char16 trail(0xDE00);

    static_assert(a.unit() == 0x0041, "Char16 stores the raw u16 unit");
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(lead.is_lead_surrogate());
    EXPECT_TRUE(trail.is_trail_surrogate());
    EXPECT_TRUE(lead.is_surrogate());
    EXPECT_TRUE(Char16(0x4E2D).is_bmp());

    std::unordered_map<Char16, int> map;
    map[a] = 1;
    EXPECT_EQ(map[Char16(0x0041)], 1);
}

TEST(Utf16StringTest, LengthCharAtAndCodePointAtUseJavaSemantics)
{
    const ca::u16 raw[] = {0x0041, 0xD83D, 0xDE00, 0x0042};
    Utf16String   s(raw, 4);

    EXPECT_EQ(s.length(), 4u);
    EXPECT_EQ(s.char_at(0), Char16(0x0041));
    EXPECT_EQ(s.char_at(1), Char16(0xD83D));
    EXPECT_EQ(s.code_point_at(0), 0x0041u);
    EXPECT_EQ(s.code_point_at(1), 0x1F600u);
    EXPECT_EQ(s.code_point_at(2), 0xDE00u);
    EXPECT_EQ(s.code_point_at(99), 0u);
}

TEST(Utf16StringTest, SubstringSlicesCodeUnits)
{
    const ca::u16 raw[] = {0x0041, 0xD83D, 0xDE00, 0x0042};
    Utf16String   s(raw, 4);

    auto emoji = s.substring(1, 3);

    EXPECT_EQ(emoji.length(), 2u);
    EXPECT_EQ(emoji.code_point_at(0), 0x1F600u);
    EXPECT_TRUE(emoji == s.slice(1, 3));
}

TEST(Utf16StringTest, IndexOfCharAndString)
{
    const ca::u16  raw[] = {0x0061, 0x0062, 0x0061, 0xD83D, 0xDE00};
    Utf16String    s(raw, 5);
    const ca::u16  needle_raw[] = {0x0061, 0xD83D, 0xDE00};
    Utf16StringRef needle       = Utf16StringRef::from_data(needle_raw, 3);

    EXPECT_EQ(s.index_of(Char16(0x0061)), 0u);
    EXPECT_EQ(s.index_of(Char16(0x0061), 1), 2u);
    EXPECT_EQ(s.index_of(Char16(0x9999)), Utf16StringRef::npos);
    EXPECT_EQ(s.index_of(needle), 2u);
    EXPECT_TRUE(s.contains(needle));
}

TEST(Utf16StringTest, StartsEndsAndLastIndexOf)
{
    const ca::u16  raw[] = {0x0061, 0x0062, 0x0061, 0x0062, 0x0061, 0xD83D, 0xDE00};
    Utf16String    s(raw, 7);
    const ca::u16  ab_raw[]    = {0x0061, 0x0062};
    const ca::u16  emoji_raw[] = {0xD83D, 0xDE00};
    Utf16StringRef ab          = Utf16StringRef::from_data(ab_raw, 2);
    Utf16StringRef emoji       = Utf16StringRef::from_data(emoji_raw, 2);
    Utf16StringRef empty;

    EXPECT_TRUE(s.starts_with(ab));
    EXPECT_TRUE(s.starts_with(ab, 2));
    EXPECT_FALSE(s.starts_with(ab, 1));
    EXPECT_TRUE(s.ends_with(emoji));
    EXPECT_FALSE(s.ends_with(ab));

    EXPECT_EQ(s.last_index_of(Char16(0x0061)), 4u);
    EXPECT_EQ(s.last_index_of(Char16(0x0061), 3), 2u);
    EXPECT_EQ(s.last_index_of(Char16(0x9999)), Utf16StringRef::npos);
    EXPECT_EQ(s.last_index_of(ab), 2u);
    EXPECT_EQ(s.last_index_of(ab, 1), 0u);
    EXPECT_EQ(s.last_index_of(emoji), 5u);
    EXPECT_EQ(s.last_index_of(empty), s.length());
    EXPECT_EQ(s.last_index_of(empty, 3), 3u);
}

TEST(Utf16StringTest, CloneEqualsConcatAndCompare)
{
    const ca::u16 left_raw[]  = {0x0041, 0x0042};
    const ca::u16 right_raw[] = {0x0043};
    Utf16String   left(left_raw, 2);
    Utf16String   right(right_raw, 1);

    auto clone = left.clone();
    EXPECT_TRUE(clone == left);
    EXPECT_NE(clone.data(), left.data());

    auto combined = left.concat(right.ref());
    EXPECT_EQ(combined.length(), 3u);
    EXPECT_EQ(combined.char_at(2), Char16(0x0043));
    EXPECT_LT(left.compare(combined.ref()), 0);
    EXPECT_LT(left.compare_to(combined.ref()), 0);
}

TEST(Utf16StringTest, JavaStyleAliases)
{
    Utf16String empty;
    Utf16String value = Utf16String::from_utf8_string(Utf8StringRef::from_cstr("abc"));

    EXPECT_TRUE(empty.is_empty());
    EXPECT_FALSE(value.is_empty());
    EXPECT_EQ(
        value.compare_to(Utf16String::from_utf8_string(Utf8StringRef::from_cstr("abd")).ref()), -1);

    auto copy = value.to_string();
    EXPECT_TRUE(copy == value);
    EXPECT_NE(copy.data(), value.data());
}

TEST(Utf16StringTest, HashCodeMatchesJavaString)
{
    Utf16String   empty;
    auto          abc   = Utf16String::from_utf8_string(Utf8StringRef::from_cstr("abc"));
    auto          aa    = Utf16String::from_utf8_string(Utf8StringRef::from_cstr("Aa"));
    auto          bb    = Utf16String::from_utf8_string(Utf8StringRef::from_cstr("BB"));
    const ca::u16 raw[] = {0x0041, 0xD83D, 0xDE00, 0x0042};
    Utf16String   with_surrogate(raw, 4);

    EXPECT_EQ(empty.hash_code(), 0);
    EXPECT_EQ(abc.hash_code(), 96354);
    EXPECT_EQ(aa.hash_code(), 2112);
    EXPECT_EQ(bb.hash_code(), 2112);
    EXPECT_EQ(with_surrogate.hash_code(), 56896350);
    EXPECT_EQ(abc.ref().hash_code(), abc.hash_code());
}

TEST(Utf16StringTest, ConvertsToAndFromUtf8)
{
    Utf8String utf8("A😀中");
    auto       utf16 = Utf16String::from_utf8_string(utf8.ref());

    EXPECT_EQ(utf16.length(), 4u);
    EXPECT_EQ(utf16.code_point_at(1), 0x1F600u);

    auto roundtrip = utf16.to_utf8_string();
    EXPECT_EQ(roundtrip, utf8);
}

TEST(Utf16StringTest, ConvertsToStdU16String)
{
    const ca::u16 raw[] = {0x0041, 0xD83D, 0xDE00};
    Utf16String   s(raw, 3);

    auto standard = s.to_std_u16_string();

    ASSERT_EQ(standard.size(), 3u);
    EXPECT_EQ(standard[0], u'A');
    EXPECT_EQ(static_cast<ca::u16>(standard[1]), 0xD83D);
    EXPECT_EQ(static_cast<ca::u16>(standard[2]), 0xDE00);
}

TEST(Utf16StringTest, InvalidUtf16ToUtf8Throws)
{
    const ca::u16 raw[] = {0xD83D, 0x0041};
    Utf16String   s(raw, 2);

    EXPECT_THROW(s.to_utf8_string(), std::runtime_error);
}

TEST(Utf16StringTest, FromCodePoint)
{
    auto bmp = Utf16String::from_code_point(0x4E2D);
    EXPECT_EQ(bmp.length(), 1u);
    EXPECT_EQ(bmp.char_at(0), Char16(0x4E2D));

    auto emoji = Utf16String::from_code_point(0x1F600);
    EXPECT_EQ(emoji.length(), 2u);
    EXPECT_EQ(emoji.code_point_at(0), 0x1F600u);

    EXPECT_THROW(Utf16String::from_code_point(0xD800), std::runtime_error);
}

TEST(Utf16StringTest, ValueOfBuildsUtf16Text)
{
    EXPECT_EQ(Utf16String::value_of(true).to_utf8_string(), Utf8String("true"));
    EXPECT_EQ(Utf16String::value_of(false).to_utf8_string(), Utf8String("false"));
    EXPECT_EQ(Utf16String::value_of(Char16(0x0041)).to_utf8_string(), Utf8String("A"));
    EXPECT_EQ(Utf16String::value_of(static_cast<ca::i32>(-42)).to_utf8_string(), Utf8String("-42"));
    EXPECT_EQ(Utf16String::value_of(static_cast<ca::i64>(1234567890123LL)).to_utf8_string(),
              Utf8String("1234567890123"));
    EXPECT_EQ(Utf16String::value_of("Hi中").to_utf8_string(), Utf8String("Hi中"));

    auto source = Utf16String::from_utf8_string(Utf8StringRef::from_cstr("copy"));
    auto copied = Utf16String::value_of(source.ref());
    EXPECT_TRUE(copied == source);
    EXPECT_NE(copied.data(), source.data());
}

TEST(Utf16StringBuilderTest, AppendInsertAndBuild)
{
    Utf16StringBuilder builder;
    builder.append(Char16(0x0041));
    EXPECT_TRUE(builder.append_code_point(0x1F600));
    builder.insert(1, Utf16String::from_code_point(0x0042).ref());

    auto s = builder.build();

    ASSERT_EQ(s.length(), 4u);
    EXPECT_EQ(s.char_at(0), Char16(0x0041));
    EXPECT_EQ(s.char_at(1), Char16(0x0042));
    EXPECT_EQ(s.code_point_at(2), 0x1F600u);
    EXPECT_FALSE(builder.is_empty());
    EXPECT_TRUE(builder.to_string() == s);
}

TEST(Utf16StringBuilderTest, AppendJavaStyleValues)
{
    Utf16StringBuilder builder;
    Utf8StringRef      ref = Utf8StringRef::from_cstr("中");

    builder.append("value=")
        .append(static_cast<ca::i32>(42))
        .append(Char16(0x002C))
        .append(true)
        .append(Char16(0x002C))
        .append(static_cast<ca::i64>(1234567890123LL))
        .append(Char16(0x002C))
        .append(ref);

    auto value = builder.to_string();

    EXPECT_EQ(value.to_utf8_string(), Utf8String("value=42,true,1234567890123,中"));
}

TEST(Utf16StringBuilderTest, InsertJavaStyleValues)
{
    Utf16StringBuilder builder;
    builder.append("AB");
    Utf8StringRef ref = Utf8StringRef::from_cstr("中");

    builder.insert(1, Char16(0x002D))
        .insert(0, static_cast<ca::i32>(42))
        .insert(99, ref)
        .insert(2, false);

    auto value = builder.to_string();

    EXPECT_EQ(value.to_utf8_string(), Utf8String("42falseA-B中"));
}

TEST(Utf16StringBuilderTest, CharAtCodePointAtAndSetCharAtUseCodeUnitIndexes)
{
    Utf16StringBuilder builder;
    builder.append(Char16(0x0041));
    ASSERT_TRUE(builder.append_code_point(0x1F600));
    builder.append(Char16(0x0042));

    EXPECT_EQ(builder.char_at(0), Char16(0x0041));
    EXPECT_EQ(builder.char_at(1), Char16(0xD83D));
    EXPECT_EQ(builder.code_point_at(1), 0x1F600u);
    EXPECT_EQ(builder.code_point_at(2), 0xDE00u);
    EXPECT_EQ(builder.char_at(99), Char16());
    EXPECT_EQ(builder.code_point_at(99), 0u);

    builder.set_char_at(0, Char16(0x005A)).set_char_at(99, Char16(0x0058));

    auto value = builder.to_string();
    ASSERT_EQ(value.length(), 4u);
    EXPECT_EQ(value.char_at(0), Char16(0x005A));
    EXPECT_EQ(value.code_point_at(1), 0x1F600u);
    EXPECT_EQ(value.char_at(3), Char16(0x0042));
}

TEST(Utf16StringBuilderTest, DeleteRangeAndCharAtUseCodeUnitIndexes)
{
    Utf16StringBuilder builder;
    builder.append("A");
    ASSERT_TRUE(builder.append_code_point(0x1F600));
    builder.append("BCD");

    builder.delete_range(3, 5).delete_char_at(1);

    auto value = builder.to_string();

    ASSERT_EQ(value.length(), 3u);
    EXPECT_EQ(value.char_at(0), Char16(0x0041));
    EXPECT_EQ(value.char_at(1), Char16(0xDE00));
    EXPECT_EQ(value.char_at(2), Char16(0x0044));

    builder.delete_range(2, 99).delete_range(99, 100).delete_char_at(99);

    auto clamped = builder.to_string();
    ASSERT_EQ(clamped.length(), 2u);
    EXPECT_EQ(clamped.char_at(0), Char16(0x0041));
    EXPECT_EQ(clamped.char_at(1), Char16(0xDE00));
}

TEST(Utf16StringBuilderTest, TruncateAndResizeAdjustCodeUnitLength)
{
    Utf16StringBuilder builder;
    builder.append("ABCD");

    builder.truncate(2);
    EXPECT_EQ(builder.length(), 2u);
    EXPECT_EQ(builder.char_at(0), Char16(0x0041));
    EXPECT_EQ(builder.char_at(1), Char16(0x0042));

    builder.truncate(99);
    EXPECT_EQ(builder.length(), 2u);

    builder.resize(5, Char16(0x0058));
    auto grown = builder.to_string();
    ASSERT_EQ(grown.length(), 5u);
    EXPECT_EQ(grown.char_at(2), Char16(0x0058));
    EXPECT_EQ(grown.char_at(4), Char16(0x0058));

    builder.resize(6);
    EXPECT_EQ(builder.length(), 6u);
    EXPECT_EQ(builder.char_at(5), Char16());
}

TEST(Utf16StringBuilderTest, ReverseKeepsSurrogatePairsTogether)
{
    Utf16StringBuilder builder;
    builder.append(Char16(0x0041));
    ASSERT_TRUE(builder.append_code_point(0x1F600));
    builder.append(Char16(0x0042));

    auto reversed = builder.reverse().build();

    ASSERT_EQ(reversed.length(), 4u);
    EXPECT_EQ(reversed.char_at(0), Char16(0x0042));
    EXPECT_EQ(reversed.code_point_at(1), 0x1F600u);
    EXPECT_EQ(reversed.char_at(3), Char16(0x0041));
}

TEST(Utf16StringTest, StreamOutputUsesUtf8Text)
{
    auto               s = Utf16String::from_utf8_string(Utf8StringRef::from_cstr("Hi中"));
    std::ostringstream os;

    os << s;

    EXPECT_EQ(os.str(), "Hi中");
}

}   // namespace ca::str
