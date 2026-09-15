#include "libca/crypto/aes.hpp"

#include "libca/crypto/crypto_util.hpp"

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

namespace {

// ── FIPS-197 S-box 与逆 S-box ──
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

}  // namespace

Result<Bytes, CryptoError> aes_ecb_encrypt(ByteSlice key, ByteSlice plaintext)
{
    if (!key_valid(key) || !block_len_valid(plaintext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];  // 最大 4*(14+1) 字 = 240 字节
    key_expansion(key, round_keys);

    BytesMut output = BytesMut::with_capacity(plaintext.size());
    u8 block[AES_BLOCK_SIZE];
    for (usize offset = 0; offset < plaintext.size(); offset += AES_BLOCK_SIZE) {
        encrypt_block(round_keys, nr, plaintext.data() + offset, block);
        output.put_slice(block, AES_BLOCK_SIZE);
    }
    secure_zero(round_keys, sizeof(round_keys));  // 轮密钥即等价密钥
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> aes_ecb_decrypt(ByteSlice key, ByteSlice ciphertext)
{
    if (!key_valid(key) || !block_len_valid(ciphertext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];
    key_expansion(key, round_keys);

    BytesMut output = BytesMut::with_capacity(ciphertext.size());
    u8 block[AES_BLOCK_SIZE];
    for (usize offset = 0; offset < ciphertext.size(); offset += AES_BLOCK_SIZE) {
        decrypt_block(round_keys, nr, ciphertext.data() + offset, block);
        output.put_slice(block, AES_BLOCK_SIZE);
    }
    secure_zero(round_keys, sizeof(round_keys));
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> aes_cbc_encrypt(ByteSlice key, ByteSlice iv, ByteSlice plaintext)
{
    if (!key_valid(key) || iv.size() != AES_BLOCK_SIZE || !block_len_valid(plaintext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];
    key_expansion(key, round_keys);

    u8 feedback[AES_BLOCK_SIZE];
    for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
        feedback[i] = iv[i];

    BytesMut output = BytesMut::with_capacity(plaintext.size());
    u8 block[AES_BLOCK_SIZE];
    for (usize offset = 0; offset < plaintext.size(); offset += AES_BLOCK_SIZE) {
        for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
            block[i] = static_cast<u8>(plaintext[offset + i] ^ feedback[i]);
        encrypt_block(round_keys, nr, block, feedback);
        output.put_slice(feedback, AES_BLOCK_SIZE);
    }
    secure_zero(round_keys, sizeof(round_keys));
    secure_zero(feedback, sizeof(feedback));
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> aes_cbc_decrypt(ByteSlice key, ByteSlice iv, ByteSlice ciphertext)
{
    if (!key_valid(key) || iv.size() != AES_BLOCK_SIZE || !block_len_valid(ciphertext.size()))
        return Err(CryptoError::INVALID_ARGUMENT);

    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];
    key_expansion(key, round_keys);

    u8 feedback[AES_BLOCK_SIZE];
    for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
        feedback[i] = iv[i];

    BytesMut output = BytesMut::with_capacity(ciphertext.size());
    u8 block[AES_BLOCK_SIZE];
    for (usize offset = 0; offset < ciphertext.size(); offset += AES_BLOCK_SIZE) {
        decrypt_block(round_keys, nr, ciphertext.data() + offset, block);
        for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
            block[i] = static_cast<u8>(block[i] ^ feedback[i]);
        output.put_slice(block, AES_BLOCK_SIZE);
        for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
            feedback[i] = ciphertext[offset + i];
    }
    secure_zero(round_keys, sizeof(round_keys));
    secure_zero(feedback, sizeof(feedback));
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> aes_ctr_crypt(ByteSlice key, ByteSlice counter_block, ByteSlice data)
{
    if (!key_valid(key) || counter_block.size() != AES_BLOCK_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    const usize nr = round_count(key.size());
    u8 round_keys[16 * 15];
    key_expansion(key, round_keys);

    u8 counter[AES_BLOCK_SIZE];
    for (usize i = 0; i < AES_BLOCK_SIZE; ++i)
        counter[i] = counter_block[i];

    BytesMut output = BytesMut::with_capacity(data.size());
    u8 keystream[AES_BLOCK_SIZE];
    usize offset = 0;
    while (offset < data.size()) {
        encrypt_block(round_keys, nr, counter, keystream);

        const usize remaining = data.size() - offset;
        const usize take = remaining < AES_BLOCK_SIZE ? remaining : AES_BLOCK_SIZE;
        for (usize i = 0; i < take; ++i)
            output.put_u8(static_cast<u8>(data[offset + i] ^ keystream[i]));

        offset += take;
        increment_counter(counter);
    }
    secure_zero(round_keys, sizeof(round_keys));
    secure_zero(counter, sizeof(counter));
    secure_zero(keystream, sizeof(keystream));
    return Ok(output.freeze());
}

}  // namespace ca::crypto
