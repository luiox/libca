#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

#include "libca/random/split_mix64.hpp"
#include "libca/random/xorshift32.hpp"

// 确定性伪随机原语的位精确对拍测试。
//
// golden 向量来源：用 Python 逐位复刻 morpher 侧正典 C++ 产出（非本实现自证）：
//   - SplitMix64 正典 = mj2x/backend/vmc4/vmc4_instance.cpp 的内联 SplitMix64；
//   - Xorshift32 正典 = mj2x/backend/regc/regc_cache_pool.cpp 的
//     regcStringXorshift32（与 mj2x/core/native_lib_crypto.hpp::xorshift32Step
//     同式，vmc4 链路引用的是后者，两者已逐位核对一致）。
// 背景 luiox/morpher#1027 A3/A4：morpher 侧 splitmix64/xorshift32 副本收敛到
// libca 单份后，要求输出逐位不变——本文件任何向量变更都意味着破坏下游对拍。

namespace ca::random::test {
namespace {

// ---- SplitMix64 golden 向量（每组 8 步）----

constexpr u64 kSplitMix64Seed0[] = {
    0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL, 0x06C45D188009454FULL, 0xF88BB8A8724C81ECULL,
    0x1B39896A51A8749BULL, 0x53CB9F0C747EA2EAULL, 0x2C829ABE1F4532E1ULL, 0xC584133AC916AB3CULL,
};

constexpr u64 kSplitMix64Seed1[] = {
    0x910A2DEC89025CC1ULL, 0xBEEB8DA1658EEC67ULL, 0xF893A2EEFB32555EULL, 0x71C18690EE42C90BULL,
    0x71BB54D8D101B5B9ULL, 0xC34D0BFF90150280ULL, 0xE099EC6CD7363CA5ULL, 0x85E7BB0F12278575ULL,
};

constexpr u64 kSplitMix64SeedDeadBeef[] = {
    0x4ADFB90F68C9EB9BULL, 0xDE586A3141A10922ULL, 0x021FBC2F8E1CFC1DULL, 0x7466CE737BE16790ULL,
    0x3BFA8764F685BD1CULL, 0xAB203E503CB55B3FULL, 0x5A2FDC2BF68CEDB3ULL, 0xB30A4CCF430B1B5AULL,
};

// ---- Xorshift32 golden 向量（每组 8 步）----

constexpr u32 kXorshift32Seed1[] = {
    0x00042021U, 0x04080601U, 0x9DCCA8C5U, 0x1255994FU,
    0x8EF917D1U, 0x2C6F5BD0U, 0x25B2331AU, 0x19F91CB2U,
};

constexpr u32 kXorshift32SeedDeadBeef[] = {
    0x477D20B7U, 0x8E1D9142U, 0xBA8C2458U, 0xFEE0503BU,
    0x680E0348U, 0xA48DB81BU, 0x6254EA5CU, 0x1CFDAFB3U,
};

// SplitMix64 位精确对拍：golden 向量逐位等于正典 vmc4 版。

TEST(SplitMix64Test, GoldenSeed0)
{
    SplitMix64 rng(0);
    for (usize i = 0; i < 8; ++i)
        EXPECT_EQ(kSplitMix64Seed0[i], rng.next()) << "step=" << i;
}

TEST(SplitMix64Test, GoldenSeed1)
{
    SplitMix64 rng(1);
    for (usize i = 0; i < 8; ++i)
        EXPECT_EQ(kSplitMix64Seed1[i], rng.next()) << "step=" << i;
}

TEST(SplitMix64Test, GoldenSeedDeadBeef)
{
    SplitMix64 rng(0xDEADBEEFULL);
    for (usize i = 0; i < 8; ++i)
        EXPECT_EQ(kSplitMix64SeedDeadBeef[i], rng.next()) << "step=" << i;
}

TEST(SplitMix64Test, SameSeedReproducesSequence)
{
    SplitMix64 a(0x0123456789ABCDEFULL);
    SplitMix64 b(0x0123456789ABCDEFULL);
    for (int i = 0; i < 64; ++i)
        EXPECT_EQ(a.next(), b.next()) << "step=" << i;
}

TEST(SplitMix64Test, DifferentSeedsDiverge)
{
    SplitMix64 a(0);
    SplitMix64 b(1);
    bool differ = false;
    for (int i = 0; i < 8; ++i)
        differ = differ || (a.next() != b.next());
    EXPECT_TRUE(differ);
}

// next_bounded = next() % bound，值域 [0, bound)。

TEST(SplitMix64Test, NextBoundedStaysInRange)
{
    const u64 bounds[] = {1, 2, 3, 7, 100, 1000, 1ULL << 32, (1ULL << 63) + 1};
    for (u64 bound : bounds) {
        SCOPED_TRACE(testing::Message() << "bound=" << bound);
        SplitMix64 rng(0xDEADBEEFULL);
        for (int i = 0; i < 100; ++i) {
            u64 value = rng.next_bounded(bound);
            EXPECT_LT(value, bound);
        }
    }
}

TEST(SplitMix64Test, NextBoundedBoundOneAlwaysZero)
{
    SplitMix64 rng(1);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(0u, rng.next_bounded(1));
}

TEST(SplitMix64Test, NextBoundedMatchesModulo)
{
    // next_bounded 必须严格等于同序列 next() 的取模结果（锁定正典公式）。
    SplitMix64 a(42);
    SplitMix64 b(42);
    for (int i = 0; i < 64; ++i)
        EXPECT_EQ(a.next() % 1000, b.next_bounded(1000)) << "step=" << i;
}

TEST(SplitMix64Test, NextBoundedZeroThrows)
{
    SplitMix64 rng(0);
    EXPECT_THROW(rng.next_bounded(0), std::invalid_argument);
}

TEST(SplitMix64Test, CopyIsIndependentAndConsistent)
{
    SplitMix64 original(0xAAAAAAAAAAAAAAAAULL);
    original.next();  // 推进一步后再拷贝
    SplitMix64 copy = original;

    // 拷贝与原对象同状态同序列，各自推进互不串扰。
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(original.next(), copy.next()) << "step=" << i;

    // 拷贝快照独立：原对象推进不改写拷贝的后续序列。
    SplitMix64 snapshot = original;
    u64 from_original = original.next();
    EXPECT_EQ(from_original, snapshot.next());
}

// Xorshift32 位精确对拍：golden 向量逐位等于正典 regc 版
// （regc_cache_pool.cpp regcStringXorshift32 == native_lib_crypto.hpp
// xorshift32Step，两处正典同式）。

TEST(Xorshift32Test, GoldenSeed1)
{
    Xorshift32 rng(1);
    for (usize i = 0; i < 8; ++i)
        EXPECT_EQ(kXorshift32Seed1[i], rng.next()) << "step=" << i;
}

TEST(Xorshift32Test, GoldenSeedDeadBeef)
{
    Xorshift32 rng(0xDEADBEEFU);
    for (usize i = 0; i < 8; ++i)
        EXPECT_EQ(kXorshift32SeedDeadBeef[i], rng.next()) << "step=" << i;
}

TEST(Xorshift32Test, ZeroSeedIsDegenerateAllZero)
{
    // seed == 0 时状态恒为不动点：序列恒 0。这是定义行为（正典亦然，
    // morpher 侧由调用方回退固定种子），此处锁定该语义防无意"修复"。
    Xorshift32 rng(0);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(0u, rng.next()) << "step=" << i;
}

TEST(Xorshift32Test, SameSeedReproducesSequence)
{
    Xorshift32 a(0xC0FFEEU);
    Xorshift32 b(0xC0FFEEU);
    for (int i = 0; i < 64; ++i)
        EXPECT_EQ(a.next(), b.next()) << "step=" << i;
}

TEST(Xorshift32Test, DifferentSeedsDiverge)
{
    Xorshift32 a(1);
    Xorshift32 b(2);
    bool differ = false;
    for (int i = 0; i < 8; ++i)
        differ = differ || (a.next() != b.next());
    EXPECT_TRUE(differ);
}

TEST(Xorshift32Test, NextBoundedStaysInRange)
{
    const u32 bounds[] = {1u, 2u, 3u, 7u, 100u, 1000u, 1U << 31};
    for (u32 bound : bounds) {
        SCOPED_TRACE(testing::Message() << "bound=" << bound);
        Xorshift32 rng(0xDEADBEEFU);
        for (int i = 0; i < 100; ++i) {
            u32 value = rng.next_bounded(bound);
            EXPECT_LT(value, bound);
        }
    }
}

TEST(Xorshift32Test, NextBoundedMatchesModulo)
{
    // next_bounded 必须严格等于同序列 next() 的取模结果（锁定正典公式）。
    Xorshift32 a(12345);
    Xorshift32 b(12345);
    for (int i = 0; i < 64; ++i)
        EXPECT_EQ(a.next() % 65536, b.next_bounded(65536)) << "step=" << i;
}

TEST(Xorshift32Test, NextBoundedZeroThrows)
{
    Xorshift32 rng(1);
    EXPECT_THROW(rng.next_bounded(0), std::invalid_argument);
}

TEST(Xorshift32Test, CopyIsIndependentAndConsistent)
{
    Xorshift32 original(0x13579BDFU);
    original.next();  // 推进一步后再拷贝
    Xorshift32 copy = original;

    // 拷贝与原对象同状态同序列，各自推进互不串扰。
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(original.next(), copy.next()) << "step=" << i;

    // 拷贝快照独立：原对象推进不改写拷贝的后续序列。
    Xorshift32 snapshot = original;
    u32 from_original = original.next();
    EXPECT_EQ(from_original, snapshot.next());
}

}  // namespace
}  // namespace ca::random::test
