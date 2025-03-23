#include "Base64.hpp"

namespace ca {
constexpr char base64_char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const char* src, size_t len)
{
    std::string ret = "";
    ret.reserve(len * 4 / 3);               // base64编码长度大概是原来的4/3
    for (size_t i = 0; i < len; i += 3) {   // 每三个byte处理一次
        char first  = src[i];
        char second = i + 1 < len ? src[i + 1] : 0;
        char third  = i + 2 < len ? src[i + 2] : 0;

        ret += base64_char[(first >> 2) & 0b111111];   // 1st high 6 bits
        ret += base64_char[(first & 0b11) << 4 |
                            ((second >> 4) & 0b1111)];   // 1st low 2 bits + 2nd high 4 bits
        if (i + 1 >= len) {
            ret += "==";
            break;
        }
        ret += base64_char[(second & 0b1111) << 2 |
                            ((third >> 6) & 0b11)];   // 2nd low 4 bits + 3rd high 2 bits
        if (i + 2 >= len) {
            ret += "=";
            break;
        }
        ret += base64_char[third & 0b111111];   // 3rd low 6 bits
    }
    return ret;
}

static char Base64CharConvert(char c)
{
    for (size_t i = 0; i < sizeof(base64_char); ++i)
        if (base64_char[i] == c) return i;
    return -1;   // Invalid base64 character
}


std::vector<char> Base64Decode(const std::string& src)
{
    std::vector<char> ret;
    ret.reserve(src.length() * 3 / 4);
    for (size_t i = 0; i < src.length(); i += 4) {
        char b1 = Base64CharConvert(src[i]);
        char b2 = Base64CharConvert(src[i + 1]);

        ret.push_back((b1 << 2) | ((b2 >> 4 & 0b11)));   // 1st 6bits, 2nd high 2bits

        if (src[i + 2] == '=') break;
        char b3 = Base64CharConvert(src[i + 2]);
        ret.push_back(((b2 & 0b1111) << 4) |
                        ((b3 >> 2) & 0b1111));   // 2nd low 4bits, 3rd high 4bits

        if (src[i + 3] == '=') break;
        char b4 = Base64CharConvert(src[i + 3]);
        ret.push_back(((b3 & 0b11) << 6) | b4);   // 3rd low 2bits, 4th 6bits
    }
    return ret;
}
    
        
}