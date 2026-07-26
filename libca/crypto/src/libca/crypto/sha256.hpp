#pragma once

#include "libca/core/bytes.hpp"

#include <cstdint>
#include <string>

namespace ca::crypto {

class SHA256 {
public:
    enum { BlockSize = 512 / 8, HashBytes = 32 };

    SHA256();
    std::string operator()(const void* data, size_t num_bytes);
    std::string operator()(const std::string& text);
    void add(const void* data, size_t num_bytes);
    std::string get_hash();
    void get_hash(unsigned char buffer[HashBytes]);
    void reset();

private:
    void process_block(const void* data);
    void process_buffer();

    uint64_t num_bytes_ = 0;
    size_t buffer_size_ = 0;
    uint8_t buffer_[BlockSize]{};

    enum { HashValues = HashBytes / 4 };
    uint32_t hash_[HashValues]{};
};

/// @brief 计算 SHA-256 digest。
/// @param data 输入字节视图。
/// @return 32 字节 SHA-256 digest。
ca::core::Bytes sha256(ca::core::ByteSlice data);

/// @brief 计算 SHA-256 digest 并返回小写十六进制字符串。
/// @param data 输入字节视图。
/// @return 64 字符小写十六进制 digest。
std::string sha256_hex(ca::core::ByteSlice data);

}
