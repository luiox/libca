#pragma once

/// @file platform.hpp
/// @brief 平台/编译器检测宏 + get_os_name()。
/// @note 有意只支持 Windows/Linux（不做 macOS），其他平台直接 #error。

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
