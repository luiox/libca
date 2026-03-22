#pragma once

#include <libca/base/Platform.hpp>
#include <cstring>
#include <string>

namespace ca {
    class Crc16 {
    public:
        Crc16() = delete;
        ~Crc16() = delete;
    public:
        static u16 calculate(const void* s, usize n, u16 crc = 0);

        static u16 calculate(const char* s);

        static u16 calculate(const std::string& s);
    };
    
    
}