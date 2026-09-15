#include "libca/crypto/aes.hpp"

#include "libca/crypto/crypto_util.hpp"

#include <vector>

#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
#include <openssl/evp.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cwchar>
// STATUS_AUTH_TAG_MISMATCH 定义于 WDK 的 ntstatus.h，常规 SDK 构建不可得，按
// MS-ERREF 官方值 0xC000A002 兜底定义（GCM 认证标签不匹配）。
#ifndef STATUS_AUTH_TAG_MISMATCH
#define STATUS_AUTH_TAG_MISMATCH ((::NTSTATUS)0xC000A002L)
#endif
#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((::NTSTATUS)(status)) >= 0)
#endif
#endif

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

namespace {

// ══════════════════ 内置参考实现（FIPS-197 朴素实现） ══════════════════

// FIPS-197 S-box 与逆 S-box。
constexpr u8 AES_SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

constexpr u8 AES_INV_SBOX[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

// GF(2^8) 上乘 2：多项式 x^8 + x^4 + x^3 + x + 1（0x11b）。
u8 xtime(u8 value) noexcept
{
    return static_cast<u8>((value << 1) ^ ((value & 0x80) ? 0x1b : 0x00));
}

// GF(2^8) 上乘 x 的任意倍数（朴素倍加，正确性优先）。
u8 gmul(u8 a, u8 b) noexcept
{
    u8 result = 0;
    while (b != 0) {
        if (b & 1)
            result = static_cast<u8>(result ^ a);
        a = xtime(a);
        b >>= 1;
    }
    return result;
}

// 密钥长度 -> 轮数（FIPS-197：Nk=4/6/8 -> Nr=10/12/14）。
usize round_count(usize key_size) noexcept
{
    switch (key_size) {
    case 16: return 10;
    case 24: return 12;
    case 32: return 14;
    default: return 0;
    }
}

// FIPS-197 KeyExpansion：扩展出 4*(Nr+1) 个 32 位字，按大端字节序存入 round_keys。
void key_expansion(ByteSlice key, u8* round_keys) noexcept
{
    const usize nk = key.size() / 4;
    const usize nr = round_count(key.size());
    const usize total_words = 4 * (nr + 1);

    u32 words[60];  // 最大 4*(14+1) = 60
    for (usize i = 0; i < nk; ++i) {
        words[i] = (static_cast<u32>(key[4 * i]) << 24) |
                   (static_cast<u32>(key[4 * i + 1]) << 16) |
                   (static_cast<u32>(key[4 * i + 2]) << 8) |
                   static_cast<u32>(key[4 * i + 3]);
    }

    u8 rcon = 0x01;
    for (usize i = nk; i < total_words; ++i) {
        u32 temp = words[i - 1];
        if (i % nk == 0) {
            // RotWord + SubWord + Rcon
            temp = (temp << 8) | (temp >> 24);
            temp = (static_cast<u32>(AES_SBOX[(temp >> 24) & 0xff]) << 24) |
                   (static_cast<u32>(AES_SBOX[(temp >> 16) & 0xff]) << 16) |
                   (static_cast<u32>(AES_SBOX[(temp >> 8) & 0xff]) << 8) |
                   static_cast<u32>(AES_SBOX[temp & 0xff]);
            temp ^= static_cast<u32>(rcon) << 24;
            rcon = xtime(rcon);
        } else if (nk > 6 && i % nk == 4) {
            temp = (static_cast<u32>(AES_SBOX[(temp >> 24) & 0xff]) << 24) |
                   (static_cast<u32>(AES_SBOX[(temp >> 16) & 0xff]) << 16) |
                   (static_cast<u32>(AES_SBOX[(temp >> 8) & 0xff]) << 8) |
                   static_cast<u32>(AES_SBOX[temp & 0xff]);
        }
        words[i] = words[i - nk] ^ temp;
    }

    for (usize i = 0; i < total_words; ++i) {
        round_keys[4 * i] = static_cast<u8>(words[i] >> 24);
        round_keys[4 * i + 1] = static_cast<u8>(words[i] >> 16);
        round_keys[4 * i + 2] = static_cast<u8>(words[i] >> 8);
        round_keys[4 * i + 3] = static_cast<u8>(words[i]);
    }
}

// state 按 FIPS-197 列主序线性化：state[r + 4*c] = 输入第 (4*c + r) 字节。

void sub_bytes(u8* state) noexcept
{
    for (usize i = 0; i < 16; ++i)
        state[i] = AES_SBOX[state[i]];
}

void inv_sub_bytes(u8* state) noexcept
{
    for (usize i = 0; i < 16; ++i)
        state[i] = AES_INV_SBOX[state[i]];
}

void shift_rows(u8* state) noexcept
{
    u8 temp;
    // 行 1 左移 1
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;
    // 行 2 左移 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    // 行 3 左移 3
    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

void inv_shift_rows(u8* state) noexcept
{
    u8 temp;
    // 行 1 右移 1
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;
    // 行 2 右移 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    // 行 3 右移 3
    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

void mix_columns(u8* state) noexcept
{
    for (usize c = 0; c < 4; ++c) {
        u8* col = state + 4 * c;
        const u8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
        col[0] = static_cast<u8>(gmul(a0, 2) ^ gmul(a1, 3) ^ a2 ^ a3);
        col[1] = static_cast<u8>(a0 ^ gmul(a1, 2) ^ gmul(a2, 3) ^ a3);
        col[2] = static_cast<u8>(a0 ^ a1 ^ gmul(a2, 2) ^ gmul(a3, 3));
        col[3] = static_cast<u8>(gmul(a0, 3) ^ a1 ^ a2 ^ gmul(a3, 2));
    }
}

void inv_mix_columns(u8* state) noexcept
{
    for (usize c = 0; c < 4; ++c) {
        u8* col = state + 4 * c;
        const u8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
        col[0] = static_cast<u8>(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
        col[1] = static_cast<u8>(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
        col[2] = static_cast<u8>(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11));
        col[3] = static_cast<u8>(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14));
    }
}

void add_round_key(u8* state, const u8* round_keys, usize round) noexcept
{
    for (usize i = 0; i < 16; ++i)
        state[i] ^= round_keys[16 * round + i];
}

// 加密单个 16 字节块（FIPS-197 Cipher）。
void encrypt_block(const u8* round_keys, usize nr, const u8* in, u8* out) noexcept
{
    u8 state[16];
    for (usize i = 0; i < 16; ++i)
        state[i] = in[i];

    add_round_key(state, round_keys, 0);
    for (usize round = 1; round < nr; ++round) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, round_keys, round);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, round_keys, nr);

    for (usize i = 0; i < 16; ++i)
        out[i] = state[i];
    secure_zero(state, sizeof(state));  // state 含中间密钥相关值
}

// 解密单个 16 字节块（FIPS-197 InvCipher）。
void decrypt_block(const u8* round_keys, usize nr, const u8* in, u8* out) noexcept
{
    u8 state[16];
    for (usize i = 0; i < 16; ++i)
        state[i] = in[i];

    add_round_key(state, round_keys, nr);
    for (usize round = nr - 1; round >= 1; --round) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, round_keys, round);
        inv_mix_columns(state);
    }
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, round_keys, 0);

    for (usize i = 0; i < 16; ++i)
        out[i] = state[i];
    secure_zero(state, sizeof(state));
}

