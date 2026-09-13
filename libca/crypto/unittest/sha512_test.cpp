#include <gtest/gtest.h>
#include <string>

#include "libca/crypto/crypto_error.hpp"
#include "libca/crypto/sha512.hpp"

using namespace ca::crypto;
using namespace ca::core;

namespace {

std::string bytes_to_hex(const ca::core::Bytes& data)
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

ByteSlice bytes_of(const std::string& text)
{
    return ByteSlice(reinterpret_cast<const ca::u8*>(text.data()), text.size());
}

}  // namespace

// ============================================================
// SHA-512：FIPS 180-4 附录 C（D.1 为消息示例）
// 空串 / "abc" / 两块消息 / 100 万个 'a'，hex 值经 python hashlib 交叉验证。
// ============================================================

TEST(SHA512Test, EmptyString)
{
    SHA512 sha512;
    EXPECT_EQ(sha512(""),
              "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
              "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
}

TEST(SHA512Test, Abc)
{
    SHA512 sha512;
    EXPECT_EQ(sha512("abc"),
              "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
              "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

// FIPS 180-4 两块消息示例（896 位），跨分组边界。
TEST(SHA512Test, TwoBlockMessage)
{
    const std::string message =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
        "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    SHA512 sha512;
    EXPECT_EQ(sha512(message),
              "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
              "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
}

// 多分组吸收 + 长度字段跨块（消息远超 2^61 字节才动用高 64 位，此例验证低 64 位大数）。
TEST(SHA512Test, MillionA)
{
    const std::string message(1000000, 'a');
    SHA512 sha512;
    EXPECT_EQ(sha512(message),
              "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
              "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");
}

// 增量更新（逐字节喂入）必须等于一次性结果：覆盖残余缓冲/整块两条路径。
TEST(SHA512Test, IncrementalEqualsOneShot)
{
    const std::string message =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
        "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

    SHA512 one_shot;
    const auto expected = one_shot(message);

    SHA512 incremental;
    for (char c : message)
        incremental.add(&c, 1);
    EXPECT_EQ(incremental.get_hash(), expected);

    // 不按块对齐的分块也要一致（消息总长 112 字节，避免越过末尾）。
    SHA512 chunked;
    chunked.add(message.data(), 25);
    chunked.add(message.data() + 25, 50);
    chunked.add(message.data() + 75, message.size() - 75);
    EXPECT_EQ(chunked.get_hash(), expected);
}

TEST(SHA512Test, ResetReuse)
{
    SHA512 sha512;
    EXPECT_EQ(sha512("abc"),
              "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
              "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    sha512.reset();
    EXPECT_EQ(sha512(""),
              "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
              "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
}

TEST(SHA512Test, GetHashRepeatable)
{
    SHA512 sha512;
    sha512.add("abc", 3);
    // get_hash 不重置状态：两次调用结果一致。
    EXPECT_EQ(sha512.get_hash(), sha512.get_hash());
}

TEST(SHA512Test, RawBytesOutput)
{
    SHA512 sha512;
    sha512.add("abc", 3);
    unsigned char hash[SHA512::HashBytes];
    sha512.get_hash(hash);
    EXPECT_EQ(hash[0], 0xdd);
    EXPECT_EQ(hash[1], 0xaf);
    EXPECT_EQ(hash[2], 0x35);
    EXPECT_EQ(hash[3], 0xa1);
    EXPECT_EQ(hash[SHA512::HashBytes - 1], 0x9f);
}

// ============================================================
// SHA-384：FIPS 180-4，SHA-512 换 IV 截断为 48 字节
// ============================================================

TEST(SHA384Test, EmptyString)
{
    SHA384 sha384;
    EXPECT_EQ(sha384(""),
              "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1d"
              "a274edebfe76f65fbd51ad2f14898b95b");
}

TEST(SHA384Test, Abc)
{
    SHA384 sha384;
    EXPECT_EQ(sha384("abc"),
              "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
              "8086072ba1e7cc2358baeca134c825a7");
}

TEST(SHA384Test, TwoBlockMessage)
{
    const std::string message =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
        "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    SHA384 sha384;
    EXPECT_EQ(sha384(message),
              "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712"
              "fcc7c71a557e2db966c3e9fa91746039");
}

TEST(SHA384Test, MillionA)
{
    const std::string message(1000000, 'a');
    SHA384 sha384;
    EXPECT_EQ(sha384(message),
              "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b"
              "07b8b3dc38ecc4ebae97ddd87f3d8985");
}

// SHA-384 与 SHA-512 初始 IV 不同（并非同一消息 SHA-512 摘要的截断），
// 这里只验证逐字节增量与一次性结果一致。
TEST(SHA384Test, TruncationOfSha512)
{
    const std::string message = "abc";

    SHA384 one_shot;
    const auto expected = one_shot(message);

    SHA384 bytewise;
    for (char c : message)
        bytewise.add(&c, 1);
    EXPECT_EQ(bytewise.get_hash(), expected);
}

TEST(SHA384Test, IncrementalEqualsOneShot)
{
    const std::string message(1000000, 'a');

    SHA384 one_shot;
    const auto expected = one_shot(message);

    SHA384 chunked;
    chunked.add(message.data(), 64);
    chunked.add(message.data() + 64, 128);
    chunked.add(message.data() + 192, message.size() - 192);
    EXPECT_EQ(chunked.get_hash(), expected);
}

TEST(SHA384Test, ResetReuse)
{
    SHA384 sha384;
    EXPECT_EQ(sha384("abc"),
              "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
              "8086072ba1e7cc2358baeca134c825a7");
    sha384.reset();
    EXPECT_EQ(sha384(""),
              "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1d"
              "a274edebfe76f65fbd51ad2f14898b95b");
}

TEST(SHA384Test, RawBytesOutput)
{
    SHA384 sha384;
    sha384.add("abc", 3);
    unsigned char hash[SHA384::HashBytes];
    sha384.get_hash(hash);
    EXPECT_EQ(hash[0], 0xcb);
    EXPECT_EQ(hash[1], 0x00);
    EXPECT_EQ(hash[2], 0x75);
    EXPECT_EQ(hash[3], 0x3f);
}

// ============================================================
// 一次性 API（Bytes / hex）
// ============================================================

TEST(Sha512BytesTest, BytesAndHex)
{
    const auto digest = sha512(bytes_of("abc"));
    EXPECT_EQ(digest.len(), SHA512::HashBytes);
    EXPECT_EQ(bytes_to_hex(digest),
              "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
              "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    EXPECT_EQ(sha512_hex(bytes_of("abc")),
              "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
              "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

TEST(Sha384BytesTest, BytesAndHex)
{
    const auto digest = sha384(bytes_of("abc"));
    EXPECT_EQ(digest.len(), SHA384::HashBytes);
    EXPECT_EQ(bytes_to_hex(digest),
              "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
              "8086072ba1e7cc2358baeca134c825a7");
    EXPECT_EQ(sha384_hex(bytes_of("abc")),
              "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
              "8086072ba1e7cc2358baeca134c825a7");
}

// UNSUPPORTED_ALGORITHM：为 OpenSSL/CNG 后端预留的错误分支。
TEST(CryptoErrorTest, UnsupportedAlgorithmToString)
{
    EXPECT_STREQ(to_string(CryptoError::UNSUPPORTED_ALGORITHM), "unsupported algorithm");
}
