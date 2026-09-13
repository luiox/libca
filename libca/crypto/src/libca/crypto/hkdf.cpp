#include "libca/crypto/hkdf.hpp"

#include "libca/crypto/hmac.hpp"
#include "libca/crypto/sha1.hpp"
#include "libca/crypto/sha256.hpp"
#include "libca/crypto/sha512.hpp"

#include <cstring>

namespace ca::crypto {

namespace {

// HMAC 函数指针：expand 公共实现按算法分发到对应 hmac_shaXxx。
using HmacFn = ca::core::Bytes (*)(ca::core::ByteSlice key, ca::core::ByteSlice data);

// HKDF-Expand 公共实现（RFC 5869 2.3）：
//   T(0) = 空串
//   T(i) = HMAC(PRK, T(i-1) | info | i)
//   OKM  = T(1) | T(2) | ... 截取前 length 字节
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_expand_common(
    ca::usize hash_len, HmacFn hmac, ca::core::ByteSlice prk, ca::core::ByteSlice info,
    ca::usize length)
{
    // RFC 5869 2.3 约束：PRK 不少于 HashLen 字节；输出不超过 255 * HashLen 字节。
    if (prk.size() < hash_len)
        return Err(CryptoError::INVALID_ARGUMENT);
    if (length > 255 * hash_len)
        return Err(CryptoError::INVALID_ARGUMENT);

    ca::core::BytesMut okm = ca::core::BytesMut::with_capacity(length);
    ca::u8 t_prev[64] = {};
    ca::core::BytesMut t_input = ca::core::BytesMut::with_capacity(64 + info.size() + 1);

    ca::usize produced = 0;
    ca::u8 counter = 1;
    while (produced < length) {
        t_input.clear();
        if (produced > 0)
            t_input.put_slice(t_prev, hash_len);
        t_input.put_slice(info.data(), info.size());
        t_input.put_u8(counter);

        const auto t = hmac(prk, ca::core::ByteSlice(t_input.as_ptr(), t_input.len()));
        const ca::usize take = (length - produced < hash_len) ? (length - produced) : hash_len;
        okm.put_slice(t.as_ptr(), take);
        std::memcpy(t_prev, t.as_ptr(), hash_len);
        produced += take;
        ++counter;
    }

    return Ok(okm.freeze());
}

}  // namespace

ca::core::Bytes hkdf_sha1_extract(ca::core::ByteSlice salt, ca::core::ByteSlice ikm)
{
    // salt 为空时 HMAC key 补零到块长，与 RFC 5869 2.2 的 HashLen 个 0 盐语义一致。
    return hmac_sha1(salt, ikm);
}

ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha1_expand(
    ca::core::ByteSlice prk, ca::core::ByteSlice info, ca::usize length)
{
    return hkdf_expand_common(SHA1::HashBytes, &hmac_sha1, prk, info, length);
}

ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha1_derive(
    ca::core::ByteSlice salt, ca::core::ByteSlice ikm, ca::core::ByteSlice info, ca::usize length)
{
    const auto prk = hkdf_sha1_extract(salt, ikm);
    return hkdf_sha1_expand(ca::core::ByteSlice(prk.as_ptr(), prk.len()), info, length);
}

ca::core::Bytes hkdf_sha256_extract(ca::core::ByteSlice salt, ca::core::ByteSlice ikm)
{
    return hmac_sha256(salt, ikm);
}

ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha256_expand(
    ca::core::ByteSlice prk, ca::core::ByteSlice info, ca::usize length)
{
    return hkdf_expand_common(SHA256::HashBytes, &hmac_sha256, prk, info, length);
}

ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha256_derive(
    ca::core::ByteSlice salt, ca::core::ByteSlice ikm, ca::core::ByteSlice info, ca::usize length)
{
    const auto prk = hkdf_sha256_extract(salt, ikm);
    return hkdf_sha256_expand(ca::core::ByteSlice(prk.as_ptr(), prk.len()), info, length);
}

ca::core::Bytes hkdf_sha512_extract(ca::core::ByteSlice salt, ca::core::ByteSlice ikm)
{
    return hmac_sha512(salt, ikm);
}

ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha512_expand(
    ca::core::ByteSlice prk, ca::core::ByteSlice info, ca::usize length)
{
    return hkdf_expand_common(SHA512::HashBytes, &hmac_sha512, prk, info, length);
}

ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha512_derive(
    ca::core::ByteSlice salt, ca::core::ByteSlice ikm, ca::core::ByteSlice info, ca::usize length)
{
    const auto prk = hkdf_sha512_extract(salt, ikm);
    return hkdf_sha512_expand(ca::core::ByteSlice(prk.as_ptr(), prk.len()), info, length);
}

}  // namespace ca::crypto
