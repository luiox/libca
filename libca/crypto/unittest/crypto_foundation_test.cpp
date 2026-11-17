#include <gtest/gtest.h>

// 聚合头编译健康检查：任何头文件语法损坏都会在这里暴露。
#include "libca/crypto/crypto.hpp"  // IWYU pragma: keep

#include "libca/crypto/base64.hpp"
#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/hex.hpp"
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

TEST(SecureZeroTest, ZeroesBufferContent)
{
    ca::u8 buf[] = {1, 2, 3, 4, 5};
    ca::crypto::secure_zero(buf, sizeof(buf));
    for (ca::u8 b : buf)
        EXPECT_EQ(b, 0);

    // size 为 0 时不操作、不崩溃
    ca::crypto::secure_zero(nullptr, 0);
}
