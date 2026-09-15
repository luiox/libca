#include <gtest/gtest.h>

#include "libca/crypto/aes.hpp"
#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/hex.hpp"

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

std::string encode_hex(const Bytes& data)
{
    return hex_encode(bytes(data));
}

Bytes decode_hex(const std::string& text)
{
    auto decoded = hex_decode(text);
    if (decoded.is_err())
        return {};
    return decoded.unwrap();
}

// hex 解码结果的持有式包装：临时对象存活到完整表达式结束，
// 经隐式转换把 ByteSlice 安全传给被测函数（避免悬垂视图）。
struct HexBytes {
    Bytes data;

    operator ByteSlice() const
    {
        return ByteSlice(data.as_ptr(), data.len());
    }
};

HexBytes hb(const std::string& text)
{
    return HexBytes{decode_hex(text)};
}

struct AesModeVector {
    const char* key;
    const char* input;   // ECB/CBC/CTR 的明文输入
    const char* output;  // 期望密文（hex）
    const char* iv;      // CBC 的 IV / CTR 的初始计数值；ECB 为空串
};

// NIST SP800-38A Appendix F 四块原文与官方向量（密文经 python cryptography
// 与 openssl 命令行双工具交叉验证，与文档原文一致）。
const char* SP800_PLAINTEXT =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

const AesModeVector ECB_VECTORS[] = {
    // F.1.1 ECB-AES128.Encrypt
    {"2b7e151628aed2a6abf7158809cf4f3c", SP800_PLAINTEXT,
     "3ad77bb40d7a3660a89ecaf32466ef97"
     "f5d3d58503b9699de785895a96fdbaaf"
     "43b1cd7f598ece23881b00e3ed030688"
     "7b0c785e27e8ad3f8223207104725dd4", ""},
    // F.1.3 ECB-AES192.Encrypt
    {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", SP800_PLAINTEXT,
     "bd334f1d6e45f25ff712a214571fa5cc"
     "974104846d0ad3ad7734ecb3ecee4eef"
     "ef7afd2270e2e60adce0ba2face6444e"
     "9a4b41ba738d6c72fb16691603c18e0e", ""},
    // F.1.5 ECB-AES256.Encrypt
    {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", SP800_PLAINTEXT,
     "f3eed1bdb5d2a03c064b5a7e3db181f8"
     "591ccb10d410ed26dc5ba74a31362870"
     "b6ed21b99ca6f4f9f153e7b1beafed1d"
     "23304b7a39f9f3ff067d8d8f9e24ecc7", ""},
};

const AesModeVector CBC_VECTORS[] = {
    // F.2.1 CBC-AES128.Encrypt，IV = 000102030405060708090a0b0c0d0e0f
    {"2b7e151628aed2a6abf7158809cf4f3c", SP800_PLAINTEXT,
     "7649abac8119b246cee98e9b12e9197d"
     "5086cb9b507219ee95db113a917678b2"
     "73bed6b8e3c1743b7116e69e22229516"
     "3ff1caa1681fac09120eca307586e1a7", "000102030405060708090a0b0c0d0e0f"},
    // F.2.3 CBC-AES192.Encrypt
    {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", SP800_PLAINTEXT,
     "4f021db243bc633d7178183a9fa071e8"
     "b4d9ada9ad7dedf4e5e738763f69145a"
     "571b242012fb7ae07fa9baac3df102e0"
     "08b0e27988598881d920a9e64f5615cd", "000102030405060708090a0b0c0d0e0f"},
    // F.2.5 CBC-AES256.Encrypt
    {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", SP800_PLAINTEXT,
     "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
     "9cfc4e967edb808d679f777bc6702c7d"
     "39f23369a9d9bacfa530e26304231461"
     "b2eb05e2c39be9fcda6c19078c6a9d1b", "000102030405060708090a0b0c0d0e0f"},
};

