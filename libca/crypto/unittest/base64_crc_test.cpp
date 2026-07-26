#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "libca/crypto/base64.hpp"
#include "libca/crypto/crc.hpp"

using namespace ca::crypto;

namespace {

ca::core::ByteSlice slice_of(const char* text) {
    return ca::core::ByteSlice(reinterpret_cast<const ca::u8*>(text), std::strlen(text));
}

std::string encode_str(const char* text) {
    return base64_encode(slice_of(text));
}

}  // namespace

// ============================================================
// Base64 tests
// ============================================================

TEST(Base64Test, emptyString) {
    EXPECT_EQ(base64_encode(ca::core::ByteSlice()), "");
    auto decoded = base64_decode("");
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_TRUE(decoded.unwrap().is_empty());
}

TEST(Base64Test, rfc4648TestVectors) {
    // RFC 4648 Section 10 test vectors
    EXPECT_EQ(encode_str("f"), "Zg==");
    EXPECT_EQ(encode_str("fo"), "Zm8=");
    EXPECT_EQ(encode_str("foo"), "Zm9v");
    EXPECT_EQ(encode_str("foob"), "Zm9vYg==");
    EXPECT_EQ(encode_str("fooba"), "Zm9vYmE=");
    EXPECT_EQ(encode_str("foobar"), "Zm9vYmFy");
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
        auto encoded = encode_str(tc);
        auto decoded = base64_decode(encoded);
        ASSERT_TRUE(decoded.is_ok()) << tc;
        auto bytes = decoded.unwrap();
        std::string decodedStr(reinterpret_cast<const char*>(bytes.as_ptr()), bytes.len());
        EXPECT_EQ(decodedStr, tc);
    }
}

TEST(Base64Test, binaryDataRoundtrip) {
    uint8_t data[] = {
        0x00, 0x01, 0x02, 0xFF, 0xFE, 0x80, 0x7F, 0x00,
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x90
    };
    auto encoded = base64_encode(ca::core::ByteSlice(data, sizeof(data)));
    auto decoded = base64_decode(encoded);
    ASSERT_TRUE(decoded.is_ok());
    auto bytes = decoded.unwrap();
    ASSERT_EQ(bytes.len(), sizeof(data));
    EXPECT_EQ(std::memcmp(bytes.as_ptr(), data, sizeof(data)), 0);
}

TEST(Base64Test, paddingVariants) {
    // 1 byte  -> 2 padding
    EXPECT_EQ(encode_str("a"), "YQ==");
    // 2 bytes -> 1 padding
    EXPECT_EQ(encode_str("ab"), "YWI=");
    // 3 bytes -> no padding
    EXPECT_EQ(encode_str("abc"), "YWJj");
}

TEST(Base64Test, rejectsInvalidInput) {
    // 旧实现遇到这些输入会静默返回截断数据；严格解码必须显式报错。
    EXPECT_TRUE(base64_decode("Zg=").is_err());       // 长度非 4 的倍数
    EXPECT_TRUE(base64_decode("Zm9v!Zm9v!!!").is_err()); // 非法字符
    EXPECT_TRUE(base64_decode("Zg==Zm8=").is_err());  // padding 后还有数据
    EXPECT_TRUE(base64_decode("=Zg=").is_err());      // padding 出现在头部
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
