#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

namespace ca::crypto {

/// @brief 生成系统安全随机字节。
/// @param len 需要生成的字节数。
/// @return 成功返回随机字节；系统 RNG 失败时返回 RANDOM_FAILED。
ca::Result<ca::core::Bytes, CryptoError> secure_random_bytes(ca::usize len);

}   // namespace ca::crypto
