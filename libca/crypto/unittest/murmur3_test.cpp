#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "libca/crypto/murmur3.hpp"

using namespace ca::crypto;

namespace {

// ============================================================
// 独立参考实现：与 murmur3.cpp 结构不同的等价写法（逐字节流式攒块，
// 满足 rotates 常量内联展开），用于与 murmur3_32 做语料级互验。
// 已发表第三方锚点向量见 publishedVectors 用例。
// ============================================================

ca::u32 murmur3_32_ref_stream(const ca::u8* data, ca::usize len, ca::u32 seed)
{
    ca::u32 h = seed;
    ca::u32 k = 0;
    ca::u32 filled = 0;

    // 攒满一个 4 字节块就走一轮主循环混淆（rotl15 / rotl13 内联展开）
    auto mix_block = [&h](ca::u32 block) {
        block *= 0xcc9e2d51u;
        block = (block << 15) | (block >> 17);
        block *= 0x1b873593u;
        h ^= block;
        h = (h << 13) | (h >> 19);
        h = h * 5u + 0xe6546b64u;
    };

    for (ca::usize i = 0; i < len; ++i) {
        k |= static_cast<ca::u32>(data[i]) << (8 * filled);
        if (++filled == 4) {
            mix_block(k);
            k = 0;
            filled = 0;
        }
    }
    // 尾块与主块共用同一混淆链
    if (filled != 0) {
        k *= 0xcc9e2d51u;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593u;
        h ^= k;
    }

    h ^= static_cast<ca::u32>(len);

    // fmix32 finalizer
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

ca::core::ByteSlice slice_of(const char* text)
{
    return ca::core::ByteSlice(reinterpret_cast<const ca::u8*>(text), std::strlen(text));
}

}  // namespace

TEST(Murmur3Test, emptyInputSeedZeroIsZero)
{
    // 数学上可证：h 初值 0，无块可处理，len=0，fmix32(0) 仍为 0
    EXPECT_EQ(murmur3_32(ca::core::ByteSlice(), 0), 0u);
    EXPECT_EQ(murmur3_32(ca::core::ByteSlice()), 0u);
    EXPECT_EQ(murmur3_32(std::string()), 0u);
}

TEST(Murmur3Test, publishedVectors)
{
    // 第三方已发表向量（seed=0）：Google Guava Murmur3Hash32Test 与
    // Rust hashcodecs / twmb-murmur3 / md5calc 等多个独立来源一致，
    // 且与本文件独立参考实现互验通过后才写入。
    EXPECT_EQ(murmur3_32(slice_of("k")), 0xCFBDA5D1u);                       // 1 字节尾块
    EXPECT_EQ(murmur3_32(slice_of("hell")), 0xA167DBF3u);                    // 整块
    EXPECT_EQ(murmur3_32(slice_of("hello")), 0x248BFA47u);                   // 整块 + 1 字节尾块
    EXPECT_EQ(murmur3_32(slice_of("http://www.google.com/")), 0x3D41B97Cu);  // 整块 + 2 字节尾块
    EXPECT_EQ(murmur3_32(slice_of("The quick brown fox jumps over the lazy dog")),
              0x2E4FF723u);                                                  // 整块 + 3 字节尾块
}

TEST(Murmur3Test, matchesIndependentStreamImplementation)
{
    // 确定性伪随机语料：长度 0~300 覆盖全部尾块形态，多 seed 双实现对比
    ca::u32 rng = 0x12345678u;
    auto next_byte = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<ca::u8>(rng >> 24);
    };

    const ca::u32 seeds[] = {0u, 1u, 0x9747B28Cu};
    for (ca::usize len = 0; len <= 300; ++len) {
        std::vector<ca::u8> buf(len);
        for (auto& b : buf)
            b = next_byte();
        for (const ca::u32 seed : seeds) {
            const ca::u32 got = murmur3_32(ca::core::ByteSlice(buf.data(), len), seed);
            const ca::u32 want = murmur3_32_ref_stream(buf.data(), len, seed);
            ASSERT_EQ(got, want) << "len=" << len << " seed=" << seed;
        }
    }
}

TEST(Murmur3Test, differentInputsDifferentHashes)
{
    EXPECT_NE(murmur3_32(slice_of("abc")), murmur3_32(slice_of("abd")));
    EXPECT_NE(murmur3_32(slice_of("a")), murmur3_32(slice_of("ab")));
    // 仅末字节不同的 4 字节输入
    EXPECT_NE(murmur3_32(slice_of("abcd")), murmur3_32(slice_of("abce")));
}

TEST(Murmur3Test, seedChangesHash)
{
    const ca::u8 data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    const ca::core::ByteSlice view(data, sizeof(data));
    EXPECT_NE(murmur3_32(view, 0), murmur3_32(view, 1));
    EXPECT_NE(murmur3_32(view, 0), murmur3_32(view, 0x9747B28Cu));
}

TEST(Murmur3Test, sameInputStableHash)
{
    // 与已验证向量的稳定性：同输入同 seed 重复计算结果一致
    EXPECT_EQ(murmur3_32(slice_of("hello")), murmur3_32(slice_of("hello")));
    EXPECT_EQ(murmur3_32(slice_of("hello"), 42u), murmur3_32(slice_of("hello"), 42u));
    EXPECT_EQ(murmur3_32(slice_of("hello")), 0x248BFA47u);
}

TEST(Murmur3Test, convenienceOverloads)
{
    EXPECT_EQ(murmur3_32(std::string("hello")), murmur3_32(slice_of("hello")));
    const ca::u8 data[] = {'h', 'i'};
    EXPECT_EQ(murmur3_32(data, sizeof(data)), murmur3_32(std::string("hi")));
}
