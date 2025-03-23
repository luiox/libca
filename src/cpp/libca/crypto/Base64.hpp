#ifndef LIBCA_CRYPTO_BASE64_HPP
#define LIBCA_CRYPTO_BASE64_HPP

#include <string>
#include <vector>

namespace ca {

std::string Base64Encode(const char* src, size_t len);

std::vector<char> Base64Decode(const std::string& src);

}

#endif // !LIBCA_CRYPTO_BASE64_HPP
