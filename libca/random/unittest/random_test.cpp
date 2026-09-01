#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include "libca/random/random.hpp"

namespace ca::random::test {
namespace {

TEST(FillBytesTest, FillsRequestedLength)
{
    unsigned char buf[32] = {0};

    fill_bytes(buf, sizeof(buf));

    // 不太可能全零（概率 2^-256）。
    bool any_nonzero = false;
    for (unsigned char c : buf)
        any_nonzero = any_nonzero || (c != 0);
    EXPECT_TRUE(any_nonzero);
}

TEST(FillBytesTest, ZeroLengthIsNoop)
{
    unsigned char buf[4] = {1, 2, 3, 4};

    fill_bytes(buf, 0);

    EXPECT_EQ(buf[0], 1);
    EXPECT_EQ(buf[3], 4);
}

TEST(FillBytesTest, TwoCallsDiffer)
{
    unsigned char a[16];
    unsigned char b[16];

    fill_bytes(a, sizeof(a));
    fill_bytes(b, sizeof(b));

    EXPECT_NE(std::memcmp(a, b, sizeof(a)), 0);
}

TEST(NextTest, StaysInRange)
{
    constexpr u64 n = 100;
    for (int i = 0; i < 1000; ++i) {
        u64 value = next(n);
        EXPECT_LT(value, n);
    }
}

TEST(NextTest, TwoCallsDiffer)
{
    // next(一个大数) 两次大概率不同
    u64 a = next(1ULL << 40);
    u64 b = next(1ULL << 40);
    EXPECT_NE(a, b);
}

TEST(NextTest, NOneAlwaysReturnsZero)
{
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(next(1), 0u);
}

TEST(NextTest, DistributionRoughlyUniform)
{
    // 粗略均匀性检查：10000 次 next(100)，每个桶应远小于总次数的 1/4。
    constexpr u64 n           = 100;
    constexpr int total       = 20000;
    int           counts[100] = {0};

    for (int i = 0; i < total; ++i)
        ++counts[next(n)];

    int max = *std::max_element(counts, counts + n);
    int min = *std::min_element(counts, counts + n);
    // 期望均值 ~200，宽松允许 [50, 400]
    EXPECT_GE(min, 50);
    EXPECT_LE(max, 400);
}

TEST(RangeTest, StaysWithinRange)
{
    constexpr u64 lo = 1000;
    constexpr u64 hi = 1010;
    for (int i = 0; i < 1000; ++i) {
        u64 value = range(lo, hi);
        EXPECT_GE(value, lo);
        EXPECT_LT(value, hi);
    }
}

TEST(RangeTest, ProducesMultipleDistinctValues)
{
    std::set<u64> seen;
    for (int i = 0; i < 200; ++i)
        seen.insert(range(0, 1000));
    EXPECT_GT(seen.size(), 1u);
}

TEST(ProbabilityTest, StaysInUnitRange)
{
    for (int i = 0; i < 1000; ++i) {
        double p = probability();
        EXPECT_GE(p, 0.0);
        EXPECT_LT(p, 1.0);
    }
}

TEST(HexStringTest, CorrectLengthAndCharset)
{
    std::string s = hex_string(16);

    EXPECT_EQ(s.size(), 32u);
    for (char c : s)
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
}

TEST(HexStringTest, ZeroLengthIsEmpty)
{
    EXPECT_EQ(hex_string(0), "");
}

TEST(HexStringTest, TwoCallsDiffer)
{
    std::string a = hex_string(16);
    std::string b = hex_string(16);
    EXPECT_NE(a, b);
}

TEST(AlphanumericTest, CorrectLengthAndCharset)
{
    std::string s = alphanumeric_string(32);

    EXPECT_EQ(s.size(), 32u);
    for (char c : s)
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

TEST(AlphanumericTest, ZeroLengthIsEmpty)
{
    EXPECT_EQ(alphanumeric_string(0), "");
}

}   // namespace
}   // namespace ca::random::test
