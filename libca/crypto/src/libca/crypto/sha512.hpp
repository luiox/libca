#pragma once

#include "libca/core/bytes.hpp"

#include <cstdint>
#include <string>

namespace ca::crypto {

/// @brief SHA-512 哈希（FIPS 180-4），64 字节摘要，1024 位分组。
class SHA512 {
public:
    enum { BlockSize = 1024 / 8, HashBytes = 64 };

    SHA512();
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
    void process_buffer();

    uint64_t num_bytes_ = 0;
    size_t buffer_size_ = 0;
    uint8_t buffer_[BlockSize]{};

    enum { HashValues = HashBytes / 8 };
    uint64_t hash_[HashValues]{};
};

/// @brief SHA-384 哈希（FIPS 180-4），48 字节摘要。
/// @note 与 SHA-512 共用压缩核心，仅初始 IV 不同并截断为 48 字节。
class SHA384 {
public:
    enum { BlockSize = 1024 / 8, HashBytes = 48 };

    SHA384();
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
    void process_buffer();

    uint64_t num_bytes_ = 0;
    size_t buffer_size_ = 0;
    uint8_t buffer_[BlockSize]{};

    // SHA-384 内部状态与 SHA-512 相同（8 个 64 位字），仅输出截断为 48 字节。
    enum { StateValues = 8 };
    uint64_t hash_[StateValues]{};
};

/// @brief 计算 SHA-512 digest。
/// @param data 输入字节视图。
/// @return 64 字节 SHA-512 digest。
ca::core::Bytes sha512(ca::core::ByteSlice data);

/// @brief 计算 SHA-512 digest 并返回小写十六进制字符串。
/// @param data 输入字节视图。
/// @return 128 字符小写十六进制 digest。
std::string sha512_hex(ca::core::ByteSlice data);

/// @brief 计算 SHA-384 digest。
/// @param data 输入字节视图。
/// @return 48 字节 SHA-384 digest。
ca::core::Bytes sha384(ca::core::ByteSlice data);

/// @brief 计算 SHA-384 digest 并返回小写十六进制字符串。
/// @param data 输入字节视图。
/// @return 96 字符小写十六进制 digest。
std::string sha384_hex(ca::core::ByteSlice data);

}
