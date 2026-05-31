#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "libca/crypto/base64.hpp"
#include "libca/crypto/crc.hpp"

using namespace ca::crypto;

// ============================================================
// Base64 tests
// ============================================================

TEST(Base64Test, emptyString) {
    EXPECT_EQ(base64Encode("", 0), "");
    EXPECT_EQ(base64Encode(std::string()), "");
    EXPECT_TRUE(base64Decode("").empty());
}

TEST(Base64Test, rfc4648TestVectors) {
    // RFC 4648 Section 10 test vectors
    EXPECT_EQ(base64Encode("f", 1), "Zg==");
    EXPECT_EQ(base64Encode("fo", 2), "Zm8=");
    EXPECT_EQ(base64Encode("foo", 3), "Zm9v");
    EXPECT_EQ(base64Encode("foob", 4), "Zm9vYg==");
    EXPECT_EQ(base64Encode("fooba", 5), "Zm9vYmE=");
    EXPECT_EQ(base64Encode("foobar", 6), "Zm9vYmFy");
}

TEST(Base64Test, encodeDecodeRoundtrip) {
    const char* testCases[] = {
        "",
        "Hello, World!",
        "Base64 Encoding Test",
        "1234567890",
        "!@#$%^&*()",
        "a",
        "ab",
        "abc",
        "abcd",
    };
    for (const auto& tc : testCases) {
        auto encoded = base64Encode(tc, std::strlen(tc));
        auto decoded = base64Decode(encoded);
        std::string decodedStr(decoded.begin(), decoded.end());
        EXPECT_EQ(decodedStr, tc);
    }
}

TEST(Base64Test, binaryDataRoundtrip) {
    uint8_t data[] = {
        0x00, 0x01, 0x02, 0xFF, 0xFE, 0x80, 0x7F, 0x00,
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x90
    };
    auto encoded = base64Encode(reinterpret_cast<const char*>(data), sizeof(data));
    auto decoded = base64Decode(encoded);
    ASSERT_EQ(decoded.size(), sizeof(data));
    EXPECT_EQ(std::memcmp(decoded.data(), data, sizeof(data)), 0);
}

TEST(Base64Test, paddingVariants) {
    // 1 byte  -> 2 padding
    EXPECT_EQ(base64Encode("a", 1), "YQ==");
    // 2 bytes -> 1 padding
    EXPECT_EQ(base64Encode("ab", 2), "YWI=");
    // 3 bytes -> no padding
    EXPECT_EQ(base64Encode("abc", 3), "YWJj");
}

TEST(Base64Test, stdStringOverload) {
    EXPECT_EQ(base64Encode(std::string("Hello")), "SGVsbG8=");
}

// ============================================================
// CRC16 tests
// ============================================================

TEST(Crc16Test, emptyString) {
    EXPECT_EQ(Crc16::calculate(""), 0);
    EXPECT_EQ(Crc16::calculate(std::string()), 0);
}

TEST(Crc16Test, knownValues) {
    // Known CRC16-CCITT values verified against online calculators
    EXPECT_EQ(Crc16::calculate("123456789"), 0x31C3);
}

TEST(Crc16Test, helloWorld) {
    EXPECT_EQ(Crc16::calculate("Hello"), 0xCBD6);
}

TEST(Crc16Test, incrementalCrc) {
    // CRC can be accumulated: crc("ab") = crc("b", 1, crc("a", 1))
    auto crcA = Crc16::calculate("a");
    auto crcAB = Crc16::calculate("b", 1, crcA);
    EXPECT_EQ(crcAB, Crc16::calculate("ab"));
}

TEST(Crc16Test, binaryData) {
    uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55};
    auto crc1 = Crc16::calculate(data, sizeof(data));
    EXPECT_NE(crc1, 0);
    // Same data should produce same CRC
    auto crc2 = Crc16::calculate(data, sizeof(data));
    EXPECT_EQ(crc1, crc2);
}

TEST(Crc16Test, differentDataDifferentCrc) {
    uint16_t crc1 = Crc16::calculate("Hello");
    uint16_t crc2 = Crc16::calculate("World");
    EXPECT_NE(crc1, crc2);
}
