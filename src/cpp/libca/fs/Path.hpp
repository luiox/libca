#pragma once

#include "libca/base/Platform.hpp"
#include <string>

namespace ca {

// 路径类
class Path final
{
private:
    constexpr static const char* IllgealChars = "\\/:*?\"<>|";
    constexpr static size_t MaxPathLen = 8;
    std::string path_;
    Path(const char* path);
    ~Path() = default;
public:
#if CA_PLATFORM_WINDOWS
    constexpr static char Seprator = '\\';
#else
    constexpr static char Seprator = '/';
#endif
    static bool isValid(const char* path);
    static Path of(const char* path);
    
};


}   // namespace ca
