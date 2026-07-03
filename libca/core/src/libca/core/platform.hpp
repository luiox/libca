#pragma once

/// @file platform.hpp
/// @brief 平台/编译器检测宏 + get_os_name()。
/// @note **Experimental**：头文件会引入平台系统头（Windows.h 等），影响下游编译环境；
///       当前只支持 Windows/Linux，其他平台直接 #error。建议只在平台适配/调试层使用，
///       不要在业务公共接口里暴露这些宏。

// ============================================================================
// 平台检测宏
// ============================================================================

#ifdef _WIN32
    #define CA_PLATFORM_WINDOWS 1
#elif __linux__
    #define CA_PLATFORM_LINUX 1
#else
    #error "Unsupported platform: only Windows and Linux are supported"
#endif

// ============================================================================
// 编译器检测宏
// ============================================================================
// 注意: __clang__ 必须在 __GNUC__ 之前检测，因为 Clang 也定义了 __GNUC__

#if defined(__clang__)
    #define CA_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define CA_COMPILER_GCC 1
#elif defined(_MSC_VER)
    #define CA_COMPILER_MSVC 1
#else
    #define CA_COMPILER_UNKNOWN 1
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
// 工具函数
// ============================================================================

#include <string>

namespace ca::core {

/// @brief 返回当前操作系统名（"Windows" 或 "Linux"）。
inline std::string get_os_name() {
#ifdef _WIN32
    return "Windows";
#else
    return "Linux";
#endif
}

} // namespace ca::core
