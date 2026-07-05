#include "libca/crypto/crypto_util.hpp"

namespace ca::crypto {

bool constant_time_eq(ca::core::ByteSlice lhs, ca::core::ByteSlice rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;

    ca::u8 diff = 0;
    for (ca::usize i = 0; i < lhs.size(); ++i)
        diff |= static_cast<ca::u8>(lhs[i] ^ rhs[i]);
    return diff == 0;
}

}  // namespace ca::crypto
