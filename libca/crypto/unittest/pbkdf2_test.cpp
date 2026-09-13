#include <gtest/gtest.h>
#include <string>

#include "libca/crypto/pbkdf2.hpp"

using namespace ca::crypto;
using namespace ca::core;

namespace {

ByteSlice bytes(const std::string& text)
{
    return ByteSlice(reinterpret_cast<const ca::u8*>(text.data()), text.size());
}

std::string to_hex(const Bytes& data)
{
    static const char kHex[17] = "0123456789abcdef";
    std::string result;
    result.reserve(2 * data.len());
    for (ca::usize i = 0; i < data.len(); ++i) {
        const ca::u8 b = data.as_ptr()[i];
        result += kHex[(b >> 4) & 0xF];
        result += kHex[b & 0xF];
    }
    return result;
}

}  // namespace

// ============================================================
// RFC 7914 §11 官方向量（hex 经 python hashlib.pbkdf2_hmac 交叉验证）
// ============================================================

// P="passwd", S="salt", c=1, dkLen=64：单轮 F 函数 + 双块截断路径。
TEST(Pbkdf2HmacSha256Test, Rfc7914OneIteration)
{
    const auto dk = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 1, 64);
    ASSERT_TRUE(dk.is_ok());
    EXPECT_EQ(dk.unwrap().len(), 64u);
    EXPECT_EQ(to_hex(dk.unwrap()),
              "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
              "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
}

// P="passwd", S="salt", c=2, dkLen=64：XOR 链至少执行一次。
TEST(Pbkdf2HmacSha256Test, Rfc7914TwoIterations)
{
    const auto dk = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 2, 64);
    ASSERT_TRUE(dk.is_ok());
    EXPECT_EQ(to_hex(dk.unwrap()),
              "2d412f896e76685e30df569f0a740634e31f031f749d607d9e44210bffb91a6a"
              "b670f500c78862001959f7d7b9f96afb3605700298acb14427e0239463c66f20");
}

// P="passwd", S="salt", c=4096, dkLen=64：典型强度。
TEST(Pbkdf2HmacSha256Test, Rfc7914FourThousandNinetySixIterations)
{
    const auto dk = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 4096, 64);
    ASSERT_TRUE(dk.is_ok());
    EXPECT_EQ(to_hex(dk.unwrap()),
              "21943fd5b7a10905c38fad60157ff498e1e81df1e03254325682a74dca3b2be8"
              "f3ab1ccb49d0a5095e69792ba334c6fdaf55d266a9922c760d3c5f5c3ec22c52");
}

// P="Password", S="NaCl", c=80000, dkLen=64：高强度用例。
TEST(Pbkdf2HmacSha256Test, Rfc7914EightyThousandIterations)
{
    const auto dk = pbkdf2_hmac_sha256(bytes("Password"), bytes("NaCl"), 80000, 64);
    ASSERT_TRUE(dk.is_ok());
    EXPECT_EQ(to_hex(dk.unwrap()),
              "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
              "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
}

// ============================================================
// 参数校验与截断路径
// ============================================================

// iterations == 0 拒绝（RFC 2898 要求 c 为正整数，不回退默认值）。
TEST(Pbkdf2HmacSha256Test, RejectsZeroIterations)
{
    const auto dk = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 0, 64);
    EXPECT_TRUE(dk.is_err());
    EXPECT_EQ(dk.unwrap_err(), CryptoError::INVALID_ARGUMENT);
}

// dk_len == 0 拒绝。
TEST(Pbkdf2HmacSha256Test, RejectsZeroDkLen)
{
    const auto dk = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 1, 0);
    EXPECT_TRUE(dk.is_err());
    EXPECT_EQ(dk.unwrap_err(), CryptoError::INVALID_ARGUMENT);
}

// 非整块长度走末块截断路径：dkLen=33（1 块 + 1 字节）结果必须是
// c=1、dkLen=64 输出的前 33 字节（PBKDF2 前缀性质）。
TEST(Pbkdf2HmacSha256Test, NonMultipleLengthIsPrefixOfFullOutput)
{
    const auto full = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 1, 64);
    const auto cut = pbkdf2_hmac_sha256(bytes("passwd"), bytes("salt"), 1, 33);
    ASSERT_TRUE(full.is_ok());
    ASSERT_TRUE(cut.is_ok());

    const auto cut_hex = to_hex(cut.unwrap());
    ASSERT_EQ(cut_hex.size(), 66u);
    EXPECT_EQ(cut_hex, to_hex(full.unwrap()).substr(0, 66));
}

// 空 password 与空 salt 是合法输入（协议允许，向量由 python 交叉验证）。
TEST(Pbkdf2HmacSha256Test, EmptyPasswordAndSalt)
{
    const auto dk = pbkdf2_hmac_sha256(ByteSlice(), ByteSlice(), 1, 32);
    ASSERT_TRUE(dk.is_ok());
    EXPECT_EQ(to_hex(dk.unwrap()),
              "f7ce0b653d2d72a4108cf5abe912ffdd777616dbbb27a70e8204f3ae2d0f6fad");
}
