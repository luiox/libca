#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace ca {
    class Crc16 {
    public:
        Crc16() = delete;
        ~Crc16() = delete;
    public:
        static uint16_t calculate(const void* s, size_t n, uint16_t crc = 0);

        static uint16_t calculate(const char* s);

        static uint16_t calculate(const std::string& s);
    };
    
    
}