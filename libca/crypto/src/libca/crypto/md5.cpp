#include "crypto/md5.hpp"

#include <cstdint>
#include <cstring>

namespace ca::crypto {

namespace {

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

inline uint32_t fromLittleEndian(uint32_t x) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return byteswap(x);
#else
    return x;
#endif
}

inline uint32_t rotate(uint32_t a, uint32_t c) {
    return (a << (c & 0x1F)) | (a >> ((32 - (c & 0x1F)) & 0x1F));
}

inline uint32_t f1(uint32_t b, uint32_t c, uint32_t d) {
    return d ^ (b & (c ^ d));
}

inline uint32_t f2(uint32_t b, uint32_t c, uint32_t d) {
    return c ^ (d & (b ^ c));
}

inline uint32_t f3(uint32_t b, uint32_t c, uint32_t d) {
    return b ^ c ^ d;
}

inline uint32_t f4(uint32_t b, uint32_t c, uint32_t d) {
    return c ^ (b | ~d);
}

}

MD5::MD5() { reset(); }

void MD5::reset() {
    num_bytes_ = 0;
    buffer_size_ = 0;
    hash_[0] = 0x67452301;
    hash_[1] = 0xefcdab89;
    hash_[2] = 0x98badcfe;
    hash_[3] = 0x10325476;
}

