#include "libca/crypto/pbkdf2.hpp"

#include "libca/crypto/hmac.hpp"
#include "libca/crypto/sha256.hpp"

#include <cstring>

namespace ca::crypto {

ca::core::Result<ca::core::Bytes, CryptoError> pbkdf2_hmac_sha256(
    ca::core::ByteSlice password, ca::core::ByteSlice salt, ca::usize iterations,
    ca::usize dk_len)
{
    // 策略：iterations 与 dk_len 都必须是正整数（RFC 2898 5.2），
    // 0 直接判参数错误，不静默回退默认值。
    if (iterations == 0)
        return Err(CryptoError::INVALID_ARGUMENT);
    if (dk_len == 0)
        return Err(CryptoError::INVALID_ARGUMENT);

    constexpr ca::usize HASH_LEN = SHA256::HashBytes;
    const ca::usize block_count = (dk_len + HASH_LEN - 1) / HASH_LEN;

    ca::core::BytesMut dk = ca::core::BytesMut::with_capacity(dk_len);
    ca::core::BytesMut u1_input = ca::core::BytesMut::with_capacity(salt.size() + 4);

    for (ca::usize block = 1; block <= block_count; ++block) {
        // U1 = PRF(P, S || INT_32_BE(i))
        u1_input.clear();
        u1_input.put_slice(salt.data(), salt.size());
        u1_input.put_u32_be(static_cast<ca::u32>(block));

        const auto u1 = hmac_sha256(password, ca::core::ByteSlice(u1_input.as_ptr(), u1_input.len()));
        // u 为上一轮 U（链式输入），t 为 XOR 累加器，二者必须分开维护：
        // U(i) = PRF(P, U(i-1))，T(i) = U1 ^ U2 ^ ... ^ Ui。
        ca::u8 u[HASH_LEN];
        ca::u8 t[HASH_LEN];
        std::memcpy(u, u1.as_ptr(), HASH_LEN);
        std::memcpy(t, u1.as_ptr(), HASH_LEN);

        for (ca::usize iter = 1; iter < iterations; ++iter) {
            const auto un = hmac_sha256(password, ca::core::ByteSlice(u, HASH_LEN));
            std::memcpy(u, un.as_ptr(), HASH_LEN);
            for (ca::usize j = 0; j < HASH_LEN; ++j)
                t[j] ^= u[j];
        }

        // 末块截断：DK = T1 || T2 || ... 取前 dk_len 字节。
        const ca::usize remaining = dk_len - dk.len();
        const ca::usize take = (remaining < HASH_LEN) ? remaining : HASH_LEN;
        dk.put_slice(t, take);
    }

    return Ok(dk.freeze());
}

}  // namespace ca::crypto
