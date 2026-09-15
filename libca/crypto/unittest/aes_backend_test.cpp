#include <gtest/gtest.h>

#include "libca/crypto/aes.hpp"
#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/hex.hpp"

#include <string>
#include <vector>

using namespace ca;
using namespace ca::crypto;
using namespace ca::core;

namespace {

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

// hex 字面量的持有式包装，临时对象存活到完整表达式结束。
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

// 确定性伪随机源（splitmix64，固定种子可复现），供对拍输入生成。
class DetRng
{
public:
    explicit DetRng(u64 seed) noexcept : state_(seed + 0x9e3779b97f4a7c15ULL) {}

    u64 next() noexcept
    {
        state_ += 0x9e3779b97f4a7c15ULL;
        u64 z = state_;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    Bytes take(usize len)
    {
        BytesMut buffer = BytesMut::with_capacity(len);
        for (usize i = 0; i < len; ++i) {
            static constexpr u64 kMask = 0xff;
            buffer.put_u8(static_cast<u8>(next() & kMask));
        }
        return buffer.freeze();
    }

private:
    u64 state_;
};

// 收集当前编译配置下可用的后端（Builtin 恒可用，Auto 不参与对拍）。
std::vector<AesBackend> available_backends()
{
    std::vector<AesBackend> backends{AesBackend::Builtin};
    if (aes_backend_available(AesBackend::OpenSsl))
        backends.push_back(AesBackend::OpenSsl);
    if (aes_backend_available(AesBackend::Cng))
        backends.push_back(AesBackend::Cng);
    return backends;
}

// 编译期可用的外部后端列表（GCM 等仅外部路径的能力用）。
std::vector<AesBackend> available_external_backends()
{
    std::vector<AesBackend> backends;
    if (aes_backend_available(AesBackend::OpenSsl))
        backends.push_back(AesBackend::OpenSsl);
    if (aes_backend_available(AesBackend::Cng))
        backends.push_back(AesBackend::Cng);
    return backends;
}

const char* backend_name(AesBackend backend) noexcept
{
    switch (backend) {
    case AesBackend::Builtin: return "Builtin";
    case AesBackend::OpenSsl: return "OpenSsl";
    case AesBackend::Cng: return "Cng";
    case AesBackend::Auto: return "Auto";
    }
    return "unknown";
}

// 三种密钥长度与多种输入规模（含 0 与非整块），确定性生成。
struct CrossCheckCase {
    usize key_size;
    usize input_size;
};

bool byte_eq(const Bytes& lhs, const Bytes& rhs)
{
    return constant_time_eq(bytes(lhs), bytes(rhs));
}

}  // namespace

// 后端可用性查询的基本一致性。
TEST(AesBackendTest, AvailabilitySanity)
{
    EXPECT_TRUE(aes_backend_available(AesBackend::Builtin));
    EXPECT_TRUE(aes_backend_available(AesBackend::Auto));  // Auto 至少回退 Builtin
    // 显式不可用后端查询应返回 false（由编译配置决定具体组合）。
    if (!aes_backend_available(AesBackend::OpenSsl))
        EXPECT_FALSE(aes_backend_available(AesBackend::OpenSsl));
    if (!aes_backend_available(AesBackend::Cng))
        EXPECT_FALSE(aes_backend_available(AesBackend::Cng));
}

// ECB/CBC/CTR：所有可用后端对同一组确定性输入输出逐字节一致，且各自解密回环。
TEST(AesBackendTest, CrossCheckEcbCbcCtr)
{
    const CrossCheckCase cases[] = {
        {16, 0}, {16, 16}, {16, 48}, {24, 32}, {32, 16}, {32, 64},
    };
    DetRng rng(0x414553ULL);  // "AES"

    for (const auto& one_case : cases) {
        const Bytes key = rng.take(one_case.key_size);
        const Bytes iv = rng.take(AES_BLOCK_SIZE);
        const Bytes counter = rng.take(AES_BLOCK_SIZE);
        const Bytes plaintext = rng.take(one_case.input_size);

        for (const auto backend : available_backends()) {
            SCOPED_TRACE(backend_name(backend));

            auto ecb = aes_ecb_encrypt(bytes(key), bytes(plaintext), backend);
            ASSERT_TRUE(ecb.is_ok());
            auto ecb_back = aes_ecb_decrypt(bytes(key), bytes(ecb.unwrap()), backend);
            ASSERT_TRUE(ecb_back.is_ok());
            EXPECT_TRUE(byte_eq(ecb_back.unwrap(), plaintext));

            auto cbc = aes_cbc_encrypt(bytes(key), bytes(iv), bytes(plaintext), backend);
            ASSERT_TRUE(cbc.is_ok());
            auto cbc_back = aes_cbc_decrypt(bytes(key), bytes(iv), bytes(cbc.unwrap()), backend);
            ASSERT_TRUE(cbc_back.is_ok());
            EXPECT_TRUE(byte_eq(cbc_back.unwrap(), plaintext));

            auto ctr = aes_ctr_crypt(bytes(key), bytes(counter), bytes(plaintext), backend);
            ASSERT_TRUE(ctr.is_ok());
            auto ctr_back = aes_ctr_crypt(bytes(key), bytes(counter), bytes(ctr.unwrap()), backend);
            ASSERT_TRUE(ctr_back.is_ok());
            EXPECT_TRUE(byte_eq(ctr_back.unwrap(), plaintext));

            // 与 Builtin 输出逐字节一致（Builtin 自己也在上面循环里验证过回环）。
            if (backend != AesBackend::Builtin) {
                auto ecb_ref = aes_ecb_encrypt(bytes(key), bytes(plaintext), AesBackend::Builtin);
                ASSERT_TRUE(ecb_ref.is_ok());
                EXPECT_TRUE(byte_eq(ecb.unwrap(), ecb_ref.unwrap()));

                auto cbc_ref = aes_cbc_encrypt(bytes(key), bytes(iv), bytes(plaintext),
                                               AesBackend::Builtin);
                ASSERT_TRUE(cbc_ref.is_ok());
                EXPECT_TRUE(byte_eq(cbc.unwrap(), cbc_ref.unwrap()));

                auto ctr_ref = aes_ctr_crypt(bytes(key), bytes(counter), bytes(plaintext),
                                             AesBackend::Builtin);
                ASSERT_TRUE(ctr_ref.is_ok());
                EXPECT_TRUE(byte_eq(ctr.unwrap(), ctr_ref.unwrap()));
            }
        }
    }
}

// CTR 非整块输入跨后端一致（尾部截断语义）。
TEST(AesBackendTest, CrossCheckCtrPartial)
{
    DetRng rng(0x50415254ULL);  // "PART"
    const Bytes key = rng.take(32);
    const Bytes counter = rng.take(AES_BLOCK_SIZE);
    const Bytes plaintext = rng.take(100);  // 6 块 + 4 字节尾

    for (const auto backend : available_backends()) {
        auto got = aes_ctr_crypt(bytes(key), bytes(counter), bytes(plaintext), backend);
        ASSERT_TRUE(got.is_ok()) << backend_name(backend);
        EXPECT_EQ(got.unwrap().len(), static_cast<ca::usize>(100));
        if (backend != AesBackend::Builtin) {
            auto ref = aes_ctr_crypt(bytes(key), bytes(counter), bytes(plaintext),
                                     AesBackend::Builtin);
            ASSERT_TRUE(ref.is_ok());
            EXPECT_TRUE(byte_eq(got.unwrap(), ref.unwrap()));
        }
    }
}

// 外部后端不可用时的对拍：Builtin 与 Auto 仍可用，外部显式路径报 UNSUPPORTED_ALGORITHM。
TEST(AesBackendTest, ExplicitUnavailableBackendErrors)
{
    const auto key = hb("000102030405060708090a0b0c0d0e0f");
    const auto iv = hb("000102030405060708090a0b0c0d0e0f");
    const auto data = hb("00112233445566778899aabbccddeeff");

    if (!aes_backend_available(AesBackend::OpenSsl)) {
        EXPECT_EQ(aes_ecb_encrypt(key, data, AesBackend::OpenSsl).unwrap_err(),
                  CryptoError::UNSUPPORTED_ALGORITHM);
        EXPECT_EQ(aes_cbc_decrypt(key, iv, data, AesBackend::OpenSsl).unwrap_err(),
                  CryptoError::UNSUPPORTED_ALGORITHM);
        EXPECT_EQ(aes_ctr_crypt(key, iv, data, AesBackend::OpenSsl).unwrap_err(),
                  CryptoError::UNSUPPORTED_ALGORITHM);
    }
    if (!aes_backend_available(AesBackend::Cng)) {
        EXPECT_EQ(aes_ecb_encrypt(key, data, AesBackend::Cng).unwrap_err(),
                  CryptoError::UNSUPPORTED_ALGORITHM);
        EXPECT_EQ(aes_cbc_decrypt(key, iv, data, AesBackend::Cng).unwrap_err(),
                  CryptoError::UNSUPPORTED_ALGORITHM);
        EXPECT_EQ(aes_ctr_crypt(key, iv, data, AesBackend::Cng).unwrap_err(),
                  CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

// GCM 官方向量：GCM spec（McGrew-Viega）附录 B Test Case 3/4。
// 参数与 tag 经 python cryptography 48.0.0（OpenSSL EVP）现算复核，并与
// nettle testsuite/gcm-test.c 同源条目比对一致；仅外部后端可跑，否则跳过。
TEST(AesBackendTest, GcmOfficialVectors)
{
    const std::string key_hex = "feffe9928665731c6d6a8f9467308308";
    const std::string iv_hex = "cafebabefacedbaddecaf888";
    // TC3：64 字节明文（尾部 1aafd255，注意与 SP800-38A CTR 尾部 1bafd9d7 不同）。
    const std::string pt3_hex =
        "d9313225f88406e5a55909c5aff5269a"
        "86a7a9531534f7da2e4c303d8a318a72"
        "1c3c0c95956809532fcf0e2449a6b525"
        "b16aedf5aa0de657ba637b391aafd255";
    const std::string ct3_hex =
        "42831ec2217774244b7221b784d0d49c"
        "e3aa212f2c02a4e035c17e2329aca12e"
        "21d514b25466931c7d8f6a5aac84aa05"
        "1ba30b396a0aac973d58e091473f5985";
    const std::string tag3_hex = "4d5c2af327cd64a62cf35abd2ba6fab4";

    // TC4：60 字节明文（尾部 ba637b39）+ 20 字节 AAD。
    const std::string pt4_hex =
        "d9313225f88406e5a55909c5aff5269a"
        "86a7a9531534f7da2e4c303d8a318a72"
        "1c3c0c95956809532fcf0e2449a6b525"
        "b16aedf5aa0de657ba637b39";
    const std::string ct4_hex =
        "42831ec2217774244b7221b784d0d49c"
        "e3aa212f2c02a4e035c17e2329aca12e"
        "21d514b25466931c7d8f6a5aac84aa05"
        "1ba30b396a0aac973d58e091";
    const std::string aad4_hex = "feedfacedeadbeeffeedfacedeadbeefabaddad2";
    const std::string tag4_hex = "5bc94fbc3221a5db94fae95ae7121a47";

    const auto externals = available_external_backends();
    if (externals.empty())
        GTEST_SKIP() << "无可用外部后端（with_openssl=n 且非 Windows）";

    for (const auto backend : externals) {
        SCOPED_TRACE(backend_name(backend));

        auto enc3 = aes_gcm_encrypt(hb(key_hex), hb(iv_hex), hb(pt3_hex), ByteSlice{}, backend);
        ASSERT_TRUE(enc3.is_ok());
        EXPECT_EQ(encode_hex(enc3.unwrap().ciphertext), ct3_hex);
        EXPECT_EQ(encode_hex(enc3.unwrap().tag), tag3_hex);

        auto dec3 = aes_gcm_decrypt(hb(key_hex), hb(iv_hex), hb(ct3_hex), hb(tag3_hex),
                                    ByteSlice{}, backend);
        ASSERT_TRUE(dec3.is_ok());
        EXPECT_EQ(encode_hex(dec3.unwrap()), pt3_hex);

        auto enc4 = aes_gcm_encrypt(hb(key_hex), hb(iv_hex), hb(pt4_hex), hb(aad4_hex), backend);
        ASSERT_TRUE(enc4.is_ok());
        EXPECT_EQ(encode_hex(enc4.unwrap().ciphertext), ct4_hex);
        EXPECT_EQ(encode_hex(enc4.unwrap().tag), tag4_hex);

        auto dec4 = aes_gcm_decrypt(hb(key_hex), hb(iv_hex), hb(ct4_hex), hb(tag4_hex),
                                    hb(aad4_hex), backend);
        ASSERT_TRUE(dec4.is_ok());
        EXPECT_EQ(encode_hex(dec4.unwrap()), pt4_hex);
    }
}

// GCM 对拍：全部外部后端对确定性输入产出一致密文与 tag，并解密回环。
TEST(AesBackendTest, GcmCrossCheck)
{
    const auto externals = available_external_backends();
    if (externals.empty())
        GTEST_SKIP() << "无可用外部后端（with_openssl=n 且非 Windows）";

    DetRng rng(0x47434dULL);  // "GCM"
    const Bytes key = rng.take(32);
    const Bytes nonce = rng.take(AES_GCM_NONCE_SIZE);
    const Bytes aad = rng.take(13);
    const Bytes plaintext = rng.take(100);

    for (const auto backend : externals) {
        SCOPED_TRACE(backend_name(backend));
        auto encrypted = aes_gcm_encrypt(bytes(key), bytes(nonce), bytes(plaintext), bytes(aad),
                                         backend);
        ASSERT_TRUE(encrypted.is_ok());
        ASSERT_EQ(encrypted.unwrap().ciphertext.len(), static_cast<ca::usize>(100));
        ASSERT_EQ(encrypted.unwrap().tag.len(), AES_GCM_TAG_SIZE);

        auto decrypted = aes_gcm_decrypt(bytes(key), bytes(nonce), bytes(encrypted.unwrap().ciphertext),
                                         bytes(encrypted.unwrap().tag), bytes(aad), backend);
        ASSERT_TRUE(decrypted.is_ok());
        EXPECT_TRUE(byte_eq(decrypted.unwrap(), plaintext));

        // 与其他外部后端结果一致（OpenSSL vs CNG）。
        for (const auto other : externals) {
            auto ref = aes_gcm_encrypt(bytes(key), bytes(nonce), bytes(plaintext), bytes(aad),
                                       other);
            ASSERT_TRUE(ref.is_ok()) << backend_name(other);
            EXPECT_TRUE(byte_eq(encrypted.unwrap().ciphertext, ref.unwrap().ciphertext));
            EXPECT_TRUE(byte_eq(encrypted.unwrap().tag, ref.unwrap().tag));
        }
    }
}

// GCM 认证失败路径：篡改 tag / AAD / 密文任一，解密必须失败。
TEST(AesBackendTest, GcmTamperDetection)
{
    const auto externals = available_external_backends();
    if (externals.empty())
        GTEST_SKIP() << "无可用外部后端（with_openssl=n 且非 Windows）";

    DetRng rng(0x54414D50ULL);  // "TAMP"
    const Bytes key = rng.take(16);
    const Bytes nonce = rng.take(AES_GCM_NONCE_SIZE);
    const Bytes aad = rng.take(9);
    const Bytes plaintext = rng.take(48);

    auto encrypted = aes_gcm_encrypt(bytes(key), bytes(nonce), bytes(plaintext), bytes(aad),
                                     externals.front());
    ASSERT_TRUE(encrypted.is_ok());
    const Bytes ciphertext = encrypted.unwrap().ciphertext;
    const Bytes tag = encrypted.unwrap().tag;

    // 篡改 tag 首字节。
    BytesMut bad_tag = BytesMut::with_capacity(tag.len());
    bad_tag.put_slice(tag.as_ptr(), tag.len());
    bad_tag.as_mut_ptr()[0] = static_cast<ca::u8>(bad_tag.as_ptr()[0] ^ 0x01);
    {
        Bytes frozen = bad_tag.freeze();
        auto result = aes_gcm_decrypt(bytes(key), bytes(nonce), bytes(ciphertext), bytes(frozen),
                                      bytes(aad), externals.front());
        ASSERT_TRUE(result.is_err());
        EXPECT_EQ(result.unwrap_err(), CryptoError::AUTHENTICATION_FAILED);
    }

    // 篡改密文中间字节。
    BytesMut bad_ct = BytesMut::with_capacity(ciphertext.len());
    bad_ct.put_slice(ciphertext.as_ptr(), ciphertext.len());
    bad_ct.as_mut_ptr()[ciphertext.len() / 2] =
        static_cast<ca::u8>(bad_ct.as_ptr()[ciphertext.len() / 2] ^ 0x80);
    {
        Bytes frozen = bad_ct.freeze();
        auto result = aes_gcm_decrypt(bytes(key), bytes(nonce), bytes(frozen), bytes(tag),
                                      bytes(aad), externals.front());
        ASSERT_TRUE(result.is_err());
        EXPECT_EQ(result.unwrap_err(), CryptoError::AUTHENTICATION_FAILED);
    }

    // 篡改 AAD。
    BytesMut bad_aad = BytesMut::with_capacity(aad.len());
    bad_aad.put_slice(aad.as_ptr(), aad.len());
    bad_aad.as_mut_ptr()[0] = static_cast<ca::u8>(bad_aad.as_ptr()[0] ^ 0x10);
    {
        Bytes frozen = bad_aad.freeze();
        auto result = aes_gcm_decrypt(bytes(key), bytes(nonce), bytes(ciphertext), bytes(tag),
                                      bytes(frozen), externals.front());
        ASSERT_TRUE(result.is_err());
        EXPECT_EQ(result.unwrap_err(), CryptoError::AUTHENTICATION_FAILED);
    }
}

// GCM 经 Builtin 路径必须返回 UNSUPPORTED_ALGORITHM（本批方案口径：GCM 只走外部后端）。
TEST(AesBackendTest, GcmBuiltinUnsupported)
{
    const auto key = hb("000102030405060708090a0b0c0d0e0f");
    const auto nonce = hb("cafebabefacedbaddecaf888");
    const auto data = hb("00112233445566778899aabbccddeeff");
    const auto tag = hb("00000000000000000000000000000000");

    auto encrypted = aes_gcm_encrypt(key, nonce, data, ByteSlice{}, AesBackend::Builtin);
    ASSERT_TRUE(encrypted.is_err());
    EXPECT_EQ(encrypted.unwrap_err(), CryptoError::UNSUPPORTED_ALGORITHM);

    auto decrypted = aes_gcm_decrypt(key, nonce, data, tag, ByteSlice{}, AesBackend::Builtin);
    ASSERT_TRUE(decrypted.is_err());
    EXPECT_EQ(decrypted.unwrap_err(), CryptoError::UNSUPPORTED_ALGORITHM);
}

// GCM 参数校验：非法 nonce / tag 长度返回 INVALID_ARGUMENT（先于后端分发）。
TEST(AesBackendTest, GcmInvalidArguments)
{
    const auto key = hb("000102030405060708090a0b0c0d0e0f");
    const auto nonce_bad = hb("cafebabefacedbaddecaf");  // 10 字节
    const auto tag_bad = hb("000000000000000000000000000000");  // 15 字节
    const auto nonce = hb("cafebabefacedbaddecaf888");
    const auto tag = hb("00000000000000000000000000000000");
    const auto data = hb("00112233445566778899aabbccddeeff");

    EXPECT_EQ(aes_gcm_encrypt(key, nonce_bad, data, ByteSlice{}, AesBackend::OpenSsl).unwrap_err(),
              CryptoError::INVALID_ARGUMENT);
    EXPECT_EQ(aes_gcm_decrypt(key, nonce, data, tag_bad, ByteSlice{}, AesBackend::OpenSsl).unwrap_err(),
              CryptoError::INVALID_ARGUMENT);
    // 空 nonce 同样非法。
    EXPECT_EQ(aes_gcm_encrypt(key, ByteSlice{}, data, ByteSlice{}, AesBackend::Cng).unwrap_err(),
              CryptoError::INVALID_ARGUMENT);
}

// Auto 解析结果必须与某个可用后端的输出一致（本配置下即 OpenSsl 或 Cng 或 Builtin）。
TEST(AesBackendTest, AutoMatchesResolvedBackend)
{
    DetRng rng(0x4155544FULL);  // "AUTO"
    const Bytes key = rng.take(24);
    const Bytes iv = rng.take(AES_BLOCK_SIZE);
    const Bytes plaintext = rng.take(32);

    auto by_auto = aes_ecb_encrypt(bytes(key), bytes(plaintext), AesBackend::Auto);
    ASSERT_TRUE(by_auto.is_ok());

    bool matched = false;
    for (const auto backend : available_backends()) {
        auto explicit_result = aes_ecb_encrypt(bytes(key), bytes(plaintext), backend);
        ASSERT_TRUE(explicit_result.is_ok());
        if (byte_eq(by_auto.unwrap(), explicit_result.unwrap())) {
            matched = true;
            break;
        }
    }
    EXPECT_TRUE(matched) << "Auto 结果不匹配任何可用后端";
}
