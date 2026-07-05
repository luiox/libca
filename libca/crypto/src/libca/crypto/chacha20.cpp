#include "libca/crypto/chacha20.hpp"

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

namespace {

u32 load_le32(const u8* data) noexcept
{
    return static_cast<u32>(data[0]) |
           (static_cast<u32>(data[1]) << 8) |
           (static_cast<u32>(data[2]) << 16) |
           (static_cast<u32>(data[3]) << 24);
}

void store_le32(u8* out, u32 value) noexcept
{
    out[0] = static_cast<u8>(value & 0xff);
    out[1] = static_cast<u8>((value >> 8) & 0xff);
    out[2] = static_cast<u8>((value >> 16) & 0xff);
    out[3] = static_cast<u8>((value >> 24) & 0xff);
}

u32 rotl32(u32 value, u32 bits) noexcept
{
    return (value << bits) | (value >> (32 - bits));
}

void quarter_round(u32& a, u32& b, u32& c, u32& d) noexcept
{
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d, 8);
    c += d; b ^= c; b = rotl32(b, 7);
}

void chacha20_init_state(u32* state, ByteSlice key, u32 counter, ByteSlice nonce) noexcept
{
    const u32 initial[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        load_le32(key.data() + 0),
        load_le32(key.data() + 4),
        load_le32(key.data() + 8),
        load_le32(key.data() + 12),
        load_le32(key.data() + 16),
        load_le32(key.data() + 20),
        load_le32(key.data() + 24),
        load_le32(key.data() + 28),
        counter,
        load_le32(nonce.data() + 0),
        load_le32(nonce.data() + 4),
        load_le32(nonce.data() + 8),
    };
    for (usize i = 0; i < 16; ++i)
        state[i] = initial[i];
}

void chacha20_block_impl(const u32* state, u8* out) noexcept
{
    u32 working[16];
    for (usize i = 0; i < 16; ++i)
        working[i] = state[i];

    for (usize round = 0; round < 10; ++round) {
        quarter_round(working[0], working[4], working[8], working[12]);
        quarter_round(working[1], working[5], working[9], working[13]);
        quarter_round(working[2], working[6], working[10], working[14]);
        quarter_round(working[3], working[7], working[11], working[15]);
        quarter_round(working[0], working[5], working[10], working[15]);
        quarter_round(working[1], working[6], working[11], working[12]);
        quarter_round(working[2], working[7], working[8], working[13]);
        quarter_round(working[3], working[4], working[9], working[14]);
    }

    for (usize i = 0; i < 16; ++i)
        store_le32(out + (i * 4), working[i] + state[i]);
}

}  // namespace

Result<Bytes, CryptoError> chacha20_block(ByteSlice key, u32 counter, ByteSlice nonce)
{
    if (key.size() != CHACHA20_KEY_SIZE || nonce.size() != CHACHA20_NONCE_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    u32 state[16];
    u8 out[CHACHA20_BLOCK_SIZE];
    chacha20_init_state(state, key, counter, nonce);
    chacha20_block_impl(state, out);

    return Ok(Bytes::copy_from_slice(out, CHACHA20_BLOCK_SIZE));
}

Result<Bytes, CryptoError> chacha20_xor(ByteSlice key, u32 counter, ByteSlice nonce, ByteSlice data)
{
    if (key.size() != CHACHA20_KEY_SIZE || nonce.size() != CHACHA20_NONCE_SIZE)
        return Err(CryptoError::INVALID_ARGUMENT);

    u32 state[16];
    u8 block[CHACHA20_BLOCK_SIZE];
    chacha20_init_state(state, key, counter, nonce);

    BytesMut output = BytesMut::with_capacity(data.size());
    usize offset = 0;
    while (offset < data.size()) {
        state[12] = counter;
        chacha20_block_impl(state, block);

        const usize remaining = data.size() - offset;
        const usize take = remaining < CHACHA20_BLOCK_SIZE ? remaining : CHACHA20_BLOCK_SIZE;
        for (usize i = 0; i < take; ++i)
            output.put_u8(static_cast<u8>(data[offset + i] ^ block[i]));

        offset += take;
        ++counter;
    }

    return Ok(output.freeze());
}

}  // namespace ca::crypto
