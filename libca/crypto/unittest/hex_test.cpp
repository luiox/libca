#include <gtest/gtest.h>

#include "libca/crypto/hex.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace ca::crypto {

TEST(HexTest, EncodesRawBytesAsLowercase) {
    const std::array<ca::u8, 5> input{0x0f, 0x00, 0x10, 0x8a, 0xff};
    const ca::core::ByteSlice bytes(input.data(), input.size());
    const std::string text(reinterpret_cast<const char*>(input.data()), input.size());

    EXPECT_EQ(hex_encode(bytes), "0f00108aff");
    EXPECT_EQ(hex_encode(input.data(), input.size()), "0f00108aff");
    EXPECT_EQ(hex_encode(text), "0f00108aff");
}

TEST(HexTest, DecodesMixedCase) {
    const std::array<ca::u8, 5> expected{0x00, 0x0f, 0x10, 0x8a, 0xff};
    auto decoded = hex_decode("000F108aFf");

    ASSERT_TRUE(decoded.is_ok());
    const auto output = decoded.unwrap();
    ASSERT_EQ(output.len(), expected.size());
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), output.as_ptr()));
}

TEST(HexTest, EmptyInputProducesEmptyOutput) {
    EXPECT_TRUE(hex_encode(ca::core::ByteSlice()).empty());
    auto decoded = hex_decode("");
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_TRUE(decoded.unwrap().is_empty());
}

TEST(HexTest, RejectsMalformedInputs) {
    const std::array<std::string, 7> malformed{
        "abc", "g0", "0g", "0x00", "ff ff ", "-f", std::string("a\0", 2),
    };
    for (const auto& input : malformed) {
        SCOPED_TRACE(input);
        auto decoded = hex_decode(input);
        ASSERT_TRUE(decoded.is_err());
        EXPECT_EQ(decoded.unwrap_err(), CryptoError::INVALID_HEX);
    }
}

}  // namespace ca::crypto