void MD5::processBlock(const void* data) {
    uint32_t a = hash_[0];
    uint32_t b = hash_[1];
    uint32_t c = hash_[2];
    uint32_t d = hash_[3];

    uint32_t words[16];
    std::memcpy(words, data, 64);
    for (int i = 0; i < 16; i++)
        words[i] = fromLittleEndian(words[i]);

    // first round
    a = rotate(a + f1(b,c,d) + words[ 0] + 0xd76aa478,  7) + b;
    d = rotate(d + f1(a,b,c) + words[ 1] + 0xe8c7b756, 12) + a;
    c = rotate(c + f1(d,a,b) + words[ 2] + 0x242070db, 17) + d;
    b = rotate(b + f1(c,d,a) + words[ 3] + 0xc1bdceee, 22) + c;
    a = rotate(a + f1(b,c,d) + words[ 4] + 0xf57c0faf,  7) + b;
    d = rotate(d + f1(a,b,c) + words[ 5] + 0x4787c62a, 12) + a;
    c = rotate(c + f1(d,a,b) + words[ 6] + 0xa8304613, 17) + d;
    b = rotate(b + f1(c,d,a) + words[ 7] + 0xfd469501, 22) + c;
    a = rotate(a + f1(b,c,d) + words[ 8] + 0x698098d8,  7) + b;
    d = rotate(d + f1(a,b,c) + words[ 9] + 0x8b44f7af, 12) + a;
    c = rotate(c + f1(d,a,b) + words[10] + 0xffff5bb1, 17) + d;
    b = rotate(b + f1(c,d,a) + words[11] + 0x895cd7be, 22) + c;
    a = rotate(a + f1(b,c,d) + words[12] + 0x6b901122,  7) + b;
    d = rotate(d + f1(a,b,c) + words[13] + 0xfd987193, 12) + a;
    c = rotate(c + f1(d,a,b) + words[14] + 0xa679438e, 17) + d;
    b = rotate(b + f1(c,d,a) + words[15] + 0x49b40821, 22) + c;

    // second round
    a = rotate(a + f2(b,c,d) + words[ 1] + 0xf61e2562,  5) + b;
    d = rotate(d + f2(a,b,c) + words[ 6] + 0xc040b340,  9) + a;
    c = rotate(c + f2(d,a,b) + words[11] + 0x265e5a51, 14) + d;
    b = rotate(b + f2(c,d,a) + words[ 0] + 0xe9b6c7aa, 20) + c;
    a = rotate(a + f2(b,c,d) + words[ 5] + 0xd62f105d,  5) + b;
    d = rotate(d + f2(a,b,c) + words[10] + 0x02441453,  9) + a;
    c = rotate(c + f2(d,a,b) + words[15] + 0xd8a1e681, 14) + d;
    b = rotate(b + f2(c,d,a) + words[ 4] + 0xe7d3fbc8, 20) + c;
    a = rotate(a + f2(b,c,d) + words[ 9] + 0x21e1cde6,  5) + b;
    d = rotate(d + f2(a,b,c) + words[14] + 0xc33707d6,  9) + a;
    c = rotate(c + f2(d,a,b) + words[ 3] + 0xf4d50d87, 14) + d;
    b = rotate(b + f2(c,d,a) + words[ 8] + 0x455a14ed, 20) + c;
    a = rotate(a + f2(b,c,d) + words[13] + 0xa9e3e905,  5) + b;
    d = rotate(d + f2(a,b,c) + words[ 2] + 0xfcefa3f8,  9) + a;
    c = rotate(c + f2(d,a,b) + words[ 7] + 0x676f02d9, 14) + d;
    b = rotate(b + f2(c,d,a) + words[12] + 0x8d2a4c8a, 20) + c;

    // third round
    a = rotate(a + f3(b,c,d) + words[ 5] + 0xfffa3942,  4) + b;
    d = rotate(d + f3(a,b,c) + words[ 8] + 0x8771f681, 11) + a;
    c = rotate(c + f3(d,a,b) + words[11] + 0x6d9d6122, 16) + d;
    b = rotate(b + f3(c,d,a) + words[14] + 0xfde5380c, 23) + c;
    a = rotate(a + f3(b,c,d) + words[ 1] + 0xa4beea44,  4) + b;
    d = rotate(d + f3(a,b,c) + words[ 4] + 0x4bdecfa9, 11) + a;
    c = rotate(c + f3(d,a,b) + words[ 7] + 0xf6bb4b60, 16) + d;
    b = rotate(b + f3(c,d,a) + words[10] + 0xbebfbc70, 23) + c;
    a = rotate(a + f3(b,c,d) + words[13] + 0x289b7ec6,  4) + b;
    d = rotate(d + f3(a,b,c) + words[ 0] + 0xeaa127fa, 11) + a;
    c = rotate(c + f3(d,a,b) + words[ 3] + 0xd4ef3085, 16) + d;
    b = rotate(b + f3(c,d,a) + words[ 6] + 0x04881d05, 23) + c;
    a = rotate(a + f3(b,c,d) + words[ 9] + 0xd9d4d039,  4) + b;
    d = rotate(d + f3(a,b,c) + words[12] + 0xe6db99e5, 11) + a;
    c = rotate(c + f3(d,a,b) + words[15] + 0x1fa27cf8, 16) + d;
    b = rotate(b + f3(c,d,a) + words[ 2] + 0xc4ac5665, 23) + c;

    // fourth round
    a = rotate(a + f4(b,c,d) + words[ 0] + 0xf4292244,  6) + b;
    d = rotate(d + f4(a,b,c) + words[ 7] + 0x432aff97, 10) + a;
    c = rotate(c + f4(d,a,b) + words[14] + 0xab9423a7, 15) + d;
    b = rotate(b + f4(c,d,a) + words[ 5] + 0xfc93a039, 21) + c;
    a = rotate(a + f4(b,c,d) + words[12] + 0x655b59c3,  6) + b;
    d = rotate(d + f4(a,b,c) + words[ 3] + 0x8f0ccc92, 10) + a;
    c = rotate(c + f4(d,a,b) + words[10] + 0xffeff47d, 15) + d;
    b = rotate(b + f4(c,d,a) + words[ 1] + 0x85845dd1, 21) + c;
    a = rotate(a + f4(b,c,d) + words[ 8] + 0x6fa87e4f,  6) + b;
    d = rotate(d + f4(a,b,c) + words[15] + 0xfe2ce6e0, 10) + a;
    c = rotate(c + f4(d,a,b) + words[ 6] + 0xa3014314, 15) + d;
    b = rotate(b + f4(c,d,a) + words[13] + 0x4e0811a1, 21) + c;
    a = rotate(a + f4(b,c,d) + words[ 4] + 0xf7537e82,  6) + b;
    d = rotate(d + f4(a,b,c) + words[11] + 0xbd3af235, 10) + a;
    c = rotate(c + f4(d,a,b) + words[ 2] + 0x2ad7d2bb, 15) + d;
    b = rotate(b + f4(c,d,a) + words[ 9] + 0xeb86d391, 21) + c;

    hash_[0] += a;
    hash_[1] += b;
    hash_[2] += c;
    hash_[3] += d;
}

void MD5::add(const void* data, size_t numBytes) {
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

void MD5::processBuffer() {
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

    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF); msgBits >>= 8;
    *addLength++ = static_cast<unsigned char>( msgBits        & 0xFF);

    processBlock(buffer_);
    if (paddedLength > BlockSize)
        processBlock(extra);
}

std::string MD5::getHash() {
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

void MD5::getHash(unsigned char buffer[HashBytes]) {
    uint32_t oldHash[HashValues];
    for (int i = 0; i < HashValues; i++)
        oldHash[i] = hash_[i];

    processBuffer();

    unsigned char* current = buffer;
    for (int i = 0; i < HashValues; i++) {
        *current++ = static_cast<unsigned char>( hash_[i]        & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >>  8) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 16) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 24) & 0xFF);
        hash_[i] = oldHash[i];
    }
}

std::string MD5::operator()(const void* data, size_t numBytes) {
    reset();
    add(data, numBytes);
    return getHash();
}

std::string MD5::operator()(const std::string& text) {
    reset();
    add(text.c_str(), text.size());
    return getHash();
}

}
