#include <gtest/gtest.h>
#include <string>

#include "libca/crypto/hex.hpp"
#include "libca/crypto/hmac.hpp"
#include "libca/crypto/sha1.hpp"

using namespace ca::crypto;
using namespace ca::core;

namespace {

ByteSlice bytes(const std::string& text)
{
    return ByteSlice(reinterpret_cast<const ca::u8*>(text.data()), text.size());
}

std::string hex_of(const Bytes& data)
{
    return hex_encode(ByteSlice(data.as_ptr(), data.len()));
}

}  // namespace

// ============================================================
// HMAC-SHA1：RFC 2202 Test Case 1-7（hex 经 python hmac 模块交叉验证）
// ============================================================

namespace {

// 按值填充重复字节的 key，便于表达 RFC 向量。
std::string repeated_key(ca::u8 value, ca::usize count)
{
    return std::string(count, static_cast<char>(value));
}

}  // namespace

// RFC 2202 Test Case 1：key = 0x0b 重复 20 字节
TEST(HmacSha1Test, Rfc2202TestCase1)
{
    const auto key = repeated_key(0x0b, 20);
    EXPECT_EQ(hmac_sha1_hex(bytes(key), bytes("Hi There")),
              "b617318655057264e28bc0b6fb378c8ef146be00");
}

