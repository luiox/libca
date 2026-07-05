#include "libca/crypto/hmac.hpp"

#include "libca/crypto/hex.hpp"
#include "libca/crypto/sha256.hpp"

namespace ca::crypto {

namespace {

constexpr ca::usize SHA256_BLOCK_SIZE = 64;

}  // namespace

ca::core::Bytes hmac_sha256(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    ca::u8 key_block[SHA256_BLOCK_SIZE] = {};

    if (key.size() > SHA256_BLOCK_SIZE) {
        const auto hashed_key = sha256(key);
        for (ca::usize i = 0; i < hashed_key.len(); ++i)
            key_block[i] = hashed_key.as_ptr()[i];
    } else {
        for (ca::usize i = 0; i < key.size(); ++i)
            key_block[i] = key[i];
    }

    ca::core::BytesMut inner = ca::core::BytesMut::with_capacity(SHA256_BLOCK_SIZE + data.size());
    ca::core::BytesMut outer = ca::core::BytesMut::with_capacity(SHA256_BLOCK_SIZE + SHA256::HashBytes);

    for (ca::usize i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        inner.put_u8(static_cast<ca::u8>(key_block[i] ^ 0x36));
        outer.put_u8(static_cast<ca::u8>(key_block[i] ^ 0x5c));
    }
    inner.put_slice(data.data(), data.size());

    const auto inner_digest = sha256(ca::core::ByteSlice(inner.as_ptr(), inner.len()));
    outer.put_slice(inner_digest.as_ptr(), inner_digest.len());

    return sha256(ca::core::ByteSlice(outer.as_ptr(), outer.len()));
}

std::string hmac_sha256_hex(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    const auto digest = hmac_sha256(key, data);
    return hex_encode(ca::core::ByteSlice(digest.as_ptr(), digest.len()));
}

}  // namespace ca::crypto
