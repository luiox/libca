#include "libca/crypto/des.hpp"

#include "libca/crypto/crypto_util.hpp"

namespace ca::crypto {

namespace {

// ══════════════════ FIPS 46-3 常量表 ══════════════════

// 位序约定：表项 n（1 起）表示取输入第 n 位（最高有效位为第 1 位）作为输出的下一位。
// 弱密钥/半弱密钥按 FIPS 46-3 附录 A 定义存在，但本实现不做校验（对齐 JCE DESKeySpec：
// 只读 8 字节原始密钥，不调整、不拒绝任何奇偶性的密钥；奇偶位经 PC-1 自然丢弃）。

// 初始置换 IP。
constexpr u8 IP_TABLE[64] = {
    58, 50, 42, 34, 26, 18, 10, 2,
    60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,
    64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1,
    59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,
    63, 55, 47, 39, 31, 23, 15, 7,
};

// 末置换 FP（IP 的逆置换）。
constexpr u8 FP_TABLE[64] = {
    40, 8, 48, 16, 56, 24, 64, 32,
    39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30,
    37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28,
    35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26,
    33, 1, 41, 9, 49, 17, 57, 25,
};

// 轮函数 E 扩展（32 位 → 48 位）。
constexpr u8 E_TABLE[48] = {
    32, 1, 2, 3, 4, 5,
    4, 5, 6, 7, 8, 9,
    8, 9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32, 1,
};

// 轮函数 P 置换（32 位 → 32 位）。
constexpr u8 P_TABLE[32] = {
    16, 7, 20, 21,
    29, 12, 28, 17,
    1, 15, 23, 26,
    5, 18, 31, 10,
    2, 8, 24, 14,
    32, 27, 3, 9,
    19, 13, 30, 6,
    22, 11, 4, 25,
};

// 密钥调度 PC-1（64 位 → 56 位，丢弃奇偶校验位）。
constexpr u8 PC1_TABLE[56] = {
    57, 49, 41, 33, 25, 17, 9,
    1, 58, 50, 42, 34, 26, 18,
    10, 2, 59, 51, 43, 35, 27,
    19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
    7, 62, 54, 46, 38, 30, 22,
    14, 6, 61, 53, 45, 37, 29,
    21, 13, 5, 28, 20, 12, 4,
};

// 密钥调度 PC-2（56 位 → 48 位）。
constexpr u8 PC2_TABLE[48] = {
    14, 17, 11, 24, 1, 5,
    3, 28, 15, 6, 21, 10,
    23, 19, 12, 4, 26, 8,
    16, 7, 27, 20, 13, 2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32,
};

// 每轮循环左移位数（C/D 各 28 位同移）。
constexpr u8 SHIFT_TABLE[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};

// 8 个 S 盒（4 行 × 16 列）：行 = 首末位拼接，列 = 中间 4 位。
constexpr u8 S_BOXES[8][4][16] = {
    {
        {14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7},
        {0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8},
        {4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0},
        {15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
    },
    {
        {15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10},
        {3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5},
        {0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15},
        {13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
    },
    {
        {10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8},
        {13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1},
        {13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7},
        {1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
    },
    {
        {7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15},
        {13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9},
        {10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4},
        {3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
    },
    {
        {2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9},
        {14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6},
        {4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14},
        {11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
    },
    {
        {12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11},
        {10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8},
        {9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6},
        {4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
    },
    {
        {4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1},
        {13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6},
        {1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2},
        {6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
    },
    {
        {13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7},
        {1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2},
        {7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8},
        {2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11},
    },
};

// 通用位置换：value 右对齐存储，in_width 为其有效位宽，按表逐位抽取输出。
u64 permute(u64 value, usize in_width, const u8* table, usize table_size)
{
    u64 out = 0;
    for (usize i = 0; i < table_size; ++i)
        out = (out << 1) | ((value >> (in_width - table[i])) & 1);
    return out;
}

// 轮函数 f：E 扩展 → 与子密钥异或 → 8 组 S 盒压缩 → P 置换。
u32 feistel(u32 half, u64 subkey)
{
    const u64 expanded = permute(half, 32, E_TABLE, 48);
    const u64 mixed = expanded ^ subkey;
    u32 s_output = 0;
    for (int box = 0; box < 8; ++box) {
        const u64 six_bits = (mixed >> (42 - 6 * box)) & 0x3F;
        const usize row = static_cast<usize>(((six_bits >> 4) & 0x2) | (six_bits & 0x1));
        const usize col = static_cast<usize>((six_bits >> 1) & 0xF);
        s_output = (s_output << 4) | S_BOXES[box][row][col];
    }
    return static_cast<u32>(permute(s_output, 32, P_TABLE, 32));
}

// 密钥调度：PC-1 → C/D 16 轮循环左移 → PC-2。reverse 为真时按解密序（子密钥逆序）。
void derive_subkeys(u64 key, bool reverse, u64 subkeys[16])
{
    const u64 cd = permute(key, 64, PC1_TABLE, 56);
    u32 c = static_cast<u32>(cd >> 28) & 0x0FFFFFFFu;
    u32 d = static_cast<u32>(cd) & 0x0FFFFFFFu;
    for (int round = 0; round < 16; ++round) {
        const int shift = SHIFT_TABLE[round];
        c = ((c << shift) | (c >> (28 - shift))) & 0x0FFFFFFFu;
        d = ((d << shift) | (d >> (28 - shift))) & 0x0FFFFFFFu;
        const u64 cd_round = (static_cast<u64>(c) << 28) | d;
        subkeys[reverse ? 15 - round : round] = permute(cd_round, 56, PC2_TABLE, 48);
    }
}

// 单块核心：IP → 16 轮 Feistel → 第 16 轮后左右互换 → FP。
u64 des_crypt_block(u64 block, const u64 subkeys[16])
{
    const u64 permuted = permute(block, 64, IP_TABLE, 64);
    u32 left = static_cast<u32>(permuted >> 32);
    u32 right = static_cast<u32>(permuted);
    for (int round = 0; round < 16; ++round) {
        const u32 prev_left = left;
        left = right;
        right = prev_left ^ feistel(right, subkeys[round]);
    }
    const u64 preoutput = (static_cast<u64>(right) << 32) | left;
    return permute(preoutput, 64, FP_TABLE, 64);
}

// 8 字节大端装载/写出（DES 位序按大端展开）。
u64 load_be_u64(const u8* data)
{
    u64 value = 0;
    for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
        value = (value << 8) | data[i];
    return value;
}

void store_be_u64(u8* data, u64 value)
{
    for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
        data[i] = static_cast<u8>(value >> (56 - 8 * i));
}

// 统一入口：按给定子密钥序逐块处理 ECB。
Result<Bytes, CryptoError> des_ecb_crypt(ByteSlice key, ByteSlice input, bool reverse)
{
    if (key.size() != DES_KEY_SIZE || input.size() % DES_BLOCK_SIZE != 0)
        return Err(CryptoError::INVALID_ARGUMENT);

    u64 subkeys[16];
    derive_subkeys(load_be_u64(key.data()), reverse, subkeys);

    BytesMut output = BytesMut::with_capacity(input.size());
    u8 block[DES_BLOCK_SIZE];
    for (usize offset = 0; offset < input.size(); offset += DES_BLOCK_SIZE) {
        const u64 processed = des_crypt_block(load_be_u64(input.data() + offset), subkeys);
        store_be_u64(block, processed);
        output.put_slice(block, DES_BLOCK_SIZE);
    }

    secure_zero(subkeys, sizeof(subkeys));  // 子密钥与密钥材料等价
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

// 统一入口：CBC 链接处理。encrypt 为真走加密链（先异或反馈后加密），否则走解密链。
Result<Bytes, CryptoError> des_cbc_crypt(ByteSlice key, ByteSlice iv, ByteSlice input,
                                         bool encrypt)
{
    if (key.size() != DES_KEY_SIZE || iv.size() != DES_BLOCK_SIZE
        || input.size() % DES_BLOCK_SIZE != 0)
        return Err(CryptoError::INVALID_ARGUMENT);

    u64 subkeys[16];
    derive_subkeys(load_be_u64(key.data()), !encrypt, subkeys);

    u8 feedback[DES_BLOCK_SIZE];
    for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
        feedback[i] = iv[i];

    BytesMut output = BytesMut::with_capacity(input.size());
    u8 block[DES_BLOCK_SIZE];
    for (usize offset = 0; offset < input.size(); offset += DES_BLOCK_SIZE) {
        if (encrypt) {
            // 加密链：先明文异或反馈再加密，C_i = E(P_i xor C_{i-1})。
            u8 xored[DES_BLOCK_SIZE];
            for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
                xored[i] = static_cast<u8>(input[offset + i] ^ feedback[i]);
            store_be_u64(block, des_crypt_block(load_be_u64(xored), subkeys));
            for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
                feedback[i] = block[i];
        } else {
            // 解密链：先块解密再异或前块密文，P_i = D(C_i) xor C_{i-1}。
            store_be_u64(block, des_crypt_block(load_be_u64(input.data() + offset), subkeys));
            for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
                block[i] = static_cast<u8>(block[i] ^ feedback[i]);
            for (usize i = 0; i < DES_BLOCK_SIZE; ++i)
                feedback[i] = input[offset + i];
        }
        output.put_slice(block, DES_BLOCK_SIZE);
    }

    secure_zero(subkeys, sizeof(subkeys));
    secure_zero(feedback, sizeof(feedback));
    secure_zero(block, sizeof(block));
    return Ok(output.freeze());
}

}  // namespace

Result<Bytes, CryptoError> des_encrypt_block(ByteSlice key, ByteSlice block)
{
    if (key.size() != DES_KEY_SIZE || block.size() != DES_BLOCK_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    u64 subkeys[16];
    derive_subkeys(load_be_u64(key.data()), false, subkeys);
    const u64 cipher = des_crypt_block(load_be_u64(block.data()), subkeys);
    secure_zero(subkeys, sizeof(subkeys));

    BytesMut output = BytesMut::with_capacity(DES_BLOCK_SIZE);
    u8 raw[DES_BLOCK_SIZE];
    store_be_u64(raw, cipher);
    output.put_slice(raw, DES_BLOCK_SIZE);
    secure_zero(raw, sizeof(raw));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> des_decrypt_block(ByteSlice key, ByteSlice block)
{
    if (key.size() != DES_KEY_SIZE || block.size() != DES_BLOCK_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    u64 subkeys[16];
    derive_subkeys(load_be_u64(key.data()), true, subkeys);
    const u64 plain = des_crypt_block(load_be_u64(block.data()), subkeys);
    secure_zero(subkeys, sizeof(subkeys));

    BytesMut output = BytesMut::with_capacity(DES_BLOCK_SIZE);
    u8 raw[DES_BLOCK_SIZE];
    store_be_u64(raw, plain);
    output.put_slice(raw, DES_BLOCK_SIZE);
    secure_zero(raw, sizeof(raw));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> des_ecb_encrypt(ByteSlice key, ByteSlice plaintext)
{
    return des_ecb_crypt(key, plaintext, false);
}

Result<Bytes, CryptoError> des_ecb_decrypt(ByteSlice key, ByteSlice ciphertext)
{
    return des_ecb_crypt(key, ciphertext, true);
}

Result<Bytes, CryptoError> des_cbc_encrypt(ByteSlice key, ByteSlice iv, ByteSlice plaintext)
{
    return des_cbc_crypt(key, iv, plaintext, true);
}

Result<Bytes, CryptoError> des_cbc_decrypt(ByteSlice key, ByteSlice iv, ByteSlice ciphertext)
{
    return des_cbc_crypt(key, iv, ciphertext, false);
}

Result<Bytes, CryptoError> pkcs5_pad(ByteSlice plaintext)
{
    // PKCS#5 固定按 DES 块大小 8：pad 值 1..8，空输入或整块对齐输入补满一块。
    const usize pad_len = DES_BLOCK_SIZE - plaintext.size() % DES_BLOCK_SIZE;
    BytesMut output = BytesMut::with_capacity(plaintext.size() + pad_len);
    output.put_slice(plaintext.data(), plaintext.size());
    for (usize i = 0; i < pad_len; ++i)
        output.put_u8(static_cast<u8>(pad_len));
    return Ok(output.freeze());
}

Result<Bytes, CryptoError> pkcs5_unpad(ByteSlice padded)
{
    if (padded.size() == 0 || padded.size() % DES_BLOCK_SIZE != 0)
        return Err(CryptoError::INVALID_ARGUMENT);

    const u8 pad_len = padded[padded.size() - 1];
    if (pad_len == 0 || pad_len > DES_BLOCK_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);
    for (usize i = padded.size() - pad_len; i < padded.size(); ++i) {
        if (padded[i] != pad_len)
            return Err(CryptoError::INVALID_ARGUMENT);
    }
    return Ok(Bytes::copy_from_slice(padded.data(), padded.size() - pad_len));
}

}  // namespace ca::crypto