// 参数检查：密钥 16/24/32 字节；块数据为 16 字节整倍数（CBC/ECB 共用）。
bool key_valid(ByteSlice key) noexcept
{
    return key.size() == 16 || key.size() == 24 || key.size() == 32;
}

bool block_len_valid(usize len) noexcept
{
    return len % AES_BLOCK_SIZE == 0;
}

// 128 位大端计数 +1（SP800-38A CTR），溢出回绕不进位出块。
void increment_counter(u8 counter[AES_BLOCK_SIZE]) noexcept
{
    for (usize i = AES_BLOCK_SIZE; i > 0; --i) {
        if (++counter[i - 1] != 0)
            break;
    }
}

// 内置 ECB 加解密（direction：true 加密）。
Result<Bytes, CryptoError> builtin_ecb(ByteSlice key, ByteSlice input, bool direction)
{
    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];  // 最大 4*(14+1) 字 = 240 字节
    key_expansion(key, round_keys);

    BytesMut output = BytesMut::with_capacity(input.size());
    u8 block[AES_BLOCK_SIZE];
    for (usize offset = 0; offset < input.size(); offset += AES_BLOCK_SIZE) {
        if (direction)
            encrypt_block(round_keys, nr, input.data() + offset, block);
        else
            decrypt_block(round_keys, nr, input.data() + offset, block);
        output.put_slice(block, AES_BLOCK_SIZE);
    }
    secure_zero(round_keys, sizeof(round_keys));  // 轮密钥即等价密钥
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

