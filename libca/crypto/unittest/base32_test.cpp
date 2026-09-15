#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "libca/crypto/base32.hpp"

using namespace ca::crypto;

namespace {

ca::core::ByteSlice slice_of(const char* text)
{
    return ca::core::ByteSlice(reinterpret_cast<const ca::u8*>(text), std::strlen(text));
}

std::string encode_str(const char* text)
{
    return base32_encode(slice_of(text));
}

std::string to_string(const ca::core::Bytes& bytes)
{
    return std::string(reinterpret_cast<const char*>(bytes.as_ptr()), bytes.len());
}

}  // namespace

TEST(Base32Test, rfc4648EncodeVectors)
{
    // RFC 4648 Section 10 官方 Base32 测试向量
    EXPECT_EQ(base32_encode(ca::core::ByteSlice()), "");
    EXPECT_EQ(encode_str("f"), "MY======");
    EXPECT_EQ(encode_str("fo"), "MZXQ====");
    EXPECT_EQ(encode_str("foo"), "MZXW6===");
    EXPECT_EQ(encode_str("foob"), "MZXW6YQ=");
    EXPECT_EQ(encode_str("fooba"), "MZXW6YTB");
    EXPECT_EQ(encode_str("foobar"), "MZXW6YTBOI======");
}

TEST(Base32Test, rfc4648DecodeVectors)
{
    auto decode_and_check = [](const char* encoded, const char* expected) {
        auto decoded = base32_decode(encoded);
        ASSERT_TRUE(decoded.is_ok()) << encoded;
        EXPECT_EQ(to_string(decoded.unwrap()), expected) << encoded;
    };
    decode_and_check("", "");
    decode_and_check("MY======", "f");
    decode_and_check("MZXQ====", "fo");
    decode_and_check("MZXW6===", "foo");
    decode_and_check("MZXW6YQ=", "foob");
    decode_and_check("MZXW6YTB", "fooba");
    decode_and_check("MZXW6YTBOI======", "foobar");
}

TEST(Base32Test, encodeWithoutPadding)
{
    EXPECT_EQ(base32_encode(slice_of("f"), false), "MY");
    EXPECT_EQ(base32_encode(slice_of("foobar"), false), "MZXW6YTBOI");
}

TEST(Base32Test, decodeAcceptsUnpaddedCanonicalInput)
{
    auto decoded = base32_decode("MZXW6YTBOI");
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(to_string(decoded.unwrap()), "foobar");

    auto single = base32_decode("MY");
    ASSERT_TRUE(single.is_ok());
    EXPECT_EQ(to_string(single.unwrap()), "f");
}

TEST(Base32Test, rejectsInvalidInput)
{
    // 非法字符：小写默认不接受（大小写严格，见头文件文档）；1/8/9 不在 alphabet；空白非法
    EXPECT_TRUE(base32_decode("mzxw6===").is_err());
    EXPECT_TRUE(base32_decode("M1======").is_err());
    EXPECT_TRUE(base32_decode("M8======").is_err());
    EXPECT_TRUE(base32_decode("MZ W6===").is_err());
    // 非法长度：余数 1/3/6 不可能是合法 Base32 数据长度
    EXPECT_TRUE(base32_decode("M").is_err());
    EXPECT_TRUE(base32_decode("MZX").is_err());
    EXPECT_TRUE(base32_decode("MZXW6Y").is_err());
    // padding 非法：数量与数据长度不匹配 / 中间出现 padding / 整组数据后带 padding
    EXPECT_TRUE(base32_decode("MY===").is_err());
    EXPECT_TRUE(base32_decode("MY===A==").is_err());
    EXPECT_TRUE(base32_decode("MZXW6YTB=").is_err());
    EXPECT_TRUE(base32_decode("=====").is_err());
    // 补零位非 0：非 canonical 编码（MZ 对应 'f' 但低 2 位为 01）
    EXPECT_TRUE(base32_decode("MZ======").is_err());
}

TEST(Base32Test, roundtripRandomData)
{
    // 确定性伪随机数据，长度 0~40 全覆盖（含全部 5 字节整组与 1~4 字节尾块形态）
    ca::u32 rng = 0xCAFEF00Du;
    for (ca::usize len = 0; len <= 40; ++len) {
        ca::u8 buf[40];
        for (ca::usize j = 0; j < len; ++j) {
            rng = rng * 1664525u + 1013904223u;
            buf[j] = static_cast<ca::u8>(rng >> 24);
        }
        const std::string encoded = base32_encode(ca::core::ByteSlice(buf, len));
        auto decoded = base32_decode(encoded);
        ASSERT_TRUE(decoded.is_ok()) << "len=" << len << " encoded=" << encoded;
        const auto bytes = decoded.unwrap();
        ASSERT_EQ(bytes.len(), len) << "len=" << len;
        EXPECT_EQ(std::memcmp(bytes.as_ptr(), buf, len), 0) << "len=" << len;
    }
}
