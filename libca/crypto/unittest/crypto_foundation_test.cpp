#include <gtest/gtest.h>

// 聚合头编译健康检查：任何头文件语法损坏都会在这里暴露。
#include "libca/crypto/crypto.hpp"  // IWYU pragma: keep

#include "libca/crypto/base64.hpp"
#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/hex.hpp"
#include "libca/crypto/hmac.hpp"
#include "libca/crypto/random.hpp"
#include "libca/crypto/sha256.hpp"

#include <string>

using namespace ca::crypto;
using namespace ca::core;

namespace {

ByteSlice bytes(const std::string& text)
{
    return ByteSlice(reinterpret_cast<const ca::u8*>(text.data()), text.size());
}

ByteSlice bytes(const Bytes& data)
{
    return ByteSlice(data.as_ptr(), data.len());
}

}  // namespace

// ============================================================
// constant_time_eq（安全敏感原语，直接行为测试）
// ============================================================

TEST(ConstantTimeEqTest, EqualContent)
{
    EXPECT_TRUE(constant_time_eq(bytes(std::string("secret")), bytes(std::string("secret"))));
}

TEST(ConstantTimeEqTest, DifferentContentSameLength)
{
    EXPECT_FALSE(constant_time_eq(bytes(std::string("secret")), bytes(std::string("secreu"))));
    // 仅首字节不同 / 仅末字节不同，两侧都必须判不等。
    EXPECT_FALSE(constant_time_eq(bytes(std::string("Xecret")), bytes(std::string("secret"))));
}

TEST(ConstantTimeEqTest, DifferentLength)
{
    EXPECT_FALSE(constant_time_eq(bytes(std::string("secret")), bytes(std::string("secre"))));
    EXPECT_FALSE(constant_time_eq(bytes(std::string("")), bytes(std::string("x"))));
}

TEST(ConstantTimeEqTest, BothEmpty)
{
    EXPECT_TRUE(constant_time_eq(ca::core::ByteSlice(), ca::core::ByteSlice()));
}

TEST(HexTest, EncodeDecodeRoundtrip)
{
    const std::string input("Hello\0World", 11);
    const auto encoded = hex_encode(bytes(input));
    EXPECT_EQ(encoded, "48656c6c6f00576f726c64");

    auto decoded = hex_decode(encoded);
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_TRUE(constant_time_eq(bytes(input), bytes(decoded.unwrap())));
}

TEST(HexTest, RejectsInvalidInput)
{
    EXPECT_TRUE(hex_decode("abc").is_err());
    EXPECT_TRUE(hex_decode("zz").is_err());
}

TEST(Base64ResultTest, StrictDecode)
{
    const std::string input = "foobar";
    const auto encoded = base64_encode(bytes(input));
    EXPECT_EQ(encoded, "Zm9vYmFy");

    auto decoded = base64_decode(encoded);
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_TRUE(constant_time_eq(bytes(input), bytes(decoded.unwrap())));

    EXPECT_TRUE(base64_decode("Zm9v=").is_err());
    EXPECT_TRUE(base64_decode("Zm=v").is_err());
    EXPECT_TRUE(base64_decode("@m8=").is_err());
    EXPECT_TRUE(base64_decode("AB==").is_err());
    EXPECT_TRUE(base64_decode("AAB=").is_err());
}

TEST(SHA256BytesTest, BytesAndHex)
{
    const auto digest = sha256(bytes("abc"));
    EXPECT_EQ(digest.len(), SHA256::HashBytes);
    EXPECT_EQ(hex_encode(bytes(digest)),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256_hex(bytes("abc")),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(HmacSHA256Test, Rfc4231TestCase1)
{
    ca::u8 key_data[20];
    for (ca::usize i = 0; i < 20; ++i)
        key_data[i] = 0x0b;

    const auto digest = hmac_sha256(
        ca::core::ByteSlice(key_data, 20),
        bytes("Hi There"));

    EXPECT_EQ(hex_encode(bytes(digest)),
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
}

// RFC 4231 Test Case 6/7：key 长度 131 > SHA-256 块长 64，覆盖
// hmac.cpp 的 hash-then-pad（先对 key 做 SHA-256 再补零）分支。
TEST(HmacSHA256Test, Rfc4231TestCase6LargerThanBlockSizeKey)
{
    ca::u8 key_data[131];
    for (ca::usize i = 0; i < 131; ++i)
        key_data[i] = 0xaa;

    const auto digest = hmac_sha256(
        ca::core::ByteSlice(key_data, 131),
        bytes("Test Using Larger Than Block-Size Key - Hash Key First"));

    EXPECT_EQ(hex_encode(bytes(digest)),
              "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

// RFC 4231 Test Case 7：超块长 key + 超块长 data。
TEST(HmacSHA256Test, Rfc4231TestCase7LargerThanBlockSizeKeyAndData)
{
    ca::u8 key_data[131];
    for (ca::usize i = 0; i < 131; ++i)
        key_data[i] = 0xaa;

    const auto digest = hmac_sha256(
        ca::core::ByteSlice(key_data, 131),
        bytes("This is a test using a larger than block-size key and a larger "
              "than block-size data. The key needs to be hashed before being "
              "used by the HMAC algorithm."));

    EXPECT_EQ(hex_encode(bytes(digest)),
              "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2");
}

TEST(RandomTest, SecureRandomBytesLength)
{
    auto random = secure_random_bytes(32);
    ASSERT_TRUE(random.is_ok());
    EXPECT_EQ(random.unwrap().len(), 32u);
}

TEST(SecureZeroTest, ZeroesBufferContent)
{
    ca::u8 buf[] = {1, 2, 3, 4, 5};
    ca::crypto::secure_zero(buf, sizeof(buf));
    for (ca::u8 b : buf)
        EXPECT_EQ(b, 0);

    // size 为 0 时不操作、不崩溃
    ca::crypto::secure_zero(nullptr, 0);
}
