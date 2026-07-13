#pragma once

#include <cstdint>
#include <string>

namespace ca::crypto {

/// @brief MD5 哈希（RFC 1321），16 字节摘要。
/// @warning MD5 已不满足抗碰撞要求，仅用于非安全场景（校验和、遗留协议）；
///          安全用途请用 SHA-256 及以上。
class MD5 {
public:
    enum { BlockSize = 512 / 8, HashBytes = 16 };

    MD5();
    /// 一次性计算，返回小写十六进制 digest。
    std::string operator()(const void* data, size_t numBytes);
    /// 一次性计算（string 重载）。
    std::string operator()(const std::string& text);
    /// 增量追加数据。
    void add(const void* data, size_t numBytes);
    /// 取十六进制 digest（不重置状态）。
    std::string getHash();
    /// 取原始字节 digest，写入 buffer[HashBytes]。
    void getHash(unsigned char buffer[HashBytes]);
    /// 清空状态，实例复用。
    void reset();

private:
    void processBlock(const void* data);
    void processBuffer();

    uint64_t num_bytes_ = 0;
    size_t buffer_size_ = 0;
    uint8_t buffer_[BlockSize]{};

    enum { HashValues = HashBytes / 4 };
    uint32_t hash_[HashValues]{};
};

}