// 内置 CBC 加解密（direction：true 加密）。
Result<Bytes, CryptoError> builtin_cbc(ByteSlice key, ByteSlice iv, ByteSlice input, bool direction)
{
    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];
    key_expansion(key, round_keys);

    u8 feedback[AES_BLOCK_SIZE];
    for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
        feedback[i] = iv[i];

    BytesMut output = BytesMut::with_capacity(input.size());
    u8 block[AES_BLOCK_SIZE];
    for (usize offset = 0; offset < input.size(); offset += AES_BLOCK_SIZE) {
        if (direction) {
            for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
                block[i] = static_cast<u8>(input[offset + i] ^ feedback[i]);
            encrypt_block(round_keys, nr, block, feedback);
            output.put_slice(feedback, AES_BLOCK_SIZE);
        } else {
            decrypt_block(round_keys, nr, input.data() + offset, block);
            for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
                block[i] = static_cast<u8>(block[i] ^ feedback[i]);
            output.put_slice(block, AES_BLOCK_SIZE);
            for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
                feedback[i] = input[offset + i];
        }
    }
    secure_zero(round_keys, sizeof(round_keys));
    secure_zero(feedback, sizeof(feedback));
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

// 内置 CTR 加解密（加解密同函数）。
Result<Bytes, CryptoError> builtin_ctr(ByteSlice key, ByteSlice counter_block, ByteSlice input)
{
    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];
    key_expansion(key, round_keys);

    u8 counter[AES_BLOCK_SIZE];
    for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
        counter[i] = counter_block[i];

    BytesMut output = BytesMut::with_capacity(input.size());
    u8 keystream[AES_BLOCK_SIZE];
    usize offset = 0;
    while (offset < input.size()) {
        encrypt_block(round_keys, nr, counter, keystream);

        const usize remaining = input.size() - offset;
        const usize take = remaining < AES_BLOCK_SIZE ? remaining : AES_BLOCK_SIZE;
        for (usize i = 0; i < take; ++i)
            output.put_u8(static_cast<u8>(input[offset + i] ^ keystream[i]));

        offset += take;
        increment_counter(counter);
    }
    secure_zero(round_keys, sizeof(round_keys));
    secure_zero(counter, sizeof(counter));
    secure_zero(keystream, sizeof(keystream));
    return Ok(output.freeze());
}

#if defined(LIBCA_CRYPTO_HAS_OPENSSL)

// ══════════════════ OpenSSL EVP 后端 ══════════════════

// 分组工作模式。
enum class OsslMode
{
    Ecb,
    Cbc,
    Ctr,
};

// 按密钥长度与模式选择 EVP 原语。
const EVP_CIPHER* openssl_cipher(ByteSlice key, OsslMode mode)
{
    const usize bits = key.size() * 8;
    switch (mode) {
    case OsslMode::Ecb:
        switch (bits) {
        case 128: return EVP_aes_128_ecb();
        case 192: return EVP_aes_192_ecb();
        case 256: return EVP_aes_256_ecb();
        default: return nullptr;
        }
    case OsslMode::Cbc:
        switch (bits) {
        case 128: return EVP_aes_128_cbc();
        case 192: return EVP_aes_192_cbc();
        case 256: return EVP_aes_256_cbc();
        default: return nullptr;
        }
    case OsslMode::Ctr:
        switch (bits) {
        case 128: return EVP_aes_128_ctr();
        case 192: return EVP_aes_192_ctr();
        case 256: return EVP_aes_256_ctr();
        default: return nullptr;
        }
    }
    return nullptr;
}

// GCM 原语按密钥长度选择。
const EVP_CIPHER* openssl_gcm_cipher(ByteSlice key)
{
    switch (key.size() * 8) {
    case 128: return EVP_aes_128_gcm();
    case 192: return EVP_aes_192_gcm();
    case 256: return EVP_aes_256_gcm();
    default: return nullptr;
    }
}

