// DES 单测：FIPS 46-3 KAT / JCE 黄金组（SunJCE 实测，跨实现对齐的硬要求）/
// CBC 链接性与 IV 依赖 / PKCS#5 正常与非法填充 / 回环属性 / 弱密钥不校验语义 /
// 错误路径。
//
// 黄金组来源：luiox/morpher mjt-deobf/test/des_eval_test.cpp（JCE harness
// work/zkm_universal/td0_harness/Td0Vectors.java，JDK17 SunJCE 实测产出；
// KAT 向量与 FIPS 46-3 附录 B 交叉一致）。morpher 侧 DES 双实现下沉本模块后，
// 这些数字即双向兼容锚点。
#include <gtest/gtest.h>

#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/des.hpp"
#include "libca/crypto/hex.hpp"

#include <string>

using namespace ca;
using namespace ca::crypto;
using namespace ca::core;

namespace {

// 十六进制字面量 → 字节（测试向量统一大写书写，与 JCE 黄金组文本一致）。
Bytes from_hex(const std::string& hex_text)
{
    auto decoded = hex_decode(hex_text);
    if (decoded.is_err())
        ADD_FAILURE() << "bad hex literal: " << hex_text;
    return decoded.is_ok() ? decoded.unwrap() : Bytes{};
}

std::string to_hex_upper(ByteSlice data)
{
    std::string out = hex_encode(data);
    for (char& c : out) {
        if (c >= 'a' && c <= 'f')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

ByteSlice view(const Bytes& data)
{
    return ByteSlice(data.as_ptr(), data.len());
}

// 确定性 LCG（回环属性测试；避免测试内随机性），与 morpher des_eval_test 同款。
u64 next_lcg(u64& state)
{
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return state ^ (state >> 29);
}

Bytes lcg_bytes(u64& state, usize len)
{
    BytesMut out = BytesMut::with_capacity(len);
    for (usize i = 0; i < len; ++i)
        out.put_u8(static_cast<u8>(next_lcg(state) >> 33));
    return out.freeze();
}

}  // namespace

// ---------------------------------------------------------------------------
// KAT：FIPS 46-3 经典向量
// ---------------------------------------------------------------------------

// 教科书向量 key=133457799BBCDFF1：E(0123456789ABCDEF)=85E813540F0AB405、
// D(0123456789ABCDEF)=EE0F7C12E0B09338（任务验收口径的两个必测数字）。
TEST(DesTest, KatTextbookVector)
{
    const Bytes key = from_hex("133457799BBCDFF1");
    const Bytes plain = from_hex("0123456789ABCDEF");

    auto encrypted = des_encrypt_block(view(key), view(plain));
    ASSERT_TRUE(encrypted.is_ok());
    EXPECT_EQ(to_hex_upper(view(encrypted.unwrap())), "85E813540F0AB405");

    auto decrypted = des_decrypt_block(view(key), view(plain));
    ASSERT_TRUE(decrypted.is_ok());
    EXPECT_EQ(to_hex_upper(view(decrypted.unwrap())), "EE0F7C12E0B09338");

    // 解密侧对称：D(密文) 回到明文。
    auto roundtrip = des_decrypt_block(view(key), view(encrypted.unwrap()));
    ASSERT_TRUE(roundtrip.is_ok());
    EXPECT_EQ(to_hex_upper(view(roundtrip.unwrap())), "0123456789ABCDEF");
}

// 其余 FIPS 46-3 附录 B 与标准例题向量（ECB 单块语义）。
TEST(DesTest, KatFipsAppendixAndClassics)
{
    // {key, plaintext, ciphertext}
    const char* kKats[][3] = {
        {"10316E028C8F3B4A", "0000000000000000", "82DCBAFBDEAB6602"},  // FIPS 46-3 附录 B
        {"0E329232EA6D0D73", "8787878787878787", "0000000000000000"},  // 标准例题（密文 0）
        {"0123456789ABCDEF", "4E6F772069732074", "3FA40E8A984D4815"},  // "Now is t" 经典向量
    };
    for (const auto& kat : kKats) {
        const Bytes key = from_hex(kat[0]);
        const Bytes plain = from_hex(kat[1]);
        const Bytes cipher = from_hex(kat[2]);
        auto encrypted = des_encrypt_block(view(key), view(plain));
        ASSERT_TRUE(encrypted.is_ok());
        EXPECT_EQ(to_hex_upper(view(encrypted.unwrap())), kat[2]) << "encrypt " << kat[0];
        auto decrypted = des_decrypt_block(view(key), view(cipher));
        ASSERT_TRUE(decrypted.is_ok());
        EXPECT_EQ(to_hex_upper(view(decrypted.unwrap())), kat[1]) << "decrypt " << kat[0];
    }
}

// ---------------------------------------------------------------------------
// JCE 黄金组（来源：luiox/morpher mjt-deobf/test/des_eval_test.cpp，SunJCE 实测）
// ---------------------------------------------------------------------------

// 单块解密黄金组：D(133457799BBCDFF1, FEDCBA9876543210)=7D4D8B4E525E14ED、
// D(133457799BBCDFF1, FFFFFFFFFFFFFFFF)=D85B9AE1CCD81834。
TEST(DesTest, JceGoldenSingleBlockDecrypt)
{
    const Bytes key = from_hex("133457799BBCDFF1");
    const Bytes ct1 = from_hex("FEDCBA9876543210");
    const Bytes ct2 = from_hex("FFFFFFFFFFFFFFFF");

    auto plain1 = des_decrypt_block(view(key), view(ct1));
    ASSERT_TRUE(plain1.is_ok());
    EXPECT_EQ(to_hex_upper(view(plain1.unwrap())), "7D4D8B4E525E14ED");

    auto plain2 = des_decrypt_block(view(key), view(ct2));
    ASSERT_TRUE(plain2.is_ok());
    EXPECT_EQ(to_hex_upper(view(plain2.unwrap())), "D85B9AE1CCD81834");

    // 加密侧对偶：E(7D4D8B4E525E14ED)=FEDCBA9876543210。
    auto back = des_encrypt_block(view(key), view(plain1.unwrap()));
    ASSERT_TRUE(back.is_ok());
    EXPECT_EQ(to_hex_upper(view(back.unwrap())), "FEDCBA9876543210");
}

// CBC 零 IV 三块链解密黄金组：85E813540F0AB405 6E6EFED3E1EE0A98 13901C9ACCA1BC75
// → 0123456789ABCDEF 23456789ABCDEF01 456789ABCDEF0123；并独立证明链接语义
// （第二块单独解密须异或前块密文才得明文）。
TEST(DesTest, JceGoldenCbcZeroIvChainedBlocks)
{
    const Bytes key = from_hex("133457799BBCDFF1");
    const Bytes zero_iv = from_hex("0000000000000000");
    const Bytes chain_cipher =
        from_hex("85E813540F0AB4056E6EFED3E1EE0A9813901C9ACCA1BC75");
    const std::string kExpectedPlain = "0123456789ABCDEF23456789ABCDEF01456789ABCDEF0123";

    auto plain = des_cbc_decrypt(view(key), view(zero_iv), view(chain_cipher));
    ASSERT_TRUE(plain.is_ok());
    EXPECT_EQ(to_hex_upper(view(plain.unwrap())), kExpectedPlain);

    // 加密方向还原黄金密文（零 IV 下首块与 ECB 等价，后续块经链接）。
    auto cipher_again = des_cbc_encrypt(view(key), view(zero_iv), view(plain.unwrap()));
    ASSERT_TRUE(cipher_again.is_ok());
    EXPECT_EQ(to_hex_upper(view(cipher_again.unwrap())),
              "85E813540F0AB4056E6EFED3E1EE0A9813901C9ACCA1BC75");

    // 链接性独立证明：第二块单独块解密 = A6AD74DDA4C75B04，异或首块密文得明文块。
    const Bytes second_block = from_hex("6E6EFED3E1EE0A98");
    const Bytes first_cipher = from_hex("85E813540F0AB405");
    const Bytes expected_second = from_hex("23456789ABCDEF01");
    auto d2 = des_decrypt_block(view(key), view(second_block));
    ASSERT_TRUE(d2.is_ok());
    EXPECT_EQ(to_hex_upper(view(d2.unwrap())), "A6AD74DDA4C75B04");
    const ByteSlice d2_view = view(d2.unwrap());
    const ByteSlice first_cipher_view = view(first_cipher);
    const ByteSlice expected_second_view = view(expected_second);
    for (usize i = 0; i < DES_BLOCK_SIZE; ++i) {
        EXPECT_EQ(static_cast<int>(d2_view[i] ^ first_cipher_view[i]),
                  static_cast<int>(expected_second_view[i]));
    }
}

// CBC 非零 IV 黄金组：D(key=133457799BBCDFF1, IV=FEDCBA9876543210, 5A3DB304D64924FD)
// = 0123456789ABCDEF。
TEST(DesTest, JceGoldenCbcNonZeroIv)
{
    const Bytes key = from_hex("133457799BBCDFF1");
    const Bytes iv = from_hex("FEDCBA9876543210");
    const Bytes cipher = from_hex("5A3DB304D64924FD");

    auto plain = des_cbc_decrypt(view(key), view(iv), view(cipher));
    ASSERT_TRUE(plain.is_ok());
    EXPECT_EQ(to_hex_upper(view(plain.unwrap())), "0123456789ABCDEF");

    auto cipher_again = des_cbc_encrypt(view(key), view(iv), view(plain.unwrap()));
    ASSERT_TRUE(cipher_again.is_ok());
    EXPECT_EQ(to_hex_upper(view(cipher_again.unwrap())), "5A3DB304D64924FD");
}

// CBC × PKCS#5 解密黄金组：明文长 0/1/7/8/9/16/23（key 0E329232EA6D0D73，零 IV）。
TEST(DesTest, JceGoldenCbcPkcs5Lengths)
{
    const Bytes key = from_hex("0E329232EA6D0D73");
    const Bytes zero_iv = from_hex("0000000000000000");
    // {密文, 解密 + 去 PKCS#5 后的明文}
    const char* kCases[][2] = {
        {"A913F4CB0BD30F97", ""},
        {"E4BC36D05EE28B92", "48"},
        {"A7382B7F439BF0DE", "48656C6C6F2C20"},
        {"9216FC83BC44F54F66AE1FEFFB23E1D8", "48656C6C6F2C205A"},
        {"9216FC83BC44F54F1828DE611DB933A9", "48656C6C6F2C205A4B"},
        {"9216FC83BC44F54F66069F83224AD8B6009276CDE10E3044",
         "48656C6C6F2C205A4B4D204445532077"},
        {"9216FC83BC44F54F66069F83224AD8B6A8335CF9149578C8",
         "48656C6C6F2C205A4B4D2044455320776F726C64214142"},
    };
    for (const auto& c : kCases) {
        const Bytes cipher = from_hex(c[0]);
        auto padded = des_cbc_decrypt(view(key), view(zero_iv), view(cipher));
        ASSERT_TRUE(padded.is_ok()) << "ct=" << c[0];
        auto plain = pkcs5_unpad(view(padded.unwrap()));
        ASSERT_TRUE(plain.is_ok()) << "ct=" << c[0];
        EXPECT_EQ(to_hex_upper(view(plain.unwrap())), c[1]) << "ct=" << c[0];
    }
}

// PKCS#5 非法填充拒绝（对齐 JCE BadPaddingException 语义）+ 一致填充正常剥离。
TEST(DesTest, Pkcs5BadPaddingRejected)
{
    const Bytes key = from_hex("0E329232EA6D0D73");
    const Bytes zero_iv = from_hex("0000000000000000");
    // {密文, 非法原因}：任意块 / pad 2 但倒数第 3 字节不一致 / pad 值 09 > 8 / pad 值 00
    const char* kBad[] = {
        "8C17D4A3B2F09E61",
        "4820DB3FF896742D",
        "29FDFD2EB6E137C6",
        "5540F53DDAFFCBDF",
    };
    for (const char* ct : kBad) {
        auto padded = des_cbc_decrypt(view(key), view(zero_iv), view(from_hex(ct)));
        ASSERT_TRUE(padded.is_ok()) << "ct=" << ct;
        auto plain = pkcs5_unpad(view(padded.unwrap()));
        EXPECT_TRUE(plain.is_err()) << "ct=" << ct;
    }
    // 一致填充（…02 02 02，pad=2）→ 正常剥离。
    auto padded = des_cbc_decrypt(view(key), view(zero_iv), view(from_hex("CB44EE7E0D900657")));
    ASSERT_TRUE(padded.is_ok());
    auto ok = pkcs5_unpad(view(padded.unwrap()));
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(to_hex_upper(view(ok.unwrap())), "414141414102");
}

// ---------------------------------------------------------------------------
// PKCS#5 助手单元行为
// ---------------------------------------------------------------------------

TEST(DesTest, Pkcs5PadUnpadUnit)
{
    // 空输入 → 补满一块 8×08；1 字节 → 补 7×07；整块输入 → 追加整块 8×08。
    struct PadCase
    {
        const char* in;
        const char* out;
    };
    const PadCase kPadCases[] = {
        {"", "0808080808080808"},
        {"48", "4807070707070707"},
        {"48656C6C6F2C20", "48656C6C6F2C2001"},
        {"48656C6C6F2C205A", "48656C6C6F2C205A0808080808080808"},
    };
    for (const auto& c : kPadCases) {
        auto padded = pkcs5_pad(view(from_hex(c.in)));
        ASSERT_TRUE(padded.is_ok()) << "in=" << c.in;
        EXPECT_EQ(to_hex_upper(view(padded.unwrap())), c.out) << "in=" << c.in;
        // pad → unpad 恒等。
        auto unpadded = pkcs5_unpad(view(padded.unwrap()));
        ASSERT_TRUE(unpadded.is_ok());
        EXPECT_EQ(to_hex_upper(view(unpadded.unwrap())), c.in);
    }

    // unpad 长度非法：空、非 8 倍数。
    EXPECT_TRUE(pkcs5_unpad(view(Bytes{})).is_err());
    EXPECT_TRUE(pkcs5_unpad(view(from_hex("0102030405"))).is_err());
    // unpad 填充值越界：尾 00、尾 09。
    EXPECT_TRUE(pkcs5_unpad(view(from_hex("48656C6C6F2C2000"))).is_err());
    EXPECT_TRUE(pkcs5_unpad(view(from_hex("48656C6C6F2C2009"))).is_err());
    // unpad 填充不一致：尾 03 但倒数第 2 字节非 03。
    EXPECT_TRUE(pkcs5_unpad(view(from_hex("48656C6C6F2C010203"))).is_err());
}

// ---------------------------------------------------------------------------
// 回环与模式属性
// ---------------------------------------------------------------------------

// 确定性伪随机回环：单块 / ECB / CBC(+PKCS#5) 双向 encrypt→decrypt 恒等。
TEST(DesTest, RoundtripProperty)
{
    u64 state = 0x243F6A8885A308D3ULL;
    const Bytes zero_iv = from_hex("0000000000000000");

    for (int i = 0; i < 128; ++i) {
        const Bytes key = lcg_bytes(state, DES_KEY_SIZE);
        const Bytes iv = lcg_bytes(state, DES_BLOCK_SIZE);
        const Bytes single = lcg_bytes(state, DES_BLOCK_SIZE);
        const usize blocks = static_cast<usize>(1 + (next_lcg(state) % 4));
        const Bytes data = lcg_bytes(state, blocks * DES_BLOCK_SIZE);

        // 单块双向。
        auto enc_block = des_encrypt_block(view(key), view(single));
        ASSERT_TRUE(enc_block.is_ok());
        auto dec_block = des_decrypt_block(view(key), view(enc_block.unwrap()));
        ASSERT_TRUE(dec_block.is_ok());
        EXPECT_TRUE(constant_time_eq(view(single), view(dec_block.unwrap())));
        auto dec_first = des_decrypt_block(view(key), view(single));
        ASSERT_TRUE(dec_first.is_ok());
        auto enc_back = des_encrypt_block(view(key), view(dec_first.unwrap()));
        ASSERT_TRUE(enc_back.is_ok());
        EXPECT_TRUE(constant_time_eq(view(single), view(enc_back.unwrap())));

        // ECB 双向。
        auto ecb_enc = des_ecb_encrypt(view(key), view(data));
        ASSERT_TRUE(ecb_enc.is_ok());
        auto ecb_dec = des_ecb_decrypt(view(key), view(ecb_enc.unwrap()));
        ASSERT_TRUE(ecb_dec.is_ok());
        EXPECT_TRUE(constant_time_eq(view(data), view(ecb_dec.unwrap())));

        // CBC 双向。
        auto cbc_enc = des_cbc_encrypt(view(key), view(iv), view(data));
        ASSERT_TRUE(cbc_enc.is_ok());
        auto cbc_dec = des_cbc_decrypt(view(key), view(iv), view(cbc_enc.unwrap()));
        ASSERT_TRUE(cbc_dec.is_ok());
        EXPECT_TRUE(constant_time_eq(view(data), view(cbc_dec.unwrap())));

        // CBC + PKCS#5：pad → 加密 → 解密 → unpad 恒等（含 0..7 字节余数）。
        const usize remainder = static_cast<usize>(next_lcg(state) % DES_BLOCK_SIZE);
        const Bytes raw = lcg_bytes(state, remainder);
        auto padded = pkcs5_pad(view(raw));
        ASSERT_TRUE(padded.is_ok());
        auto padded_enc = des_cbc_encrypt(view(key), view(zero_iv), view(padded.unwrap()));
        ASSERT_TRUE(padded_enc.is_ok());
        auto padded_dec = des_cbc_decrypt(view(key), view(zero_iv), view(padded_enc.unwrap()));
        ASSERT_TRUE(padded_dec.is_ok());
        auto unpadded = pkcs5_unpad(view(padded_dec.unwrap()));
        ASSERT_TRUE(unpadded.is_ok());
        EXPECT_TRUE(constant_time_eq(view(raw), view(unpadded.unwrap())));
    }
}

// CBC 链接性：IV 变化改变全部输出；首块只依赖 IV（错 IV 解密仅首块不同）；
// 逐块验证 C_i = E(P_i xor C_{i-1})。
TEST(DesTest, CbcChainingProperties)
{
    const Bytes key = from_hex("133457799BBCDFF1");
    const Bytes iv1 = from_hex("FEDCBA9876543210");
    const Bytes iv2 = from_hex("0123456789ABCDEF");
    const Bytes data = from_hex("4E6F77206973207468652074696D6520666F7220616C6C20");

    auto enc1 = des_cbc_encrypt(view(key), view(iv1), view(data));
    auto enc2 = des_cbc_encrypt(view(key), view(iv2), view(data));
    ASSERT_TRUE(enc1.is_ok());
    ASSERT_TRUE(enc2.is_ok());
    // IV 不同 → 密文完全不同。
    EXPECT_FALSE(constant_time_eq(view(enc1.unwrap()), view(enc2.unwrap())));

    // 错 IV 解密：后续块仍正确，仅首块不同。
    auto wrong_iv = des_cbc_decrypt(view(key), view(iv2), view(enc1.unwrap()));
    ASSERT_TRUE(wrong_iv.is_ok());
    EXPECT_FALSE(constant_time_eq(view(wrong_iv.unwrap()), view(data)));
    EXPECT_EQ(0, std::string(to_hex_upper(view(wrong_iv.unwrap())))
                     .compare(16, std::string::npos,
                              to_hex_upper(view(data)).substr(16)));

    // 链式定义逐块核对：C_0 = E(P_0 xor IV)，C_i = E(P_i xor C_{i-1})。
    auto cbc = des_cbc_encrypt(view(key), view(iv1), view(data));
    ASSERT_TRUE(cbc.is_ok());
    Bytes feedback = iv1;
    const ByteSlice data_view = view(data);
    for (usize offset = 0; offset < data.len(); offset += DES_BLOCK_SIZE) {
        BytesMut xored = BytesMut::with_capacity(DES_BLOCK_SIZE);
        const ByteSlice feedback_view = view(feedback);
        for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
            xored.put_u8(static_cast<u8>(data_view[offset + i] ^ feedback_view[i]));
        auto block_cipher = des_encrypt_block(view(key), view(xored.freeze()));
        ASSERT_TRUE(block_cipher.is_ok());
        ByteSlice cipher = view(cbc.unwrap());
        EXPECT_TRUE(constant_time_eq(view(block_cipher.unwrap()),
                                     ByteSlice(cipher.data() + offset, DES_BLOCK_SIZE)))
            << "block at offset " << offset;
        feedback = Bytes::copy_from_slice(cipher.data() + offset, DES_BLOCK_SIZE);
    }

    // ECB 无链接：相同明文块 → 相同密文块（与 CBC 对比的存在意义）。
    const Bytes repeated = from_hex("0123456789ABCDEF0123456789ABCDEF");
    auto ecb = des_ecb_encrypt(view(key), view(repeated));
    ASSERT_TRUE(ecb.is_ok());
    EXPECT_TRUE(constant_time_eq(view(ecb.unwrap()).sub_slice(0, 8),
                                 view(ecb.unwrap()).sub_slice(8, 8)));
}

// 弱密钥/半弱密钥不做校验（JCE DESKeySpec 语义：奇偶位不调整、不拒绝），
// 且 FIPS 46-3 附录 A 定义的数学性质成立。
TEST(DesTest, WeakKeyAcceptedWithFipsProperties)
{
    const Bytes plain = from_hex("0123456789ABCDEF");

    // 全零密钥（奇偶位全 0，JCE 侧合法）：奇偶位经 PC-1 丢弃后与弱密钥
    // 0101010101010101 完全同键，故 E(0) 同为 8CA64DE9C1B123A7。
    const Bytes zero_key = from_hex("0000000000000000");
    auto zero_enc = des_encrypt_block(view(zero_key), view(from_hex("0000000000000000")));
    ASSERT_TRUE(zero_enc.is_ok());  // 未被弱密钥校验拒绝
    EXPECT_EQ(to_hex_upper(view(zero_enc.unwrap())), "8CA64DE9C1B123A7");

    // 4 个弱密钥：E(E(P)) = P（子密钥序列自逆）。
    const char* kWeak[] = {
        "0101010101010101",
        "FEFEFEFEFEFEFEFE",
        "E0E0E0E0F1F1F1F1",
        "1F1F1F1F0E0E0E0E",
    };
    for (const char* k : kWeak) {
        const Bytes key = from_hex(k);
        auto once = des_encrypt_block(view(key), view(plain));
        ASSERT_TRUE(once.is_ok()) << k;
        auto twice = des_encrypt_block(view(key), view(once.unwrap()));
        ASSERT_TRUE(twice.is_ok()) << k;
        EXPECT_EQ(to_hex_upper(view(twice.unwrap())), "0123456789ABCDEF") << k;
    }
    // JCE golden：弱密钥 0101010101010101 下 E(0) = 8CA64DE9C1B123A7。
    const Bytes weak = from_hex("0101010101010101");
    auto enc_zero = des_encrypt_block(view(weak), view(from_hex("0000000000000000")));
    ASSERT_TRUE(enc_zero.is_ok());
    EXPECT_EQ(to_hex_upper(view(enc_zero.unwrap())), "8CA64DE9C1B123A7");

    // 3 个半弱对：E_K2(E_K1(P)) = P 且 E_K1(E_K2(P)) = P。
    const char* kPairs[][2] = {
        {"01FE01FE01FE01FE", "FE01FE01FE01FE01"},
        {"1FE01FE00EF10EF1", "E01FE01FF10EF10E"},
        {"01E001E001F101F1", "E001E001F101F101"},
    };
    for (const auto& pair : kPairs) {
        const Bytes k1 = from_hex(pair[0]);
        const Bytes k2 = from_hex(pair[1]);
        auto e1 = des_encrypt_block(view(k1), view(plain));
        auto e2 = des_encrypt_block(view(k2), view(plain));
        ASSERT_TRUE(e1.is_ok());
        ASSERT_TRUE(e2.is_ok());
        auto back12 = des_encrypt_block(view(k2), view(e1.unwrap()));
        auto back21 = des_encrypt_block(view(k1), view(e2.unwrap()));
        ASSERT_TRUE(back12.is_ok());
        ASSERT_TRUE(back21.is_ok());
        EXPECT_EQ(to_hex_upper(view(back12.unwrap())), "0123456789ABCDEF") << pair[0];
        EXPECT_EQ(to_hex_upper(view(back21.unwrap())), "0123456789ABCDEF") << pair[1];
    }
}

// 错误路径：密钥/块/IV 长度与块对齐约束统一返回 INVALID_ARGUMENT。
TEST(DesTest, RejectsInvalidArguments)
{
    const Bytes key = from_hex("133457799BBCDFF1");
    const Bytes block = from_hex("0123456789ABCDEF");
    const Bytes iv = from_hex("FEDCBA9876543210");
    const u8 short_key_raw[7] = {0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF};
    const ByteSlice short_key(short_key_raw, 7);
    const u8 long_key_raw[9] = {0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1, 0x00};
    const ByteSlice long_key(long_key_raw, 9);
    const u8 empty_key_raw[1] = {0};
    const ByteSlice empty_key(empty_key_raw, 0);

    // 单块：key 长度 0/7/9、块长非 8。
    EXPECT_TRUE(des_encrypt_block(short_key, view(block)).is_err());
    EXPECT_TRUE(des_encrypt_block(long_key, view(block)).is_err());
    EXPECT_TRUE(des_encrypt_block(empty_key, view(block)).is_err());
    EXPECT_TRUE(des_encrypt_block(view(key), view(from_hex("0123456789ABCDEF00"))).is_err());
    EXPECT_TRUE(des_decrypt_block(short_key, view(block)).is_err());
    EXPECT_TRUE(des_decrypt_block(view(key), view(from_hex("01"))).is_err());

    // ECB/CBC：key 非法、数据非 8 倍数、IV 长度非法。
    const Bytes unaligned = from_hex("0102030405");
    EXPECT_TRUE(des_ecb_encrypt(short_key, view(block)).is_err());
    EXPECT_TRUE(des_ecb_encrypt(view(key), view(unaligned)).is_err());
    EXPECT_TRUE(des_ecb_decrypt(view(key), view(unaligned)).is_err());
    EXPECT_TRUE(des_cbc_encrypt(short_key, view(iv), view(block)).is_err());
    EXPECT_TRUE(des_cbc_encrypt(view(key), view(unaligned), view(block)).is_err());
    EXPECT_TRUE(des_cbc_decrypt(view(key), view(iv), view(unaligned)).is_err());

    // 空 IV 与空 key 的 CBC。
    const u8 empty_raw[1] = {0};
    EXPECT_TRUE(des_cbc_encrypt(view(key), ByteSlice(empty_raw, 0), view(block)).is_err());

    // 零长度数据是合法的块对齐输入（返回空输出）。
    auto empty_ecb = des_ecb_encrypt(view(key), view(Bytes{}));
    ASSERT_TRUE(empty_ecb.is_ok());
    EXPECT_EQ(empty_ecb.unwrap().len(), static_cast<usize>(0));
    auto empty_cbc = des_cbc_encrypt(view(key), view(iv), view(Bytes{}));
    ASSERT_TRUE(empty_cbc.is_ok());
    EXPECT_EQ(empty_cbc.unwrap().len(), static_cast<usize>(0));
}
