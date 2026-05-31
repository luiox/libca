#pragma once

#include <string>

// ============================================================================
// 平台检测宏
// ============================================================================

#ifdef _WIN32
    #define CA_PLATFORM_WINDOWS 1
    #define CA_PLATFORM_LINUX   0
    #define CA_PLATFORM_MACOS   0
    #define CA_PLATFORM_UNKNOWN 0
#elif __linux__
    #define CA_PLATFORM_LINUX   1
    #define CA_PLATFORM_WINDOWS 0
    #define CA_PLATFORM_MACOS   0
    #define CA_PLATFORM_UNKNOWN 0
#elif __APPLE__
    #define CA_PLATFORM_MACOS   1
    #define CA_PLATFORM_WINDOWS 0
    #define CA_PLATFORM_LINUX   0
    #define CA_PLATFORM_UNKNOWN 0
#else
    #define CA_PLATFORM_UNKNOWN 1
    #define CA_PLATFORM_WINDOWS 0
    #define CA_PLATFORM_LINUX   0
    #define CA_PLATFORM_MACOS   0
#endif

// ============================================================================
// 编译器检测宏
// ============================================================================

#if defined(__GNUC__)
    #define COMPILER_GCC   1
    #define COMPILER_MSVC  0
    #define COMPILER_CLANG 0
#elif defined(_MSC_VER)
    #define COMPILER_GCC   0
    #define COMPILER_MSVC  1
    #define COMPILER_CLANG 0
#elif defined(__clang__)
    #define COMPILER_GCC   0
    #define COMPILER_MSVC  0
    #define COMPILER_CLANG 1
#else
    #define COMPILER_GCC   0
    #define COMPILER_MSVC  0
    #define COMPILER_CLANG 0
    #warning "Unknown compiler"
#endif

// ============================================================================
// 平台相关头文件引入
// ============================================================================

#ifdef _WIN32
    #include <Windows.h>
    #include <wincrypt.h>
#endif
#ifdef __linux__
    #include <unistd.h>
#endif

// ============================================================================
// 工具类
// ============================================================================

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

} // namespace ca
