#include "libca/crypto/sha512.hpp"

#include <cstdint>
#include <cstring>

namespace ca::crypto {

namespace {

inline uint64_t rotate64(uint64_t a, int c) {
    return (a >> c) | (a << (64 - c));
}

inline uint64_t byteswap64(uint64_t x) {
#if defined(_MSC_VER)
    return _byteswap_uint64(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#else
    return (x >> 56) |
          ((x >> 40) & 0x000000000000FF00ULL) |
          ((x >> 24) & 0x0000000000FF0000ULL) |
          ((x >>  8) & 0x00000000FF000000ULL) |
          ((x <<  8) & 0x000000FF00000000ULL) |
          ((x << 24) & 0x0000FF0000000000ULL) |
          ((x << 40) & 0x00FF000000000000ULL) |
           (x << 56);
#endif
}

inline uint64_t sigma1(uint64_t e, uint64_t f, uint64_t g) {
    uint64_t term1 = rotate64(e, 14) ^ rotate64(e, 18) ^ rotate64(e, 41);
    uint64_t term2 = (e & f) ^ (~e & g);
    return term1 + term2;
}

inline uint64_t sigma0(uint64_t a, uint64_t b, uint64_t c) {
    uint64_t term1 = rotate64(a, 28) ^ rotate64(a, 34) ^ rotate64(a, 39);
    uint64_t term2 = ((a | b) & c) | (a & b);
    return term1 + term2;
}

// K 常量：前 80 个素数立方根小数部分前 64 位（FIPS 180-4 4.2.3）。
const uint64_t SHA512_K[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

// SHA-512/384 共用的压缩函数：对 hash[8] 就地吸收一个 128 字节分组。
void sha512_compress(uint64_t hash[8], const void* data) {
    uint64_t a = hash[0];
    uint64_t b = hash[1];
    uint64_t c = hash[2];
    uint64_t d = hash[3];
    uint64_t e = hash[4];
    uint64_t f = hash[5];
    uint64_t g = hash[6];
    uint64_t h = hash[7];

    uint64_t words[80];
    const uint8_t* input = static_cast<const uint8_t*>(data);
    for (int i = 0; i < 16; i++) {
        uint64_t w = 0;
        for (int j = 0; j < 8; j++)
            w = (w << 8) | input[i * 8 + j];
        words[i] = w;
    }

    for (int i = 16; i < 80; i++) {
        uint64_t s0 = rotate64(words[i-15], 1) ^ rotate64(words[i-15], 8) ^ (words[i-15] >> 7);
        uint64_t s1 = rotate64(words[i-2], 19) ^ rotate64(words[i-2], 61) ^ (words[i-2] >> 6);
        words[i] = words[i-16] + s0 + words[i-7] + s1;
    }

    for (int i = 0; i < 80; i++) {
        uint64_t temp1 = h + sigma1(e, f, g) + SHA512_K[i] + words[i];
        uint64_t temp2 = sigma0(a, b, c);
        d += temp1;
        h = temp1 + temp2;
        uint64_t tmp = a;
        a = h;
        h = g;
        g = f;
        f = e;
        e = d;
        d = c;
        c = b;
        b = tmp;
    }

    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
}

}  // namespace

SHA512::SHA512() { reset(); }

void SHA512::reset() {
    num_bytes_ = 0;
    buffer_size_ = 0;
    hash_[0] = 0x6a09e667f3bcc908;
    hash_[1] = 0xbb67ae8584caa73b;
    hash_[2] = 0x3c6ef372fe94f82b;
    hash_[3] = 0xa54ff53a5f1d36f1;
    hash_[4] = 0x510e527fade682d1;
    hash_[5] = 0x9b05688c2b3e6c1f;
    hash_[6] = 0x1f83d9abfb41bd6b;
    hash_[7] = 0x5be0cd19137e2179;
}

void SHA384::reset() {
    // SHA-384 IV = 前 16 个素数平方根小数部分前 64 位（FIPS 180-4 4.3.3）。
    num_bytes_ = 0;
    buffer_size_ = 0;
    hash_[0] = 0xcbbb9d5dc1059ed8;
    hash_[1] = 0x629a292a367cd507;
    hash_[2] = 0x9159015a3070dd17;
    hash_[3] = 0x152fecd8f70e5939;
    hash_[4] = 0x67332667ffc00b31;
    hash_[5] = 0x8eb44a8768581511;
    hash_[6] = 0xdb0c2e0d64f98fa7;
    hash_[7] = 0x47b5481dbefa4fa4;
}

SHA384::SHA384() { reset(); }

void SHA512::process_block(const void* data) {
    sha512_compress(hash_, data);
}

void SHA384::process_block(const void* data) {
    sha512_compress(hash_, data);
}

void SHA512::add(const void* data, size_t num_bytes) {
    const uint8_t* current = static_cast<const uint8_t*>(data);

    if (buffer_size_ > 0) {
        while (num_bytes > 0 && buffer_size_ < BlockSize) {
            buffer_[buffer_size_++] = *current++;
            num_bytes--;
        }
    }

    if (buffer_size_ == BlockSize) {
        process_block(buffer_);
        num_bytes_ += BlockSize;
        buffer_size_ = 0;
    }

    if (num_bytes == 0) return;

    while (num_bytes >= BlockSize) {
        process_block(current);
        current += BlockSize;
        num_bytes_ += BlockSize;
        num_bytes -= BlockSize;
    }

    while (num_bytes > 0) {
        buffer_[buffer_size_++] = *current++;
        num_bytes--;
    }
}

void SHA384::add(const void* data, size_t num_bytes) {
    // 与 SHA512::add 相同的缓冲逻辑，仅状态属于 SHA384。
    const uint8_t* current = static_cast<const uint8_t*>(data);

    if (buffer_size_ > 0) {
        while (num_bytes > 0 && buffer_size_ < BlockSize) {
            buffer_[buffer_size_++] = *current++;
            num_bytes--;
        }
    }

    if (buffer_size_ == BlockSize) {
        process_block(buffer_);
        num_bytes_ += BlockSize;
        buffer_size_ = 0;
    }

    if (num_bytes == 0) return;

    while (num_bytes >= BlockSize) {
        process_block(current);
        current += BlockSize;
        num_bytes_ += BlockSize;
        num_bytes -= BlockSize;
    }

    while (num_bytes > 0) {
        buffer_[buffer_size_++] = *current++;
        num_bytes--;
    }
}

// 填充并吸收残余缓冲。128 位长度字段按字节展开，避免 64 位移位溢出。
void SHA512::process_buffer() {
    // 填充后总长 = buffer_size_ + 1(0x80) 向上对齐到 mod 128 == 112，
    // 再留 16 字节长度字段凑满一个分组。
    size_t padded_len = buffer_size_ + 1;
    size_t lower7bits = padded_len & 127;
    if (lower7bits <= 112)
        padded_len += 112 - lower7bits;
    else
        padded_len += 128 + 112 - lower7bits;

    unsigned char extra[BlockSize];

    if (buffer_size_ < BlockSize)
        buffer_[buffer_size_] = 128;
    else
        extra[0] = 128;

    size_t i;
    for (i = buffer_size_ + 1; i < BlockSize; i++)
        buffer_[i] = 0;
    for (; i < padded_len; i++)
        extra[i - BlockSize] = 0;

    // 消息位长（128 位大端）：总字节数 * 8。高 64 位 = 总字节数 >> 61，
    // 低 64 位 = 总字节数 << 3（移位不丢信息，二者拼出完整 128 位）。
    uint64_t total_bytes = num_bytes_ + buffer_size_;
    uint64_t high_bits = total_bytes >> 61;
    uint64_t low_bits = total_bytes << 3;

    unsigned char* add_length;
    if (padded_len < BlockSize)
        add_length = buffer_ + padded_len;
    else
        add_length = extra + padded_len - BlockSize;

    *add_length++ = static_cast<unsigned char>((high_bits >> 56) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 48) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 40) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 32) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 24) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 16) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >>  8) & 0xFF);
    *add_length++ = static_cast<unsigned char>( high_bits        & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 56) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 48) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 40) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 32) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 24) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 16) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >>  8) & 0xFF);
    *add_length   = static_cast<unsigned char>( low_bits        & 0xFF);

    process_block(buffer_);
    if (padded_len > BlockSize)
        process_block(extra);
}

