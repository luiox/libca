#include <gtest/gtest.h>

#include "libca/crypto/chacha20.hpp"
#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/hex.hpp"
#include "libca/crypto/rc4.hpp"

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

Bytes decode_hex_or_empty(const std::string& text)
{
    auto decoded = hex_decode(text);
    if (decoded.is_err())
        return {};
    return decoded.unwrap();
}

}  // namespace

TEST(RC4Test, KnownVector)
{
    auto encrypted = rc4_crypt(bytes("Key"), bytes("Plaintext"));
    ASSERT_TRUE(encrypted.is_ok());
    EXPECT_EQ(hex_encode(bytes(encrypted.unwrap())), "bbf316e8d940af0ad3");

    auto ciphertext = decode_hex_or_empty("bbf316e8d940af0ad3");
    auto decrypted = rc4_crypt(bytes("Key"), bytes(ciphertext));
    ASSERT_TRUE(decrypted.is_ok());
    EXPECT_TRUE(constant_time_eq(bytes("Plaintext"), bytes(decrypted.unwrap())));
}

TEST(ChaCha20Test, Rfc8439BlockFunction)
{
    ca::u8 key_data[CHACHA20_KEY_SIZE];
    for (ca::usize i = 0; i < CHACHA20_KEY_SIZE; ++i)
        key_data[i] = static_cast<ca::u8>(i);

    ca::u8 nonce_data[CHACHA20_NONCE_SIZE] = {
        0x00, 0x00, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x4a,
        0x00, 0x00, 0x00, 0x00,
    };

    auto block = chacha20_block(
        ByteSlice(key_data, CHACHA20_KEY_SIZE),
        1,
        ByteSlice(nonce_data, CHACHA20_NONCE_SIZE));

    ASSERT_TRUE(block.is_ok());
    EXPECT_EQ(hex_encode(bytes(block.unwrap())),
              "10f1e7e4d13b5915500fdd1fa32071c4"
              "c7d1f4c733c068030422aa9ac3d46c4e"
              "d2826446079faa0914c2d705d98b02a2"
              "b5129cd1de164eb9cbd083e8a2503c4e");
}

TEST(ChaCha20Test, Rfc8439Encryption)
{
    ca::u8 key_data[CHACHA20_KEY_SIZE];
    for (ca::usize i = 0; i < CHACHA20_KEY_SIZE; ++i)
        key_data[i] = static_cast<ca::u8>(i);

    ca::u8 nonce_data[CHACHA20_NONCE_SIZE] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x4a,
        0x00, 0x00, 0x00, 0x00,
    };

    const std::string plaintext =
        "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";

    auto encrypted = chacha20_xor(
        ByteSlice(key_data, CHACHA20_KEY_SIZE),
        1,
        ByteSlice(nonce_data, CHACHA20_NONCE_SIZE),
        bytes(plaintext));

    ASSERT_TRUE(encrypted.is_ok());
    EXPECT_EQ(hex_encode(bytes(encrypted.unwrap())),
              "6e2e359a2568f98041ba0728dd0d6981"
              "e97e7aec1d4360c20a27afccfd9fae0b"
              "f91b65c5524733ab8f593dabcd62b357"
              "1639d624e65152ab8f530c359f0861d8"
              "07ca0dbf500d6a6156a38e088a22b65e"
              "52bc514d16ccf806818ce91ab7793736"
              "5af90bbf74a35be6b40b8eedf2785e42"
              "874d");

    auto decrypted = chacha20_xor(
        ByteSlice(key_data, CHACHA20_KEY_SIZE),
        1,
        ByteSlice(nonce_data, CHACHA20_NONCE_SIZE),
        bytes(encrypted.unwrap()));

    ASSERT_TRUE(decrypted.is_ok());
    EXPECT_TRUE(constant_time_eq(bytes(plaintext), bytes(decrypted.unwrap())));
}