const AesModeVector CTR_VECTORS[] = {
    // F.5.1 CTR-AES128.Encrypt，初始计数值 f0f1..ff
    {"2b7e151628aed2a6abf7158809cf4f3c", SP800_PLAINTEXT,
     "874d6191b620e3261bef6864990db6ce"
     "9806f66b7970fdff8617187bb9fffdff"
     "5ae4df3edbd5d35e5b4f09020db03eab"
     "1e031dda2fbe03d1792170a0f3009cee", "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"},
    // F.5.3 CTR-AES192.Encrypt
    {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", SP800_PLAINTEXT,
     "1abc932417521ca24f2b0459fe7e6e0b"
     "090339ec0aa6faefd5ccc2c6f4ce8e94"
     "1e36b26bd1ebc670d1bd1d665620abf7"
     "4f78a7f6d29809585a97daec58c6b050", "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"},
    // F.5.5 CTR-AES256.Encrypt
    {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", SP800_PLAINTEXT,
     "601ec313775789a5b7a7f504bbf3d228"
     "f443e3ca4d62b59aca84e990cacaf5c5"
     "2b0930daa23de94ce87017ba2d84988d"
     "dfc9c58db67aada613c2dd08457941a6", "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"},
};

}  // namespace

// FIPS-197 附录 C 单块 ECB 向量（三种密钥长度各一）。
TEST(AesTest, Fips197AppendixC)
{
    struct FipsVector {
        const char* key;
        const char* expected;
    };
    const FipsVector vectors[] = {
        // C.1 AES-128
        {"000102030405060708090a0b0c0d0e0f", "69c4e0d86a7b0430d8cdb78070b4c55a"},
        // C.2 AES-192
        {"000102030405060708090a0b0c0d0e0f1011121314151617", "dda97ca4864cdfe06eaf70a0ec0d7191"},
        // C.3 AES-256
        {"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
         "8ea2b7ca516745bfeafc49904b496089"},
    };
    const std::string plaintext = "00112233445566778899aabbccddeeff";

    for (const auto& vector : vectors) {
        auto encrypted = aes_ecb_encrypt(hb(vector.key), hb(plaintext));
        ASSERT_TRUE(encrypted.is_ok());
        EXPECT_EQ(encode_hex(encrypted.unwrap()), vector.expected);

        auto decrypted = aes_ecb_decrypt(hb(vector.key), hb(vector.expected));
        ASSERT_TRUE(decrypted.is_ok());
        EXPECT_EQ(encode_hex(decrypted.unwrap()), plaintext);
    }
}

// NIST SP800-38A F.1 ECB 官方向量（三种密钥长度，四块数据）。
TEST(AesTest, Sp800EcbVectors)
{
    for (const auto& vector : ECB_VECTORS) {
        auto encrypted = aes_ecb_encrypt(hb(vector.key), hb(vector.input));
        ASSERT_TRUE(encrypted.is_ok());
        EXPECT_EQ(encode_hex(encrypted.unwrap()), vector.output);

        auto decrypted = aes_ecb_decrypt(hb(vector.key), hb(vector.output));
        ASSERT_TRUE(decrypted.is_ok());
        EXPECT_EQ(encode_hex(decrypted.unwrap()), vector.input);
    }
}

// NIST SP800-38A F.2 CBC 官方向量（三种密钥长度）。
TEST(AesTest, Sp800CbcVectors)
{
    for (const auto& vector : CBC_VECTORS) {
        auto encrypted = aes_cbc_encrypt(hb(vector.key), hb(vector.iv), hb(vector.input));
        ASSERT_TRUE(encrypted.is_ok());
        EXPECT_EQ(encode_hex(encrypted.unwrap()), vector.output);

        auto decrypted = aes_cbc_decrypt(hb(vector.key), hb(vector.iv), hb(vector.output));
        ASSERT_TRUE(decrypted.is_ok());
        EXPECT_EQ(encode_hex(decrypted.unwrap()), vector.input);
    }
}

// NIST SP800-38A F.5 CTR 官方向量（三种密钥长度，加解密同函数）。
TEST(AesTest, Sp800CtrVectors)
{
    for (const auto& vector : CTR_VECTORS) {
        auto encrypted = aes_ctr_crypt(hb(vector.key), hb(vector.iv), hb(vector.input));
        ASSERT_TRUE(encrypted.is_ok());
        EXPECT_EQ(encode_hex(encrypted.unwrap()), vector.output);

        auto decrypted = aes_ctr_crypt(hb(vector.key), hb(vector.iv), hb(vector.output));
        ASSERT_TRUE(decrypted.is_ok());
        EXPECT_EQ(encode_hex(decrypted.unwrap()), vector.input);
    }
}

