#include "crypto/sha1.hpp"

#include <cstdint>
#include <cstring>

namespace ca::crypto {

namespace {

inline uint32_t f1(uint32_t b, uint32_t c, uint32_t d) {
    return d ^ (b & (c ^ d));
}

inline uint32_t f2(uint32_t b, uint32_t c, uint32_t d) {
    return b ^ c ^ d;
}

inline uint32_t f3(uint32_t b, uint32_t c, uint32_t d) {
    return (b & c) | (b & d) | (c & d);
}

inline uint32_t rotate(uint32_t a, uint32_t c) {
    return (a << (c & 0x1F)) | (a >> ((32 - (c & 0x1F)) & 0x1F));
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

}

SHA1::SHA1() { reset(); }

void SHA1::reset() {
    num_bytes_ = 0;
    buffer_size_ = 0;
    hash_[0] = 0x67452301;
    hash_[1] = 0xefcdab89;
    hash_[2] = 0x98badcfe;
    hash_[3] = 0x10325476;
    hash_[4] = 0xc3d2e1f0;
}

void SHA1::processBlock(const void* data) {
    uint32_t a = hash_[0];
    uint32_t b = hash_[1];
    uint32_t c = hash_[2];
    uint32_t d = hash_[3];
    uint32_t e = hash_[4];

    uint32_t words[80];
    std::memcpy(words, data, 64);
    for (int i = 0; i < 16; i++) {
        words[i] = byteswap(words[i]);
    }
    for (int i = 16; i < 80; i++) {
        words[i] = rotate(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);
    }

    for (int i = 0; i < 4; i++) {
        int offset = 5 * i;
        e += rotate(a, 5) + f1(b, c, d) + words[offset  ] + 0x5a827999; b = rotate(b, 30);
        d += rotate(e, 5) + f1(a, b, c) + words[offset+1] + 0x5a827999; a = rotate(a, 30);
        c += rotate(d, 5) + f1(e, a, b) + words[offset+2] + 0x5a827999; e = rotate(e, 30);
        b += rotate(c, 5) + f1(d, e, a) + words[offset+3] + 0x5a827999; d = rotate(d, 30);
        a += rotate(b, 5) + f1(c, d, e) + words[offset+4] + 0x5a827999; c = rotate(c, 30);
    }

    for (int i = 4; i < 8; i++) {
        int offset = 5 * i;
        e += rotate(a, 5) + f2(b, c, d) + words[offset  ] + 0x6ed9eba1; b = rotate(b, 30);
        d += rotate(e, 5) + f2(a, b, c) + words[offset+1] + 0x6ed9eba1; a = rotate(a, 30);
        c += rotate(d, 5) + f2(e, a, b) + words[offset+2] + 0x6ed9eba1; e = rotate(e, 30);
        b += rotate(c, 5) + f2(d, e, a) + words[offset+3] + 0x6ed9eba1; d = rotate(d, 30);
        a += rotate(b, 5) + f2(c, d, e) + words[offset+4] + 0x6ed9eba1; c = rotate(c, 30);
    }

    for (int i = 8; i < 12; i++) {
        int offset = 5 * i;
        e += rotate(a, 5) + f3(b, c, d) + words[offset  ] + 0x8f1bbcdc; b = rotate(b, 30);
        d += rotate(e, 5) + f3(a, b, c) + words[offset+1] + 0x8f1bbcdc; a = rotate(a, 30);
        c += rotate(d, 5) + f3(e, a, b) + words[offset+2] + 0x8f1bbcdc; e = rotate(e, 30);
        b += rotate(c, 5) + f3(d, e, a) + words[offset+3] + 0x8f1bbcdc; d = rotate(d, 30);
        a += rotate(b, 5) + f3(c, d, e) + words[offset+4] + 0x8f1bbcdc; c = rotate(c, 30);
    }

    for (int i = 12; i < 16; i++) {
        int offset = 5 * i;
        e += rotate(a, 5) + f2(b, c, d) + words[offset  ] + 0xca62c1d6; b = rotate(b, 30);
        d += rotate(e, 5) + f2(a, b, c) + words[offset+1] + 0xca62c1d6; a = rotate(a, 30);
        c += rotate(d, 5) + f2(e, a, b) + words[offset+2] + 0xca62c1d6; e = rotate(e, 30);
        b += rotate(c, 5) + f2(d, e, a) + words[offset+3] + 0xca62c1d6; d = rotate(d, 30);
        a += rotate(b, 5) + f2(c, d, e) + words[offset+4] + 0xca62c1d6; c = rotate(c, 30);
    }

    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
    hash_[4] += e;
}

void SHA1::add(const void* data, size_t numBytes) {
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

void SHA1::processBuffer() {
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

std::string SHA1::getHash() {
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

void SHA1::getHash(unsigned char buffer[HashBytes]) {
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

std::string SHA1::operator()(const void* data, size_t numBytes) {
    reset();
    add(data, numBytes);
    return getHash();
}

std::string SHA1::operator()(const std::string& text) {
    reset();
    add(text.c_str(), text.size());
    return getHash();
}

}