// ECB/CBC/CTR 通用 EVP 流程（direction：true 加密；ecb 时 iv 为空视图）。
Result<Bytes, CryptoError> openssl_crypt(const EVP_CIPHER* cipher, ByteSlice key, ByteSlice iv,
                                         ByteSlice input, bool direction)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr)
        return Err(CryptoError::BACKEND_FAILED);

    bool ok = true;
    std::vector<u8> output(input.size() + static_cast<usize>(EVP_MAX_BLOCK_LENGTH));
    int out_len = 0;
    int total = 0;
    const u8* iv_data = iv.empty() ? nullptr : iv.data();

    if (direction) {
        ok = ok && EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), iv_data) == 1;
        ok = ok && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1;
        ok = ok &&
             EVP_EncryptUpdate(ctx, output.data(), &out_len, input.data(),
                               static_cast<int>(input.size())) == 1;
        total = out_len;
        ok = ok && EVP_EncryptFinal_ex(ctx, output.data() + total, &out_len) == 1;
    } else {
        ok = ok && EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), iv_data) == 1;
        ok = ok && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1;
        ok = ok &&
             EVP_DecryptUpdate(ctx, output.data(), &out_len, input.data(),
                               static_cast<int>(input.size())) == 1;
        total = out_len;
        ok = ok && EVP_DecryptFinal_ex(ctx, output.data() + total, &out_len) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return Err(CryptoError::BACKEND_FAILED);
    return Ok(Bytes::copy_from_slice(output.data(), static_cast<usize>(total)));
}

// GCM 加密：返回密文与 16 字节 tag。
Result<AesGcmResult, CryptoError> openssl_gcm_encrypt(ByteSlice key, ByteSlice nonce,
                                                      ByteSlice plaintext, ByteSlice aad)
{
    const EVP_CIPHER* cipher = openssl_gcm_cipher(key);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (cipher == nullptr || ctx == nullptr) {
        if (ctx != nullptr)
            EVP_CIPHER_CTX_free(ctx);
        return Err(CryptoError::BACKEND_FAILED);
    }

    bool ok = true;
    int out_len = 0;
    ok = ok && EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) == 1;
    ok = ok &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()),
                             nullptr) == 1;
    ok = ok && EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) == 1;
    if (!aad.empty())
        ok = ok &&
             EVP_EncryptUpdate(ctx, nullptr, &out_len, aad.data(), static_cast<int>(aad.size())) == 1;

    std::vector<u8> ciphertext(plaintext.size());
    int total = 0;
    if (!plaintext.empty()) {
        ok = ok &&
             EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len, plaintext.data(),
                               static_cast<int>(plaintext.size())) == 1;
        total = out_len;
    }
    ok = ok && EVP_EncryptFinal_ex(ctx, ciphertext.data() + total, &out_len) == 1;
    total += out_len;  // GCM Final 不产出数据

    u8 tag[AES_GCM_TAG_SIZE] = {};
    ok = ok &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(AES_GCM_TAG_SIZE),
                             tag) == 1;
    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return Err(CryptoError::BACKEND_FAILED);

    AesGcmResult result;
    result.ciphertext = Bytes::copy_from_slice(ciphertext.data(), static_cast<usize>(total));
    result.tag = Bytes::copy_from_slice(tag, AES_GCM_TAG_SIZE);
    return Ok(std::move(result));
}

