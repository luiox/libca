#pragma once

#include <cstdint>
#include <string>

namespace ca::crypto {

class SHA1 {
public:
    enum { BlockSize = 512 / 8, HashBytes = 20 };

    SHA1();
    std::string operator()(const void* data, size_t numBytes);
    std::string operator()(const std::string& text);
    void add(const void* data, size_t numBytes);
    std::string getHash();
    void getHash(unsigned char buffer[HashBytes]);
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
