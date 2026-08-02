#include <gtest/gtest.h>

#include <set>
#include <string>

#include "libca/uuid/uuid.hpp"

namespace ca::uuid::test {
namespace {

bool is_lower_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

void expect_well_formed_v4(const std::string& id)
{
    ASSERT_EQ(id.size(), 36u);
    EXPECT_EQ(id[8], '-');
    EXPECT_EQ(id[13], '-');
    EXPECT_EQ(id[18], '-');
    EXPECT_EQ(id[23], '-');
    for (std::size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            continue;
        EXPECT_TRUE(is_lower_hex(id[i])) << "non-hex char '" << id[i] << "' at " << i;
    }
    EXPECT_EQ(id[14], '4');
    EXPECT_TRUE(id[19] == '8' || id[19] == '9' || id[19] == 'a' || id[19] == 'b');
}

TEST(UuidV4Test, FormatIsLowercaseHexWithDashes)
{
    expect_well_formed_v4(v4());
}

TEST(UuidV4Test, StampsVersionAndVariant)
{
    expect_well_formed_v4(v4());
}

TEST(UuidV4Test, TwoCallsDiffer)
{
    std::string a = v4();
    std::string b = v4();

    EXPECT_NE(a, b);
}

TEST(UuidV4Test, ManyCallsAreUnique)
{
    constexpr int count = 1000;
    std::set<std::string> seen;

    for (int i = 0; i < count; ++i) {
        auto id = v4();
        ASSERT_TRUE(seen.insert(id).second) << "duplicate uuid at i=" << i;
    }
    EXPECT_EQ(seen.size(), static_cast<size_t>(count));
}

TEST(UuidNilTest, NilIsAllZeros)
{
    std::string n = nil();

    EXPECT_EQ(n, "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(n.size(), 36u);
}

TEST(UuidIsValidTest, AcceptsValidV4)
{
    EXPECT_TRUE(is_valid("550e8400-e29b-41d4-a716-446655440000"));
    EXPECT_TRUE(is_valid(v4()));
}

TEST(UuidIsValidTest, NilIsValidOnlyInLenientMode)
{
    // nil UUID 的 version/variant 位均为 0，不是 v4：严格校验拒绝，宽松校验通过。
    EXPECT_FALSE(is_valid("00000000-0000-0000-0000-000000000000"));
    EXPECT_TRUE(is_valid("00000000-0000-0000-0000-000000000000", false));
}

TEST(UuidIsValidTest, AcceptsUppercase)
{
    EXPECT_TRUE(is_valid("550E8400-E29B-41D4-A716-446655440000"));
}

TEST(UuidIsValidTest, RejectsBadLength)
{
    EXPECT_FALSE(is_valid(""));
    EXPECT_FALSE(is_valid("550e8400-e29b-41d4-a716-44665544000"));
    EXPECT_FALSE(is_valid("550e8400-e29b-41d4-a716-4466554400000"));
}

TEST(UuidIsValidTest, RejectsBadHyphenPositions)
{
    EXPECT_FALSE(is_valid("550e8400+e29b-41d4-a716-446655440000"));
    EXPECT_FALSE(is_valid("550e8400-e29b-41d4-a716 446655440000"));
}

TEST(UuidIsValidTest, RejectsNonHex)
{
    EXPECT_FALSE(is_valid("550e8400-e29b-41d4-a716-44665544zzzz"));
    EXPECT_FALSE(is_valid("gg0e8400-e29b-41d4-a716-446655440000"));
}

TEST(UuidIsValidTest, RejectsWrongVersionWhenStrict)
{
    // version 位非 4
    EXPECT_FALSE(is_valid("550e8400-e29b-31d4-a716-446655440000"));
    // variant 位非 8/9/a/b
    EXPECT_FALSE(is_valid("550e8400-e29b-41d4-1716-446655440000"));
}

TEST(UuidIsValidTest, LenientModeIgnoresVersionVariant)
{
    EXPECT_TRUE(is_valid("550e8400-e29b-31d4-1716-446655440000", false));
}

}  // namespace
}  // namespace ca::uuid::test