// GCM 解密：tag 校验失败返回 AUTHENTICATION_FAILED。
Result<Bytes, CryptoError> openssl_gcm_decrypt(ByteSlice key, ByteSlice nonce, ByteSlice ciphertext,
                                               ByteSlice tag, ByteSlice aad)
{
    const EVP_CIPHER* cipher = openssl_gcm_cipher(key);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (cipher == nullptr || ctx == nullptr) {
        if (ctx != nullptr)
            EVP_CIPHER_CTX_free(ctx);
        return Err(CryptoError::BACKEND_FAILED);
    }

    bool ok = true;
    int out_len = 0;
    ok = ok && EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) == 1;
    ok = ok &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()),
                             nullptr) == 1;
    ok = ok && EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) == 1;
    if (!aad.empty())
        ok = ok &&
             EVP_DecryptUpdate(ctx, nullptr, &out_len, aad.data(), static_cast<int>(aad.size())) == 1;

    std::vector<u8> plaintext(ciphertext.size());
    int total = 0;
    if (!ciphertext.empty()) {
        ok = ok &&
             EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, ciphertext.data(),
                               static_cast<int>(ciphertext.size())) == 1;
        total = out_len;
    }
    ok = ok &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(AES_GCM_TAG_SIZE),
                             const_cast<u8*>(tag.data())) == 1;
    ok = ok && EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &out_len) == 1;
    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return Err(CryptoError::AUTHENTICATION_FAILED);
    return Ok(Bytes::copy_from_slice(plaintext.data(), static_cast<usize>(total)));
}

#endif  // LIBCA_CRYPTO_HAS_OPENSSL

#if defined(_WIN32)

// ══════════════════ Windows CNG（bcrypt.dll）后端 ══════════════════

// RAII：算法 provider 句柄。
struct CngAlgHandle
{
    BCRYPT_ALG_HANDLE handle{nullptr};

    ~CngAlgHandle()
    {
        if (handle != nullptr)
            BCryptCloseAlgorithmProvider(handle, 0);
    }
};

// RAII：对称密钥句柄。
struct CngKeyHandle
{
    BCRYPT_KEY_HANDLE handle{nullptr};

    ~CngKeyHandle()
    {
        if (handle != nullptr)
            BCryptDestroyKey(handle);
    }
};

// 打开 AES provider 并设置 chaining mode（须在 GenerateSymmetricKey 之前）。
// 失败时保留已打开的句柄，由 CngAlgHandle 析构统一关闭。
bool cng_open_aes(const wchar_t* chaining_mode, CngAlgHandle& alg)
{
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        return false;
    if (!NT_SUCCESS(BCryptSetProperty(alg.handle, BCRYPT_CHAINING_MODE,
                                      reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(chaining_mode)),
                                      static_cast<ULONG>((wcslen(chaining_mode) + 1) *
                                                         sizeof(wchar_t)),
                                      0)))
        return false;
    return true;
}

// ECB/CBC 通用 CNG 流程（direction：true 加密；ECB 传空 iv）。
Result<Bytes, CryptoError> cng_crypt(const wchar_t* chaining_mode, ByteSlice key, ByteSlice iv,
                                     ByteSlice input, bool direction)
{
    CngAlgHandle alg;
    if (!cng_open_aes(chaining_mode, alg))
        return Err(CryptoError::BACKEND_FAILED);

    CngKeyHandle key_handle;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0,
                                               const_cast<PUCHAR>(key.data()),
                                               static_cast<ULONG>(key.size()), 0)))
        return Err(CryptoError::BACKEND_FAILED);

    // CNG 会就地更新 IV 缓冲，必须传副本。
    u8 iv_copy[AES_BLOCK_SIZE] = {};
    PUCHAR iv_arg = nullptr;
    ULONG iv_len = 0;
    if (!iv.empty()) {
        for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
            iv_copy[i] = iv[i];
        iv_arg = iv_copy;
        iv_len = AES_BLOCK_SIZE;
    }

    std::vector<u8> output(input.size());
    ULONG result_len = 0;
    NTSTATUS status;
    if (direction) {
        status = BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(input.data()),
                               static_cast<ULONG>(input.size()), nullptr, iv_arg, iv_len,
                               output.empty() ? nullptr : output.data(),
                               static_cast<ULONG>(output.size()), &result_len, 0);
    } else {
        status = BCryptDecrypt(key_handle.handle, const_cast<PUCHAR>(input.data()),
                               static_cast<ULONG>(input.size()), nullptr, iv_arg, iv_len,
                               output.empty() ? nullptr : output.data(),
                               static_cast<ULONG>(output.size()), &result_len, 0);
    }
    if (!NT_SUCCESS(status))
        return Err(CryptoError::BACKEND_FAILED);
    if (result_len != output.size())
        return Err(CryptoError::BACKEND_FAILED);
    return Ok(Bytes::copy_from_slice(output.data(), output.size()));
}

