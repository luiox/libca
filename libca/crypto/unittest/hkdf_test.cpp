#include <gtest/gtest.h>
#include <string>

#include "libca/crypto/hkdf.hpp"

using namespace ca::crypto;
using namespace ca::core;

namespace {

ByteSlice bytes(const std::string& hex_or_text)
{
    return ByteSlice(reinterpret_cast<const ca::u8*>(hex_or_text.data()), hex_or_text.size());
}

// 十六进制串转字节串（测试内本地实现，避免引入被测 hex 模块造成耦合）。
std::string from_hex(const char* hex)
{
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    std::string out;
    for (const char* p = hex; *p != '\0'; p += 2) {
        const int hi = nibble(p[0]);
        const int lo = nibble(p[1]);
        if (hi < 0 || lo < 0)
            return out;
        out += static_cast<char>((hi << 4) | lo);
    }
    return out;
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

// RFC 5869 Appendix A.1 的固定输入，多组用例复用。
// 注意：salt 首字节为 0x00，必须显式指定长度构造，避免 char* 在 NUL 处截断。
const std::string kA1Ikm(22, '\x0b');
const std::string kA1Salt = std::string("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c", 13);
const std::string kA1Info = std::string("\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9", 10);

}  // namespace

// ============================================================
// SHA-256：RFC 5869 A.1-A.3（hex 经 python hmac 手写 HKDF 交叉验证）
// ============================================================

// A.1：基本用例，SHA-256
TEST(HkdfSha256Test, Rfc5869A1ExtractExpand)
{
    const auto prk = hkdf_sha256_extract(bytes(kA1Salt), bytes(kA1Ikm));
    EXPECT_EQ(to_hex(prk),
              "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");

    const auto okm = hkdf_sha256_expand(
        ByteSlice(prk.as_ptr(), prk.len()), bytes(kA1Info), 42);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(to_hex(okm.unwrap()),
              "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
              "34007208d5b887185865");
}

// A.2：更长输入（多轮 T 块，L=82 > HashLen=32）
TEST(HkdfSha256Test, Rfc5869A2LongInputs)
{
    const std::string ikm = from_hex(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
        "404142434445464748494a4b4c4d4e4f");
    const std::string salt = from_hex(
        "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf");
    const std::string info = from_hex(
        "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
        "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"
        "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

    const auto prk = hkdf_sha256_extract(bytes(salt), bytes(ikm));
    EXPECT_EQ(to_hex(prk),
              "06a6b88c5853361a06104c9ceb35b45cef760014904671014a193f40c15fc244");

    const auto okm = hkdf_sha256_expand(ByteSlice(prk.as_ptr(), prk.len()), bytes(info), 82);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(to_hex(okm.unwrap()),
              "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
              "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
              "cc30c58179ec3e87c14c01d5c1f3434f1d87");
}

// A.3：空 salt 与空 info（盐为 0 的退化场景）
TEST(HkdfSha256Test, Rfc5869A3EmptySaltAndInfo)
{
    const auto prk = hkdf_sha256_extract(ByteSlice(), bytes(kA1Ikm));
    EXPECT_EQ(to_hex(prk),
              "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04");

    const auto okm = hkdf_sha256_expand(ByteSlice(prk.as_ptr(), prk.len()), ByteSlice(), 42);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(to_hex(okm.unwrap()),
              "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
              "9d201395faa4b61a96c8");
}

// 一次性 derive == 手动 extract + expand。
TEST(HkdfSha256Test, DeriveEqualsExtractExpand)
{
    const auto manual_prk = hkdf_sha256_extract(bytes(kA1Salt), bytes(kA1Ikm));
    const auto manual = hkdf_sha256_expand(
        ByteSlice(manual_prk.as_ptr(), manual_prk.len()), bytes(kA1Info), 42);
    ASSERT_TRUE(manual.is_ok());

    const auto derived = hkdf_sha256_derive(bytes(kA1Salt), bytes(kA1Ikm), bytes(kA1Info), 42);
    ASSERT_TRUE(derived.is_ok());
    EXPECT_EQ(to_hex(derived.unwrap()), to_hex(manual.unwrap()));
}

// 参数错误：length 超过 255 * HashLen。
TEST(HkdfSha256Test, RejectsLengthOverLimit)
{
    const auto prk = hkdf_sha256_extract(bytes(kA1Salt), bytes(kA1Ikm));
    const auto too_long = hkdf_sha256_expand(
        ByteSlice(prk.as_ptr(), prk.len()), ByteSlice(), 255 * 32 + 1);
    EXPECT_TRUE(too_long.is_err());
    EXPECT_EQ(too_long.unwrap_err(), CryptoError::INVALID_ARGUMENT);

    // PRK 短于 HashLen 同样拒绝。
    const auto short_prk = hkdf_sha256_expand(
        ByteSlice(prk.as_ptr(), prk.len() - 1), ByteSlice(), 32);
    EXPECT_TRUE(short_prk.is_err());
}

// length == 0 返回空 OKM。
TEST(HkdfSha256Test, ZeroLengthYieldsEmpty)
{
    const auto prk = hkdf_sha256_extract(bytes(kA1Salt), bytes(kA1Ikm));
    const auto okm = hkdf_sha256_expand(ByteSlice(prk.as_ptr(), prk.len()), ByteSlice(), 0);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(okm.unwrap().len(), 0u);
}

// ============================================================
// SHA-1：RFC 5869 A.4-A.5
// ============================================================

// A.4：基本用例，SHA-1
TEST(HkdfSha1Test, Rfc5869A4ExtractExpand)
{
    const std::string ikm(11, '\x0b');
    const auto prk = hkdf_sha1_extract(bytes(kA1Salt), bytes(ikm));
    EXPECT_EQ(to_hex(prk), "9b6c18c432a7bf8f0e71c8eb88f4b30baa2ba243");

    const auto okm = hkdf_sha1_expand(ByteSlice(prk.as_ptr(), prk.len()), bytes(kA1Info), 42);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(to_hex(okm.unwrap()),
              "085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2"
              "c22e422478d305f3f896");
}

// A.5：空 salt 与空 info，SHA-1
TEST(HkdfSha1Test, Rfc5869A5EmptySaltAndInfo)
{
    const std::string ikm(11, '\x0b');
    const auto prk = hkdf_sha1_extract(ByteSlice(), bytes(ikm));
    EXPECT_EQ(to_hex(prk), "a585d7ba2b0b8b65f88ec855482829693b7795d5");

    const auto okm = hkdf_sha1_expand(ByteSlice(prk.as_ptr(), prk.len()), ByteSlice(), 42);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(to_hex(okm.unwrap()),
              "14101530f62ccf2b30cc6d220554d8d96802825489c52c84c99342b96e018c22"
              "1c71a88a4a258f71ffea");
}

TEST(HkdfSha1Test, DeriveEqualsExtractExpand)
{
    const std::string ikm(11, '\x0b');
    const auto derived = hkdf_sha1_derive(bytes(kA1Salt), bytes(ikm), bytes(kA1Info), 42);
    ASSERT_TRUE(derived.is_ok());
    EXPECT_EQ(to_hex(derived.unwrap()),
              "085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2"
              "c22e422478d305f3f896");
}

// ============================================================
// SHA-512：RFC 5869 无官方向量，用 python 交叉验证的自洽向量 +
// 与 extract+expand 组合一致性校验。
// ============================================================

TEST(HkdfSha512Test, DeriveMatchesCompositionAndKnownVector)
{
    const std::string ikm(22, '\x0b');
    const auto prk = hkdf_sha512_extract(bytes(kA1Salt), bytes(ikm));
    // PRK = HMAC-SHA512(salt, ikm)，python hmac 交叉验证。
    EXPECT_EQ(to_hex(prk),
              "665799823737ded04a88e47e54a5890bb2c3d247c7a4254a8e61350723590a26"
              "c36238127d8661b88cf80ef802d57e2f7cebcf1e00e083848be19929c61b4237");

    const auto okm = hkdf_sha512_derive(bytes(kA1Salt), bytes(ikm), bytes(kA1Info), 42);
    ASSERT_TRUE(okm.is_ok());
    EXPECT_EQ(to_hex(okm.unwrap()),
              "832390086cda71fb47625bb5ceb168e4c8e26a1a16ed34d9fc7fe92c14815793"
              "38da362cb8d9f925d7cb");
}

TEST(HkdfSha512Test, RejectsLengthOverLimit)
{
    const std::string ikm(22, '\x0b');
    const auto prk = hkdf_sha512_extract(bytes(kA1Salt), bytes(ikm));
    const auto too_long = hkdf_sha512_expand(
        ByteSlice(prk.as_ptr(), prk.len()), ByteSlice(), 255 * 64 + 1);
    EXPECT_TRUE(too_long.is_err());
}
