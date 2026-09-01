#include "libca/crypto/random.hpp"

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <bcrypt.h>
#elif defined(__linux__)
#    include <cerrno>
#    include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#    include <cstdlib>
#else
#    include <random>
#endif

namespace ca::crypto {

using namespace ca;
using namespace ca::core;

Result<Bytes, CryptoError> secure_random_bytes(usize len)
{
    BytesMut output = BytesMut::with_capacity(len);
    for (usize i = 0; i < len; ++i)
        output.put_u8(0);

    if (len == 0)
        return Ok(output.freeze());

#if defined(_WIN32)
    usize offset = 0;
    while (offset < len) {
        const usize chunk  = (len - offset) > 0xFFFFFFFFu ? 0xFFFFFFFFu : (len - offset);
        const auto  status = BCryptGenRandom(nullptr,
                                            output.as_mut_ptr() + offset,
                                            static_cast<ULONG>(chunk),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0)
            return Err(CryptoError::RANDOM_FAILED);
        offset += chunk;
    }
#elif defined(__linux__)
    usize offset = 0;
    while (offset < len) {
        const auto ret = getrandom(output.as_mut_ptr() + offset, len - offset, 0);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            return Err(CryptoError::RANDOM_FAILED);
        }
        offset += static_cast<usize>(ret);
    }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(output.as_mut_ptr(), len);
#else
    std::random_device rd;
    usize              i = 0;
    while (i < len) {
        auto value = rd();
        for (usize j = 0; j < sizeof(value) && i < len; ++j) {
            output.as_mut_ptr()[i++] = static_cast<u8>(value & 0xFF);
            value >>= 8;
        }
    }
#endif

    return Ok(output.freeze());
}

}   // namespace ca::crypto