// CTR 专用 CNG 流程。实测部分 Windows 的 AES primitive provider 会拒绝
// ChainingModeCTR（BCryptSetProperty 返回 STATUS_INVALID_PARAMETER），因此这里
// 不依赖原生 CTR：用「连续计数块缓冲 + ECB 批量加密」生成 keystream 后与数据
// XOR，计数块按 128 位大端递增，语义与 SP800-38A 及其他后端严格一致。
Result<Bytes, CryptoError> cng_ctr_crypt(ByteSlice key, ByteSlice counter_block, ByteSlice input)
{
    CngAlgHandle alg;
    if (!cng_open_aes(BCRYPT_CHAIN_MODE_ECB, alg))
        return Err(CryptoError::BACKEND_FAILED);

    CngKeyHandle key_handle;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0,
                                               const_cast<PUCHAR>(key.data()),
                                               static_cast<ULONG>(key.size()), 0)))
        return Err(CryptoError::BACKEND_FAILED);

    // 批处理规模：每次调用最多生成 4096 块（64KB）keystream。
    constexpr usize kMaxBatchBlocks = 4096;
    const usize total_blocks = (input.size() + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
    const usize batch_blocks = total_blocks < kMaxBatchBlocks ? total_blocks : kMaxBatchBlocks;

    std::vector<u8> counters(batch_blocks * AES_BLOCK_SIZE);
    std::vector<u8> keystream(batch_blocks * AES_BLOCK_SIZE);

    u8 counter[AES_BLOCK_SIZE];
    for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
        counter[i] = counter_block[i];

    BytesMut output = BytesMut::with_capacity(input.size());
    usize offset = 0;
    while (offset < input.size()) {
        const usize remaining = input.size() - offset;
        const usize blocks = (remaining + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
        const usize n = blocks < batch_blocks ? blocks : batch_blocks;

        for (usize b = 0; b < n; ++b) {
            for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
                counters[b * AES_BLOCK_SIZE + i] = counter[i];
            increment_counter(counter);
        }

        ULONG done = 0;
        if (!NT_SUCCESS(BCryptEncrypt(key_handle.handle, counters.data(),
                                      static_cast<ULONG>(n * AES_BLOCK_SIZE), nullptr, nullptr, 0,
                                      keystream.data(),
                                      static_cast<ULONG>(n * AES_BLOCK_SIZE), &done, 0)))
            return Err(CryptoError::BACKEND_FAILED);
        if (done != n * AES_BLOCK_SIZE)
            return Err(CryptoError::BACKEND_FAILED);

        const usize take = remaining < n * AES_BLOCK_SIZE ? remaining : n * AES_BLOCK_SIZE;
        for (usize i = 0; i < take; ++i)
            output.put_u8(static_cast<u8>(input[offset + i] ^ keystream[i]));
        offset += take;
    }

    secure_zero(counter, sizeof(counter));
    secure_zero(counters.data(), counters.size());
    secure_zero(keystream.data(), keystream.size());
    return Ok(output.freeze());
}

// GCM 加密：返回密文与 16 字节 tag。
Result<AesGcmResult, CryptoError> cng_gcm_encrypt(ByteSlice key, ByteSlice nonce,
                                                  ByteSlice plaintext, ByteSlice aad)
{
    CngAlgHandle alg;
    if (!cng_open_aes(BCRYPT_CHAIN_MODE_GCM, alg))
        return Err(CryptoError::BACKEND_FAILED);

    CngKeyHandle key_handle;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0,
                                               const_cast<PUCHAR>(key.data()),
                                               static_cast<ULONG>(key.size()), 0)))
        return Err(CryptoError::BACKEND_FAILED);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = const_cast<PUCHAR>(nonce.data());
    info.cbNonce = static_cast<ULONG>(nonce.size());
    u8 tag[AES_GCM_TAG_SIZE] = {};
    info.pbTag = tag;
    info.cbTag = static_cast<ULONG>(AES_GCM_TAG_SIZE);
    if (!aad.empty()) {
        info.pbAuthData = const_cast<PUCHAR>(aad.data());
        info.cbAuthData = static_cast<ULONG>(aad.size());
    }

    std::vector<u8> output(plaintext.size());
    ULONG result_len = 0;
    if (!NT_SUCCESS(BCryptEncrypt(key_handle.handle,
                                  plaintext.empty() ? nullptr : const_cast<PUCHAR>(plaintext.data()),
                                  static_cast<ULONG>(plaintext.size()), &info, nullptr, 0,
                                  output.empty() ? nullptr : output.data(),
                                  static_cast<ULONG>(output.size()), &result_len, 0)))
        return Err(CryptoError::BACKEND_FAILED);

    AesGcmResult result;
    result.ciphertext = Bytes::copy_from_slice(output.data(), output.size());
    result.tag = Bytes::copy_from_slice(tag, AES_GCM_TAG_SIZE);
    return Ok(std::move(result));
}

