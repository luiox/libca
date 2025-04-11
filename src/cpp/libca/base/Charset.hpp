// 用于提供字符编码操作
#pragma once

#include "Platform.hpp"
#include <vector>

namespace ca {
    class Charset {
    public:
        enum Encoding {
            Latin1,
            Gbk,
            Gb2312,
            Utf8,
            Utf16,
            Utf32
        };

        static std::vector<u8> encode(Encoding encoding, const std::vector<u8> &str);
    };
}