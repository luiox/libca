#ifndef LIBCA_PLATFORM_HPP
#define LIBCA_PLATFORM_HPP

#include <string>

#ifdef _WIN32
        #define CA_PLATFORM_WINDOWS 1
        #define CA_PLATFORM_LINUX 0
        #define CA_PLATFORM_MACOS 0
        #define CA_PLATFORM_UNKNOWN 0
#elif __linux__
        #define CA_PLATFORM_LINUX 1
        #define CA_PLATFORM_WINDOWS 0
        #define CA_PLATFORM_MACOS 0
        #define CA_PLATFORM_UNKNOWN 0
#elif __APPLE__
        #define CA_PLATFORM_MACOS 1
        #define CA_PLATFORM_WINDOWS 0
        #define CA_PLATFORM_LINUX 0
        #define CA_PLATFORM_UNKNOWN 0
#else
      #define CA_PLATFORM_UNKNOWN 1
      #define CA_PLATFORM_WINDOWS 0
      #define CA_PLATFORM_LINUX 0
      #define CA_PLATFORM_MACOS 0
#endif

namespace ca {
class OsUtil
{
public:
    static std::string getOsName()
    {
#ifdef _WIN32
        return "Windows";
#elif __linux__
        return "Linux";
#elif __APPLE__
        return "MacOS";
#else
        return "Unknown";
#endif
    }
    static bool isWindows()
    {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    static bool isLinux()
    {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }

    static bool isMac()
    {
#ifdef __APPLE__
        return true;
#else
        return false;
#endif
    }
};

class ArchUtil
{
public:
    static std::string getArchName()
    {
#ifdef _WIN32
        return "x86_64";
#elif __linux__
        return "x86_64";
#elif __APPLE__
        return "x86_64";
#else
        return "Unknown";
#endif
    }
};


}   // namespace ca

#ifdef _WIN32
#    include <Windows.h>
#    include <wincrypt.h>
#endif
#ifdef __linux__
#    include <unistd.h>
#endif
#ifdef __APPLE__
// unsuport
#endif

////////////////////////////////////////////////////////////////////////////////

// complier

#if defined(__GNUC__)
    // Code for GCC
#    define COMPLIER_GCC 1
#elif defined(_MSC_VER)
    // Code for MSVC
#    define COMPLIER_MSVC 1
#elif defined(__clang__)
    // Code for Clang
#    define COMPLIER_CLANG 1
#else
#    warning "Unknown compiler"
    // Fallback or generic code
#endif

#ifdef LIBCA_DLL_MODE
#    ifdef LIBCA_DLL_EXPORT
#        define LIBCA_API __declspec(dllexport)
#    else
#        define LIBCA_API __declspec(dllimport)
#    endif
#else
#    define LIBCA_API
#endif

#include <cctype>

namespace ca {

// 类型定义
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = size_t;

}   // namespace ca

#endif   // !LIBCA_PLATFORM_HPP