// GCM 解密：tag 不匹配（STATUS_AUTH_TAG_MISMATCH）返回 AUTHENTICATION_FAILED。
Result<Bytes, CryptoError> cng_gcm_decrypt(ByteSlice key, ByteSlice nonce, ByteSlice ciphertext,
                                           ByteSlice tag, ByteSlice aad)
{
    CngAlgHandle alg;
    if (!cng_open_aes(BCRYPT_CHAIN_MODE_GCM, alg))
        return Err(CryptoError::BACKEND_FAILED);

    CngKeyHandle key_handle;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0,
                                               const_cast<PUCHAR>(key.data()),
                                               static_cast<ULONG>(key.size()), 0)))
        return Err(CryptoError::BACKEND_FAILED);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = const_cast<PUCHAR>(nonce.data());
    info.cbNonce = static_cast<ULONG>(nonce.size());
    info.pbTag = const_cast<PUCHAR>(tag.data());
    info.cbTag = static_cast<ULONG>(tag.size());
    if (!aad.empty()) {
        info.pbAuthData = const_cast<PUCHAR>(aad.data());
        info.cbAuthData = static_cast<ULONG>(aad.size());
    }

    std::vector<u8> output(ciphertext.size());
    ULONG result_len = 0;
    const NTSTATUS status = BCryptDecrypt(
        key_handle.handle,
        ciphertext.empty() ? nullptr : const_cast<PUCHAR>(ciphertext.data()),
        static_cast<ULONG>(ciphertext.size()), &info, nullptr, 0,
        output.empty() ? nullptr : output.data(), static_cast<ULONG>(output.size()), &result_len, 0);
    if (status == STATUS_AUTH_TAG_MISMATCH)
        return Err(CryptoError::AUTHENTICATION_FAILED);
    if (!NT_SUCCESS(status))
        return Err(CryptoError::BACKEND_FAILED);
    if (result_len != output.size())
        return Err(CryptoError::BACKEND_FAILED);
    return Ok(Bytes::copy_from_slice(output.data(), output.size()));
}

#endif  // _WIN32

// ══════════════════ 后端解析与分发 ══════════════════

// 把 Auto 折算为具体后端：OpenSSL（编译期可用）> CNG（Windows）> Builtin。
AesBackend resolve_backend(AesBackend backend) noexcept
{
    if (backend != AesBackend::Auto)
        return backend;
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    return AesBackend::OpenSsl;
#elif defined(_WIN32)
    return AesBackend::Cng;
#else
    return AesBackend::Builtin;
#endif
}

// 检查 GCM 参数：密钥长度 + nonce 固定 12 字节 + tag 固定 16 字节。
bool gcm_args_valid(ByteSlice key, ByteSlice nonce, ByteSlice tag) noexcept
{
    return key_valid(key) && nonce.size() == AES_GCM_NONCE_SIZE && tag.size() == AES_GCM_TAG_SIZE;
}

}  // namespace

bool aes_backend_available(AesBackend backend) noexcept
{
    switch (resolve_backend(backend)) {
    case AesBackend::Builtin:
        return true;
    case AesBackend::OpenSsl:
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
        return true;
#else
        return false;
#endif
    case AesBackend::Cng:
#if defined(_WIN32)
        return true;
#else
        return false;
#endif
    default:
        return false;
    }
}

