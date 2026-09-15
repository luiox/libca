#include "libca/crypto/hmac.hpp"

#include "libca/crypto/crypto_util.hpp"
#include "libca/crypto/hex.hpp"
#include "libca/crypto/sha1.hpp"
#include "libca/crypto/sha256.hpp"
#include "libca/crypto/sha512.hpp"

namespace ca::crypto {

namespace {

// 通用 HMAC（RFC 2104）：HashT 须为 SHA1/SHA256/SHA512 之一，
// 三者暴露同构的 BlockSize/HashBytes 常量与 add()/get_hash(unsigned char*) 接口。
template<typename HashT>
ca::core::Bytes hmac_impl(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    constexpr ca::usize BLOCK_SIZE = HashT::BlockSize;
    constexpr ca::usize DIGEST_SIZE = HashT::HashBytes;

    ca::u8 key_block[BLOCK_SIZE] = {};

    if (key.size() > BLOCK_SIZE) {
        // 超块长 key：先哈希再补零（RFC 2104 6.3）。
        HashT key_hasher;
        key_hasher.add(key.data(), key.size());
        ca::u8 hashed_key[DIGEST_SIZE];
        key_hasher.get_hash(hashed_key);
        for (ca::usize i = 0; i < DIGEST_SIZE; ++i)
            key_block[i] = hashed_key[i];
        // 密钥哈希与密钥等价（HMAC 语义下可直接充当密钥），同样属于敏感材料。
        secure_zero(hashed_key, sizeof(hashed_key));
    } else {
        for (ca::usize i = 0; i < key.size(); ++i)
            key_block[i] = key[i];
    }

    ca::core::BytesMut inner = ca::core::BytesMut::with_capacity(BLOCK_SIZE + data.size());
    ca::core::BytesMut outer = ca::core::BytesMut::with_capacity(BLOCK_SIZE + DIGEST_SIZE);

    for (ca::usize i = 0; i < BLOCK_SIZE; ++i) {
        inner.put_u8(static_cast<ca::u8>(key_block[i] ^ 0x36));
        outer.put_u8(static_cast<ca::u8>(key_block[i] ^ 0x5c));
    }
    inner.put_slice(data.data(), data.size());

    HashT inner_hasher;
    inner_hasher.add(inner.as_ptr(), inner.len());
    ca::u8 inner_digest[DIGEST_SIZE];
    inner_hasher.get_hash(inner_digest);
    outer.put_slice(inner_digest, DIGEST_SIZE);

    HashT outer_hasher;
    outer_hasher.add(outer.as_ptr(), outer.len());
    ca::u8 result[DIGEST_SIZE];
    outer_hasher.get_hash(result);

    // 密钥派生材料离场清零：key_block、inner_digest 与 ipad/opad 缓冲
    // （inner/outer 为本函数独占缓冲，fill 覆盖全部已写字节且抗死存储消除）。
    secure_zero(key_block, sizeof(key_block));
    secure_zero(inner_digest, sizeof(inner_digest));
    inner.fill(0);
    outer.fill(0);
    return ca::core::Bytes::copy_from_slice(result, DIGEST_SIZE);
}

}  // namespace

ca::core::Bytes hmac_sha1(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    return hmac_impl<SHA1>(key, data);
}

std::string hmac_sha1_hex(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    const auto digest = hmac_sha1(key, data);
    return hex_encode(ca::core::ByteSlice(digest.as_ptr(), digest.len()));
}

ca::core::Bytes hmac_sha256(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    return hmac_impl<SHA256>(key, data);
}

std::string hmac_sha256_hex(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    const auto digest = hmac_sha256(key, data);
    return hex_encode(ca::core::ByteSlice(digest.as_ptr(), digest.len()));
}

ca::core::Bytes hmac_sha512(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    return hmac_impl<SHA512>(key, data);
}

std::string hmac_sha512_hex(ca::core::ByteSlice key, ca::core::ByteSlice data)
{
    const auto digest = hmac_sha512(key, data);
    return hex_encode(ca::core::ByteSlice(digest.as_ptr(), digest.len()));
}

}  // namespace ca::crypto
