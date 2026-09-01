#include "libca/crypto/hex.hpp"

#include <cctype>

namespace ca::crypto {

namespace {

ca::i32 from_hex(char ch) noexcept
{
    if (ch >= '0' && ch <= '9')
        return static_cast<ca::i32>(ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return static_cast<ca::i32>(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F')
        return static_cast<ca::i32>(ch - 'A' + 10);
    return -1;
}

}   // namespace

std::string hex_encode(ca::core::ByteSlice data)
{
    static constexpr char HEX[] = "0123456789abcdef";

    std::string output;
    output.reserve(data.size() * 2);
    for (ca::usize i = 0; i < data.size(); ++i) {
        const ca::u8 byte = data[i];
        output.push_back(HEX[(byte >> 4) & 0x0F]);
        output.push_back(HEX[byte & 0x0F]);
    }
    return output;
}

ca::Result<ca::core::Bytes, CryptoError> hex_decode(const std::string& text)
{
    if ((text.size() % 2) != 0)
        return ca::Err(CryptoError::INVALID_HEX);

    ca::core::BytesMut output = ca::core::BytesMut::with_capacity(text.size() / 2);
    for (ca::usize i = 0; i < text.size(); i += 2) {
        const ca::i32 high = from_hex(text[i]);
        const ca::i32 low  = from_hex(text[i + 1]);
        if (high < 0 || low < 0)
            return ca::Err(CryptoError::INVALID_HEX);
        output.put_u8(static_cast<ca::u8>((high << 4) | low));
    }

    return ca::Ok(output.freeze());
}

}   // namespace ca::crypto