Result<Bytes, CryptoError> aes_ecb_encrypt(ByteSlice key, ByteSlice plaintext, AesBackend backend)
{
    if (!key_valid(key) || !block_len_valid(plaintext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
    case AesBackend::Builtin:
        return builtin_ecb(key, plaintext, true);
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_crypt(openssl_cipher(key, OsslMode::Ecb), key, ByteSlice{}, plaintext, true);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_crypt(BCRYPT_CHAIN_MODE_ECB, key, ByteSlice{}, plaintext, true);
#endif
    default:
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

Result<Bytes, CryptoError> aes_ecb_decrypt(ByteSlice key, ByteSlice ciphertext, AesBackend backend)
{
    if (!key_valid(key) || !block_len_valid(ciphertext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
    case AesBackend::Builtin:
        return builtin_ecb(key, ciphertext, false);
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_crypt(openssl_cipher(key, OsslMode::Ecb), key, ByteSlice{}, ciphertext,
                             false);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_crypt(BCRYPT_CHAIN_MODE_ECB, key, ByteSlice{}, ciphertext, false);
#endif
    default:
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

Result<Bytes, CryptoError> aes_cbc_encrypt(ByteSlice key, ByteSlice iv, ByteSlice plaintext,
                                           AesBackend backend)
{
    if (!key_valid(key) || iv.size() != AES_BLOCK_SIZE || !block_len_valid(plaintext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
    case AesBackend::Builtin:
        return builtin_cbc(key, iv, plaintext, true);
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_crypt(openssl_cipher(key, OsslMode::Cbc), key, iv, plaintext, true);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_crypt(BCRYPT_CHAIN_MODE_CBC, key, iv, plaintext, true);
#endif
    default:
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

Result<Bytes, CryptoError> aes_cbc_decrypt(ByteSlice key, ByteSlice iv, ByteSlice ciphertext,
                                           AesBackend backend)
{
    if (!key_valid(key) || iv.size() != AES_BLOCK_SIZE || !block_len_valid(ciphertext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
    case AesBackend::Builtin:
        return builtin_cbc(key, iv, ciphertext, false);
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_crypt(openssl_cipher(key, OsslMode::Cbc), key, iv, ciphertext, false);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_crypt(BCRYPT_CHAIN_MODE_CBC, key, iv, ciphertext, false);
#endif
    default:
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

Result<Bytes, CryptoError> aes_ctr_crypt(ByteSlice key, ByteSlice counter_block, ByteSlice data,
                                         AesBackend backend)
{
    if (!key_valid(key) || counter_block.size() != AES_BLOCK_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
    case AesBackend::Builtin:
        return builtin_ctr(key, counter_block, data);
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_crypt(openssl_cipher(key, OsslMode::Ctr), key, counter_block, data, true);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_ctr_crypt(key, counter_block, data);
#endif
    default:
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

Result<AesGcmResult, CryptoError> aes_gcm_encrypt(ByteSlice key, ByteSlice nonce,
                                                  ByteSlice plaintext, ByteSlice aad,
                                                  AesBackend backend)
{
    // GCM 只提供外部后端路径：nonce 固定 12 字节（本模块统一口径）。
    if (!key_valid(key) || nonce.size() != AES_GCM_NONCE_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_gcm_encrypt(key, nonce, plaintext, aad);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_gcm_encrypt(key, nonce, plaintext, aad);
#endif
    default:
        // Builtin 与编译期不可用的外部后端均不支持 GCM。
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

Result<Bytes, CryptoError> aes_gcm_decrypt(ByteSlice key, ByteSlice nonce, ByteSlice ciphertext,
                                           ByteSlice tag, ByteSlice aad, AesBackend backend)
{
    if (!gcm_args_valid(key, nonce, tag))
        return Err(CryptoError::INVALID_ARGUMENT);

    switch (resolve_backend(backend)) {
#if defined(LIBCA_CRYPTO_HAS_OPENSSL)
    case AesBackend::OpenSsl:
        return openssl_gcm_decrypt(key, nonce, ciphertext, tag, aad);
#endif
#if defined(_WIN32)
    case AesBackend::Cng:
        return cng_gcm_decrypt(key, nonce, ciphertext, tag, aad);
#endif
    default:
        // Builtin 与编译期不可用的外部后端均不支持 GCM。
        return Err(CryptoError::UNSUPPORTED_ALGORITHM);
    }
}

}  // namespace ca::crypto
