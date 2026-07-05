#include "libca/crypto/sha256.hpp"

#include <cstdint>
#include <cstring>

namespace ca::crypto {

namespace {

inline uint32_t rotate(uint32_t a, uint32_t c) {
    return (a >> (c & 0x1F)) | (a << ((32 - (c & 0x1F)) & 0x1F));
}

inline uint32_t byteswap(uint32_t x) {
#if defined(_MSC_VER)
    return _byteswap_ulong(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
#else
    return (x >> 24) |
          ((x >>  8) & 0x0000FF00) |
          ((x <<  8) & 0x00FF0000) |
           (x << 24);
#endif
}

inline uint32_t f1(uint32_t e, uint32_t f, uint32_t g) {
    uint32_t term1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
    uint32_t term2 = (e & f) ^ (~e & g);
    return term1 + term2;
}

inline uint32_t f2(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t term1 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
    uint32_t term2 = ((a | b) & c) | (a & b);
    return term1 + term2;
}

}

SHA256::SHA256() { reset(); }

void SHA256::reset() {
    num_bytes_ = 0;
    buffer_size_ = 0;
    hash_[0] = 0x6a09e667;
    hash_[1] = 0xbb67ae85;
    hash_[2] = 0x3c6ef372;
    hash_[3] = 0xa54ff53a;
    hash_[4] = 0x510e527f;
    hash_[5] = 0x9b05688c;
    hash_[6] = 0x1f83d9ab;
    hash_[7] = 0x5be0cd19;
}

void SHA256::processBlock(const void* data) {
    uint32_t a = hash_[0];
    uint32_t b = hash_[1];
    uint32_t c = hash_[2];
    uint32_t d = hash_[3];
    uint32_t e = hash_[4];
    uint32_t f = hash_[5];
    uint32_t g = hash_[6];
    uint32_t h = hash_[7];

    uint32_t words[64];
    std::memcpy(words, data, 64);

    for (int i = 0; i < 16; i++)
        words[i] = byteswap(words[i]);

    for (int i = 16; i < 64; i++)
        words[i] = words[i-16]
                 + (rotate(words[i-15], 7) ^ rotate(words[i-15], 18) ^ (words[i-15] >> 3))
                 + words[i-7]
                 + (rotate(words[i-2], 17) ^ rotate(words[i-2], 19) ^ (words[i-2] >> 10));

    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    for (int i = 0; i < 64; i++) {
        uint32_t temp1 = h + f1(e, f, g) + K[i] + words[i];
        uint32_t temp2 = f2(a, b, c);
        d += temp1;
        h = temp1 + temp2;
        uint32_t tmp = a;
        a = h;
        h = g;
        g = f;
        f = e;
        e = d;
        d = c;
        c = b;
        b = tmp;
    }

    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
    hash_[4] += e;
    hash_[5] += f;
    hash_[6] += g;
    hash_[7] += h;
}

void SHA256::add(const void* data, size_t numBytes) {
    const uint8_t* current = static_cast<const uint8_t*>(data);

    if (buffer_size_ > 0) {
        while (numBytes > 0 && buffer_size_ < BlockSize) {
            buffer_[buffer_size_++] = *current++;
            numBytes--;
        }
    }

    if (buffer_size_ == BlockSize) {
        processBlock(buffer_);
        num_bytes_ += BlockSize;
        buffer_size_ = 0;
    }

    if (numBytes == 0) return;

    while (numBytes >= BlockSize) {
        processBlock(current);
        current += BlockSize;
        num_bytes_ += BlockSize;
        numBytes -= BlockSize;
    }

    while (numBytes > 0) {
        buffer_[buffer_size_++] = *current++;
        numBytes--;
    }
}

void SHA256::processBuffer() {
    size_t paddedLength = buffer_size_ * 8;
    paddedLength++;

    size_t lower11Bits = paddedLength & 511;
    if (lower11Bits <= 448)
        paddedLength += 448 - lower11Bits;
    else
        paddedLength += 512 + 448 - lower11Bits;
    paddedLength /= 8;

    unsigned char extra[BlockSize];

    if (buffer_size_ < BlockSize)
        buffer_[buffer_size_] = 128;
    else
        extra[0] = 128;

    size_t i;
    for (i = buffer_size_ + 1; i < BlockSize; i++)
        buffer_[i] = 0;
    for (; i < paddedLength; i++)
        extra[i - BlockSize] = 0;

    uint64_t msgBits = 8 * (num_bytes_ + buffer_size_);
    unsigned char* addLength;
    if (paddedLength < BlockSize)
        addLength = buffer_ + paddedLength;
    else
        addLength = extra + paddedLength - BlockSize;

    *addLength++ = static_cast<unsigned char>((msgBits >> 56) & 0xFF);
    *addLength++ = static_cast<unsigned char>((msgBits >> 48) & 0xFF);
    *addLength++ = static_cast<unsigned char>((msgBits >> 40) & 0xFF);
    *addLength++ = static_cast<unsigned char>((msgBits >> 32) & 0xFF);
    *addLength++ = static_cast<unsigned char>((msgBits >> 24) & 0xFF);
    *addLength++ = static_cast<unsigned char>((msgBits >> 16) & 0xFF);
    *addLength++ = static_cast<unsigned char>((msgBits >>  8) & 0xFF);
    *addLength   = static_cast<unsigned char>( msgBits        & 0xFF);

    processBlock(buffer_);
    if (paddedLength > BlockSize)
        processBlock(extra);
}

std::string SHA256::getHash() {
    unsigned char rawHash[HashBytes];
    getHash(rawHash);

    std::string result;
    result.reserve(2 * HashBytes);
    for (int i = 0; i < HashBytes; i++) {
        static const char dec2hex[17] = "0123456789abcdef";
        result += dec2hex[(rawHash[i] >> 4) & 15];
        result += dec2hex[ rawHash[i]       & 15];
    }
    return result;
}

void SHA256::getHash(unsigned char buffer[HashBytes]) {
    uint32_t oldHash[HashValues];
    for (int i = 0; i < HashValues; i++)
        oldHash[i] = hash_[i];

    processBuffer();

    unsigned char* current = buffer;
    for (int i = 0; i < HashValues; i++) {
        *current++ = static_cast<unsigned char>((hash_[i] >> 24) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 16) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >>  8) & 0xFF);
        *current++ = static_cast<unsigned char>( hash_[i]        & 0xFF);
        hash_[i] = oldHash[i];
    }
}

std::string SHA256::operator()(const void* data, size_t numBytes) {
    reset();
    add(data, numBytes);
    return getHash();
}

std::string SHA256::operator()(const std::string& text) {
    reset();
    add(text.c_str(), text.size());
    return getHash();
}

ca::core::Bytes sha256(ca::core::ByteSlice data)
{
    SHA256 hasher;
    hasher.add(data.data(), data.size());

    ca::u8 digest[SHA256::HashBytes];
    hasher.getHash(digest);
    return ca::core::Bytes::copy_from_slice(digest, SHA256::HashBytes);
}

std::string sha256_hex(ca::core::ByteSlice data)
{
    SHA256 hasher;
    hasher.add(data.data(), data.size());
    return hasher.getHash();
}

}