// RFC 2202 Test Case 2：短 key + 短 data
TEST(HmacSha1Test, Rfc2202TestCase2)
{
    EXPECT_EQ(hmac_sha1_hex(bytes("Jefe"), bytes("what do ya want for nothing?")),
              "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
}

// RFC 2202 Test Case 3：key = 0xaa 重复 20 字节，data = 0xdd 重复 50 字节
TEST(HmacSha1Test, Rfc2202TestCase3)
{
    const auto key = repeated_key(0xaa, 20);
    const std::string data(50, static_cast<char>(0xdd));
    EXPECT_EQ(hmac_sha1_hex(bytes(key), bytes(data)),
              "125d7342b9ac11cd91a39af48aa17b4f63f175d3");
}

// RFC 2202 Test Case 4：25 字节递增 key（恰好跨块内非对齐长度）
TEST(HmacSha1Test, Rfc2202TestCase4)
{
    std::string key;
    for (ca::u8 i = 1; i <= 25; ++i)
        key += static_cast<char>(i);
    const std::string data(50, static_cast<char>(0xcd));
    EXPECT_EQ(hmac_sha1_hex(bytes(key), bytes(data)),
              "4c9007f4026250c6bc8414f9bf50c86c2d7235da");
}

// RFC 2202 Test Case 5：截断展示场景，这里验证完整 20 字节 digest 前 12 字节
// 与 RFC 给出的截断值一致（4c1a03424b55e07fe7f27be1）。
TEST(HmacSha1Test, Rfc2202TestCase5)
{
    const auto key = repeated_key(0x0c, 20);
    const auto digest = hmac_sha1_hex(bytes(key), bytes("Test With Truncation"));
    EXPECT_EQ(digest.substr(0, 24), "4c1a03424b55e07fe7f27be1");
    EXPECT_EQ(digest,
              "4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");
}

// RFC 2202 Test Case 6：80 字节 key > SHA-1 块长 64，覆盖 hash-then-pad 分支
TEST(HmacSha1Test, Rfc2202TestCase6LargerThanBlockSizeKey)
{
    const auto key = repeated_key(0xaa, 80);
    EXPECT_EQ(hmac_sha1_hex(bytes(key),
                            bytes("Test Using Larger Than Block-Size Key - Hash Key First")),
              "aa4ae5e15272d00e95705637ce8a3b55ed402112");
}

// RFC 2202 Test Case 7：超块长 key + 超块长 data
TEST(HmacSha1Test, Rfc2202TestCase7LargerThanBlockSizeKeyAndData)
{
    const auto key = repeated_key(0xaa, 80);
    EXPECT_EQ(hmac_sha1_hex(bytes(key),
                            bytes("This is a test using a larger than block-size key and a "
                                  "larger than block-size data. The key needs to be hashed "
                                  "before being used by the HMAC algorithm.")),
              "ee8089bab17872f013f572dba60ef38e32573f15");
}

// Bytes 版本与 hex 版本结果一致。
TEST(HmacSha1Test, BytesVariantMatchesHex)
{
    const auto digest = hmac_sha1(bytes("Jefe"), bytes("what do ya want for nothing?"));
    EXPECT_EQ(digest.len(), SHA1::HashBytes);
    EXPECT_EQ(hex_of(digest), hmac_sha1_hex(bytes("Jefe"), bytes("what do ya want for nothing?")));
}

// ============================================================
// HMAC-SHA512：RFC 4231 Test Case 1-7
// ============================================================

// RFC 4231 Test Case 1：key = 0x0b 重复 20 字节
TEST(HmacSha512Test, Rfc4231TestCase1)
{
    const auto key = repeated_key(0x0b, 20);
    EXPECT_EQ(hmac_sha512_hex(bytes(key), bytes("Hi There")),
              "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
              "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");
}

// RFC 4231 Test Case 2
TEST(HmacSha512Test, Rfc4231TestCase2)
{
    EXPECT_EQ(hmac_sha512_hex(bytes("Jefe"), bytes("what do ya want for nothing?")),
              "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
              "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");
}

// RFC 4231 Test Case 3
TEST(HmacSha512Test, Rfc4231TestCase3)
{
    const auto key = repeated_key(0xaa, 20);
    const std::string data(50, static_cast<char>(0xdd));
    EXPECT_EQ(hmac_sha512_hex(bytes(key), bytes(data)),
              "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39"
              "bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb");
}

// RFC 4231 Test Case 4：25 字节递增 key
TEST(HmacSha512Test, Rfc4231TestCase4)
{
    std::string key;
    for (ca::u8 i = 1; i <= 25; ++i)
        key += static_cast<char>(i);
    const std::string data(50, static_cast<char>(0xcd));
    EXPECT_EQ(hmac_sha512_hex(bytes(key), bytes(data)),
              "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3db"
              "a91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd");
}

// RFC 4231 Test Case 5：截断展示场景，验证完整 digest 前 16 字节
// 与 RFC 给出的截断值一致（415fad6271580a531d4179bc891d87a6）。
TEST(HmacSha512Test, Rfc4231TestCase5)
{
    const auto key = repeated_key(0x0c, 20);
    const auto digest = hmac_sha512_hex(bytes(key), bytes("Test With Truncation"));
    EXPECT_EQ(digest.substr(0, 32), "415fad6271580a531d4179bc891d87a6");
    EXPECT_EQ(digest,
              "415fad6271580a531d4179bc891d87a650188707922a4fbb36663a1eb16da008"
              "711c5b50ddd0fc235084eb9d3364a1454fb2ef67cd1d29fe6773068ea266e96b");
}

// RFC 4231 Test Case 6：131 字节 key > SHA-512 块长 128，覆盖 hash-then-pad 分支
TEST(HmacSha512Test, Rfc4231TestCase6LargerThanBlockSizeKey)
{
    const auto key = repeated_key(0xaa, 131);
    EXPECT_EQ(hmac_sha512_hex(bytes(key),
                              bytes("Test Using Larger Than Block-Size Key - Hash Key First")),
              "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
              "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598");
}

// RFC 4231 Test Case 7：超块长 key + 超块长 data
TEST(HmacSha512Test, Rfc4231TestCase7LargerThanBlockSizeKeyAndData)
{
    const auto key = repeated_key(0xaa, 131);
    EXPECT_EQ(hmac_sha512_hex(bytes(key),
                              bytes("This is a test using a larger than block-size key and a "
                                    "larger than block-size data. The key needs to be hashed "
                                    "before being used by the HMAC algorithm.")),
              "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944"
              "b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58");
}

TEST(HmacSha512Test, BytesVariantMatchesHex)
{
    const auto digest = hmac_sha512(bytes("Jefe"), bytes("what do ya want for nothing?"));
    EXPECT_EQ(digest.len(), 64u);
    EXPECT_EQ(hex_of(digest), hmac_sha512_hex(bytes("Jefe"), bytes("what do ya want for nothing?")));
}

// ============================================================
// HMAC 家族回归：hmac_sha256 重构到共享模板后行为不变（RFC 4231 TC2）
// ============================================================

TEST(HmacSha256Test, Rfc4231TestCase2AfterSharedImplRefactor)
{
    EXPECT_EQ(hmac_sha256_hex(bytes("Jefe"), bytes("what do ya want for nothing?")),
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}
