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

void secure_zero(void* data, ca::usize size) noexcept
{
    // volatile 指针逐字节写：防止编译器把「写后即弃」识别为死存储而消除。
    // 供密钥材料（HMAC key_block、ChaCha20 state、RC4 S 盒）离开作用域前清零，
    // 避免密钥残留栈帧被后续栈复用或进程转储读出。
    volatile ca::u8* p = static_cast<volatile ca::u8*>(data);
    for (ca::usize i = 0; i < size; ++i)
        p[i] = 0;
}

}   // namespace ca::crypto
