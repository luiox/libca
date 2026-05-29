#pragma once

#include <cstddef>
#include <string>

namespace ca::crypto {

class Hash {
public:
    virtual ~Hash() = default;
    virtual std::string operator()(const void* data, size_t numBytes) = 0;
    virtual std::string operator()(const std::string& text) = 0;
    virtual void add(const void* data, size_t numBytes) = 0;
    virtual std::string getHash() = 0;
    virtual void getHash(unsigned char buffer[]) = 0;
    virtual void reset() = 0;
};

}