void SHA384::process_buffer() {
    size_t padded_len = buffer_size_ + 1;
    size_t lower7bits = padded_len & 127;
    if (lower7bits <= 112)
        padded_len += 112 - lower7bits;
    else
        padded_len += 128 + 112 - lower7bits;

    unsigned char extra[BlockSize];

    if (buffer_size_ < BlockSize)
        buffer_[buffer_size_] = 128;
    else
        extra[0] = 128;

    size_t i;
    for (i = buffer_size_ + 1; i < BlockSize; i++)
        buffer_[i] = 0;
    for (; i < padded_len; i++)
        extra[i - BlockSize] = 0;

    uint64_t total_bytes = num_bytes_ + buffer_size_;
    uint64_t high_bits = total_bytes >> 61;
    uint64_t low_bits = total_bytes << 3;

    unsigned char* add_length;
    if (padded_len < BlockSize)
        add_length = buffer_ + padded_len;
    else
        add_length = extra + padded_len - BlockSize;

    *add_length++ = static_cast<unsigned char>((high_bits >> 56) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 48) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 40) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 32) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 24) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >> 16) & 0xFF);
    *add_length++ = static_cast<unsigned char>((high_bits >>  8) & 0xFF);
    *add_length++ = static_cast<unsigned char>( high_bits        & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 56) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 48) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 40) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 32) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 24) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >> 16) & 0xFF);
    *add_length++ = static_cast<unsigned char>((low_bits >>  8) & 0xFF);
    *add_length   = static_cast<unsigned char>( low_bits        & 0xFF);

    process_block(buffer_);
    if (padded_len > BlockSize)
        process_block(extra);
}

