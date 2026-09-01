#pragma once

#include <cstdint>
#include <string>

namespace ca::crypto {

/// @brief SHA-1 哈希（FIPS 180-4），20 字节摘要。
/// @warning SHA-1 已被证明不抗碰撞，仅用于非安全场景；安全用途请用 SHA-256 及以上。
class SHA1
{
public:
    enum
    {
        BlockSize = 512 / 8,
        HashBytes = 20
    };

    SHA1();
    /// 一次性计算，返回小写十六进制 digest。
    std::string operator()(const void* data, size_t num_bytes);
    /// 一次性计算（string 重载）。
    std::string operator()(const std::string& text);
    /// 增量追加数据。
    void add(const void* data, size_t num_bytes);
    /// 取十六进制 digest（不重置状态）。
    std::string get_hash();
    /// 取原始字节 digest，写入 buffer[HashBytes]。
    void get_hash(unsigned char buffer[HashBytes]);
    /// 清空状态，实例复用。
    void reset();

private:
    void process_block(const void* data);
    void process_buffer();

    uint64_t num_bytes_   = 0;
    size_t   buffer_size_ = 0;
    uint8_t  buffer_[BlockSize]{};

    enum
    {
        HashValues = HashBytes / 4
    };
    uint32_t hash_[HashValues]{};
};

}   // namespace ca::crypto
