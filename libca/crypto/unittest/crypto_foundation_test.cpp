#include <gtest/gtest.h>

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

TEST(RandomTest, SecureRandomBytesLength)
{
    auto random = secure_random_bytes(32);
    ASSERT_TRUE(random.is_ok());
    EXPECT_EQ(random.unwrap().len(), 32u);
}