std::string SHA512::get_hash() {
    unsigned char rawHash[HashBytes];
    get_hash(rawHash);

    std::string result;
    result.reserve(2 * HashBytes);
    for (int i = 0; i < HashBytes; i++) {
        static const char dec2hex[17] = "0123456789abcdef";
        result += dec2hex[(rawHash[i] >> 4) & 15];
        result += dec2hex[ rawHash[i]       & 15];
    }
    return result;
}

void SHA512::get_hash(unsigned char buffer[HashBytes]) {
    // 先备份状态再填充吸收，取完结果恢复状态：get_hash 可重复调用且不重置。
    uint64_t oldHash[HashValues];
    for (int i = 0; i < HashValues; i++)
        oldHash[i] = hash_[i];

    process_buffer();

    unsigned char* current = buffer;
    for (int i = 0; i < HashValues; i++) {
        *current++ = static_cast<unsigned char>((hash_[i] >> 56) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 48) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 40) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 32) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 24) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 16) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >>  8) & 0xFF);
        *current++ = static_cast<unsigned char>( hash_[i]        & 0xFF);
        hash_[i] = oldHash[i];
    }
}

std::string SHA384::get_hash() {
    unsigned char rawHash[HashBytes];
    get_hash(rawHash);

    std::string result;
    result.reserve(2 * HashBytes);
    for (int i = 0; i < HashBytes; i++) {
        static const char dec2hex[17] = "0123456789abcdef";
        result += dec2hex[(rawHash[i] >> 4) & 15];
        result += dec2hex[ rawHash[i]       & 15];
    }
    return result;
}

void SHA384::get_hash(unsigned char buffer[HashBytes]) {
    uint64_t oldHash[StateValues];
    for (int i = 0; i < StateValues; i++)
        oldHash[i] = hash_[i];

    process_buffer();

    // SHA-384 = SHA-512 摘要截断前 48 字节（前 6 个 64 位字）。
    unsigned char* current = buffer;
    for (int i = 0; i < HashBytes / 8; i++) {
        *current++ = static_cast<unsigned char>((hash_[i] >> 56) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 48) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 40) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 32) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 24) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >> 16) & 0xFF);
        *current++ = static_cast<unsigned char>((hash_[i] >>  8) & 0xFF);
        *current++ = static_cast<unsigned char>( hash_[i]        & 0xFF);
        hash_[i] = oldHash[i];
    }
}

std::string SHA512::operator()(const void* data, size_t num_bytes) {
    reset();
    add(data, num_bytes);
    return get_hash();
}

std::string SHA512::operator()(const std::string& text) {
    reset();
    add(text.c_str(), text.size());
    return get_hash();
}

std::string SHA384::operator()(const void* data, size_t num_bytes) {
    reset();
    add(data, num_bytes);
    return get_hash();
}

std::string SHA384::operator()(const std::string& text) {
    reset();
    add(text.c_str(), text.size());
    return get_hash();
}

ca::core::Bytes sha512(ca::core::ByteSlice data)
{
    SHA512 hasher;
    hasher.add(data.data(), data.size());

    ca::u8 digest[SHA512::HashBytes];
    hasher.get_hash(digest);
    return ca::core::Bytes::copy_from_slice(digest, SHA512::HashBytes);
}

std::string sha512_hex(ca::core::ByteSlice data)
{
    SHA512 hasher;
    hasher.add(data.data(), data.size());
    return hasher.get_hash();
}

ca::core::Bytes sha384(ca::core::ByteSlice data)
{
    SHA384 hasher;
    hasher.add(data.data(), data.size());

    ca::u8 digest[SHA384::HashBytes];
    hasher.get_hash(digest);
    return ca::core::Bytes::copy_from_slice(digest, SHA384::HashBytes);
}

std::string sha384_hex(ca::core::ByteSlice data)
{
    SHA384 hasher;
    hasher.add(data.data(), data.size());
    return hasher.get_hash();
}

}
