#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "libca/core/datatype.hpp"
#include "libca/crypto/sha3.h"

using namespace ca::crypto;

// NIST FIPS 202 标准测试向量。

TEST(Sha3Test, EmptyInput) {
    EXPECT_EQ(SHA3(SHA3::Bits224)(""),
              "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7");
    EXPECT_EQ(SHA3(SHA3::Bits256)(""),
              "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
    EXPECT_EQ(SHA3(SHA3::Bits384)(""),
              "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2a"
              "c3713831264adb47fb6bd1e058d5f004");
    EXPECT_EQ(SHA3(SHA3::Bits512)(""),
              "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
              "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");
}

TEST(Sha3Test, Abc) {
    EXPECT_EQ(SHA3(SHA3::Bits224)("abc"),
              "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf");
    EXPECT_EQ(SHA3(SHA3::Bits256)("abc"),
              "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
    EXPECT_EQ(SHA3(SHA3::Bits384)("abc"),
              "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b2"
              "98d88cea927ac7f539f1edf228376d25");
    EXPECT_EQ(SHA3(SHA3::Bits512)("abc"),
              "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e"
              "10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0");
}

TEST(Sha3Test, NistExampleMessage) {
    // FIPS 202 附录 A 官方示例消息（56 字节 / 448 位）：跨 SHA3-256/384 的 rate 边界，
    // 覆盖多块吸收路径。
    const std::string message =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    EXPECT_EQ(SHA3(SHA3::Bits224)(message),
              "8a24108b154ada21c9fd5574494479ba5c7e7ab76ef264ead0fcce33");
    EXPECT_EQ(SHA3(SHA3::Bits256)(message),
              "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376");
    EXPECT_EQ(SHA3(SHA3::Bits384)(message),
              "991c665755eb3a4b6bbdfb75c78a492e8c56a22c5c4d7e429bfdbc32b9d4ad5a"
              "a04a1f076e62fea19eef51acd0657c22");
}

TEST(Sha3Test, GetHashIsIdempotentAndNonDestructive) {
    // get_hash() 不破坏内部状态：重复取值一致，且之后继续 add 仍等价于一次性喂入。
    SHA3 sha3;
    sha3.add("abc", 3);
    const std::string first = sha3.get_hash();
    EXPECT_EQ(sha3.get_hash(), first);
    sha3.add("def", 3);
    EXPECT_EQ(sha3.get_hash(), SHA3(SHA3::Bits256)("abcdef"));
}

TEST(Sha3Test, SplitAtEveryRateBoundary) {
    // 在各变体 rate 边界前后逐一切开，分段喂入须与一次性喂入一致。
    const SHA3::Bits kVariants[] = {SHA3::Bits224, SHA3::Bits256,
                                     SHA3::Bits384, SHA3::Bits512};
    const std::string payload(500, 'x');   // 远大于任一变体的 rate
    const ca::usize kSplitPoints[] = {71, 72, 73, 103, 104, 105, 135,
                                      136, 137, 143, 144, 145, 499};
    for (SHA3::Bits bits : kVariants) {
        const std::string expected = SHA3(bits)(payload);
        for (ca::usize split : kSplitPoints) {
            SHA3 stream(bits);
            stream.add(payload.data(), split);
            stream.add(payload.data() + split, payload.size() - split);
            EXPECT_EQ(stream.get_hash(), expected)
                << "bits=" << static_cast<int>(bits) << " split=" << split;
        }
    }
}

TEST(Sha3Test, EmptyAddIsNoOp) {
    SHA3 sha3;
    sha3.add(nullptr, 0);
    EXPECT_EQ(sha3.get_hash(),
              "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
}

TEST(Sha3Test, DigestLengthMatchesVariant) {
    EXPECT_EQ(SHA3(SHA3::Bits224)("").size(), 56u);
    EXPECT_EQ(SHA3(SHA3::Bits256)("").size(), 64u);
    EXPECT_EQ(SHA3(SHA3::Bits384)("").size(), 96u);
    EXPECT_EQ(SHA3(SHA3::Bits512)("").size(), 128u);
}

TEST(Sha3Test, RawDataOverloadMatchesStringOverload) {
    const char data[] = "How are you";
    SHA3 a;
    SHA3 b;
    EXPECT_EQ(a(data, sizeof(data) - 1), b(std::string(data)));
}

TEST(Sha3Test, StreamingMatchesOneShot) {
    const std::string text = "The quick brown fox jumps over the lazy dog";
    SHA3 oneshot;
    const std::string expected = oneshot(text);

    SHA3 streaming;
    // 按不规则块大小分段喂入，覆盖跨块缓冲路径。
    ca::usize pos = 0;
    const ca::usize chunks[] = {1, 3, 7, 13, 100};
    for (ca::usize c : chunks) {
        if (pos >= text.size()) break;
        const ca::usize n = std::min<ca::usize>(c, text.size() - pos);
        streaming.add(text.data() + pos, n);
        pos += n;
    }
    EXPECT_EQ(streaming.get_hash(), expected);
}

TEST(Sha3Test, ResetAllowsReuse) {
    SHA3 sha3;
    sha3.add("garbage", 7);
    sha3.reset();
    EXPECT_EQ(sha3("abc"),
              "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
}

TEST(Sha3Test, LongInputCrossesBlockBoundary) {
    // 200 字节 'a'：大于任一变体的块大小，覆盖多块处理路径。
    const std::string input(200, 'a');
    SHA3 sha3;
    const std::string once = sha3(input);

    SHA3 split;
    split.add(input.data(), 137);
    split.add(input.data() + 137, input.size() - 137);
    EXPECT_EQ(split.get_hash(), once);
}

TEST(Sha3Test, UnalignedBufferInput) {
    // 同一份内容分别从对齐/非对齐位置喂入：process_block 曾按 uint64_t* 直接解引用，
    // 严格对齐架构上是 UB；修复后非对齐缓冲结果须与对齐一致。
    unsigned char pattern[256];
    for (ca::usize i = 0; i < 256; ++i)
        pattern[i] = static_cast<unsigned char>(i * 7 + 3);

    SHA3 aligned;
    const std::string expected = aligned(pattern, sizeof(pattern));

    alignas(8) unsigned char storage[8 + 256];
    for (ca::usize offset = 1; offset < 8; ++offset) {
        std::memcpy(storage + offset, pattern, sizeof(pattern));
        SHA3 unaligned;
        EXPECT_EQ(unaligned(storage + offset, sizeof(pattern)), expected) << "offset=" << offset;
    }
}
