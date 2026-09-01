#include "libca/crypto/rc4.hpp"

#include "libca/crypto/crypto_util.hpp"

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

Result<Bytes, CryptoError> rc4_crypt(ByteSlice key, ByteSlice data)
{
    if (key.size() == 0 || key.size() > 256)
        return Err(CryptoError::INVALID_ARGUMENT);

    u8 state[256];
    for (usize i = 0; i < 256; ++i)
        state[i] = static_cast<u8>(i);

    u8 j = 0;
    for (usize i = 0; i < 256; ++i) {
        j            = static_cast<u8>(j + state[i] + key[i % key.size()]);
        const u8 tmp = state[i];
        state[i]     = state[j];
        state[j]     = tmp;
    }

    BytesMut output = BytesMut::with_capacity(data.size());
    u8       i      = 0;
    j               = 0;
    for (usize n = 0; n < data.size(); ++n) {
        i = static_cast<u8>(i + 1);
        j = static_cast<u8>(j + state[i]);

        const u8 tmp = state[i];
        state[i]     = state[j];
        state[j]     = tmp;

        const u8 k = state[static_cast<u8>(state[i] + state[j])];
        output.put_u8(static_cast<u8>(data[n] ^ k));
    }

    secure_zero(state, sizeof(state));   // KSA 后的 S 盒与密钥等价
    return Ok(output.freeze());
}

}   // namespace ca::crypto