// CTR 非整块输入：末尾部分块只 XOR 对应长度的 keystream 前缀。
TEST(AesTest, CtrPartialBlock)
{
    const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
    const std::string ctr_hex = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

    // 短输入密文与长输入密文前缀逐字节一致（同一计数块 keystream 前缀）。
    auto full = aes_ctr_crypt(hb(key_hex), hb(ctr_hex),
                              hb("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"));
    ASSERT_TRUE(full.is_ok());

    auto partial = aes_ctr_crypt(hb(key_hex), hb(ctr_hex),
                                 hb("6bc1bee22e409f96e93d7e117393172a"));
    ASSERT_TRUE(partial.is_ok());
    EXPECT_EQ(encode_hex(partial.unwrap()), encode_hex(full.unwrap()).substr(0, 32));

    // 1 字节输入也应成功。
    auto one = aes_ctr_crypt(hb(key_hex), hb(ctr_hex), hb("6b"));
    ASSERT_TRUE(one.is_ok());
    EXPECT_EQ(one.unwrap().len(), static_cast<ca::usize>(1));
}

// 错误路径：非法密钥长度、非整块 ECB/CBC 输入、错误 IV 长度。
TEST(AesTest, RejectsInvalidArguments)
{
    const std::string key128 = "2b7e151628aed2a6abf7158809cf4f3c";
    const std::string key_bad = "2b7e151628aed2a6abf7158809cf4f";  // 15 字节
    const std::string iv16 = "000102030405060708090a0b0c0d0e0f";
    const std::string block = "6bc1bee22e409f96e93d7e117393172a";
    const std::string not_block = "6bc1bee22e409f96e93d7e11739317";  // 15 字节
    const std::string iv15 = "000102030405060708090a0b0c0d0e";

    // 非法密钥长度
    EXPECT_TRUE(aes_ecb_encrypt(hb(key_bad), hb(block)).is_err());
    EXPECT_TRUE(aes_ecb_decrypt(hb(key_bad), hb(block)).is_err());
    EXPECT_TRUE(aes_cbc_encrypt(hb(key_bad), hb(iv16), hb(block)).is_err());
    EXPECT_TRUE(aes_cbc_decrypt(hb(key_bad), hb(iv16), hb(block)).is_err());
    EXPECT_TRUE(aes_ctr_crypt(hb(key_bad), hb(iv16), hb(block)).is_err());

    // 非整块 ECB/CBC 输入
    EXPECT_TRUE(aes_ecb_encrypt(hb(key128), hb(not_block)).is_err());
    EXPECT_TRUE(aes_ecb_decrypt(hb(key128), hb(not_block)).is_err());
    EXPECT_TRUE(aes_cbc_encrypt(hb(key128), hb(iv16), hb(not_block)).is_err());
    EXPECT_TRUE(aes_cbc_decrypt(hb(key128), hb(iv16), hb(not_block)).is_err());

    // 错误 IV 长度
    EXPECT_TRUE(aes_cbc_encrypt(hb(key128), hb(iv15), hb(block)).is_err());
    EXPECT_TRUE(aes_cbc_decrypt(hb(key128), hb(iv15), hb(block)).is_err());
    // CTR 计数块长度错误
    EXPECT_TRUE(aes_ctr_crypt(hb(key128), hb("f0f1f2f3"), hb(block)).is_err());

    // 空 ECB/CBC 输入合法（0 为 16 的整倍数）；空密钥非法。
    const ByteSlice empty_view(nullptr, 0);
    auto empty = aes_ecb_encrypt(hb(key128), empty_view);
    ASSERT_TRUE(empty.is_ok());
    EXPECT_EQ(empty.unwrap().len(), static_cast<ca::usize>(0));
    EXPECT_TRUE(aes_ecb_encrypt(empty_view, hb(block)).is_err());
}

// CBC 加解密回环：密文解密还原原文（反馈语义 sanity check）。
TEST(AesTest, CbcRoundtrip)
{
    const auto key = decode_hex("2b7e151628aed2a6abf7158809cf4f3c");
    const auto iv = decode_hex("000102030405060708090a0b0c0d0e0f");
    const auto plaintext = decode_hex(SP800_PLAINTEXT);

    auto encrypted = aes_cbc_encrypt(bytes(key), bytes(iv), bytes(plaintext));
    ASSERT_TRUE(encrypted.is_ok());

    auto decrypted = aes_cbc_decrypt(bytes(key), bytes(iv), bytes(encrypted.unwrap()));
    ASSERT_TRUE(decrypted.is_ok());
    EXPECT_TRUE(constant_time_eq(bytes(plaintext), bytes(decrypted.unwrap())));
}
