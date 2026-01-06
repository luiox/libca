/**
 * @file compiler_detect.h
 * @author canrad (1517807724@qq.com)
 * @brief 负责编译器宏相关的检测
 * @version 0.1
 * @date 2025-11-02
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef COMPILER_DETECT_H
#define COMPILER_DETECT_H


/*==============================================================================
  编译器、平台和标准特性检测头文件
  提供统一的宏来检测当前编译环境和可用特性
==============================================================================*/

/*==============================================================================
  1. 基础编译器检测
==============================================================================*/
// 检测编译器类型
#if defined(_MSC_VER)
#    define COMPILER_MSVC _MSC_VER
#    define COMPILER_NAME "Microsoft Visual C++"
#elif defined(__clang__)
#    define COMPILER_CLANG __clang_major__
#    define COMPILER_NAME "Clang"
#elif defined(__GNUC__)
#    define COMPILER_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#    define COMPILER_NAME "GNU GCC"
#else
#    define COMPILER_UNKNOWN 1
#    define COMPILER_NAME "Unknown Compiler"
#endif

// 检测编译器版本
#if defined(COMPILER_MSVC)
#    define COMPILER_VERSION_MAJOR _MSC_VER / 100
#    define COMPILER_VERSION_MINOR _MSC_VER % 100
#elif defined(COMPILER_CLANG)
#    define COMPILER_VERSION_MAJOR __clang_major__
#    define COMPILER_VERSION_MINOR __clang_minor__
#elif defined(COMPILER_GCC)
#    define COMPILER_VERSION_MAJOR __GNUC__
#    define COMPILER_VERSION_MINOR __GNUC_MINOR__
#else
#    define COMPILER_VERSION_MAJOR 0
#    define COMPILER_VERSION_MINOR 0
#endif

/*==============================================================================
  2. 语言标准检测
==============================================================================*/
// C标准检测
#if defined(__STDC_VERSION__)
#    if __STDC_VERSION__ >= 201710L
#        define C_STANDARD 201710
#        define C_STANDARD_NAME "C18"
#    elif __STDC_VERSION__ >= 201112L
#        define C_STANDARD 201112
#        define C_STANDARD_NAME "C11"
#    elif __STDC_VERSION__ >= 199901L
#        define C_STANDARD 199901
#        define C_STANDARD_NAME "C99"
#    elif defined(__STDC__)
#        define C_STANDARD 198909
#        define C_STANDARD_NAME "C89/C90"
#    else
#        define C_STANDARD 0
#        define C_STANDARD_NAME "Pre-C89"
#    endif
#else
#    define C_STANDARD 0
#    define C_STANDARD_NAME "Not C"
#endif

// C++标准检测
#ifdef __cplusplus
#    if __cplusplus >= 202302L
#        define CPP_STANDARD 202302
#        define CPP_STANDARD_NAME "C++23"
#    elif __cplusplus >= 202002L
#        define CPP_STANDARD 202002
#        define CPP_STANDARD_NAME "C++20"
#    elif __cplusplus >= 201703L
#        define CPP_STANDARD 201703
#        define CPP_STANDARD_NAME "C++17"
#    elif __cplusplus >= 201402L
#        define CPP_STANDARD 201402
#        define CPP_STANDARD_NAME "C++14"
#    elif __cplusplus >= 201103L
#        define CPP_STANDARD 201103
#        define CPP_STANDARD_NAME "C++11"
#    elif __cplusplus >= 199711L
#        define CPP_STANDARD 199711
#        define CPP_STANDARD_NAME "C++98"
#    else
#        define CPP_STANDARD 0
#        define CPP_STANDARD_NAME "Pre-C++98"
#    endif
#else
#    define CPP_STANDARD 0
#    define CPP_STANDARD_NAME "Not C++"
#endif

/*==============================================================================
  3. 平台和架构检测
==============================================================================*/
// 操作系统检测
#if defined(_WIN32) || defined(_WIN64)
#    define PLATFORM_WINDOWS 1
#    define PLATFORM_NAME "Windows"
#elif defined(__linux__)
#    define PLATFORM_LINUX 1
#    define PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
#    include <TargetConditionals.h>
#    if TARGET_OS_IPHONE
#        define PLATFORM_IOS 1
#        define PLATFORM_NAME "iOS"
#    elif TARGET_OS_MAC
#        define PLATFORM_MACOS 1
#        define PLATFORM_NAME "macOS"
#    else
#        define PLATFORM_APPLE 1
#        define PLATFORM_NAME "Apple"
#    endif
#elif defined(__unix__)
#    define PLATFORM_UNIX 1
#    define PLATFORM_NAME "Unix"
#else
#    define PLATFORM_UNKNOWN 1
#    define PLATFORM_NAME "Unknown Platform"
#endif

// 架构检测
#if defined(_M_X64) || defined(__x86_64__)
#    define ARCH_X64 1
#    define ARCH_NAME "x86_64"
#elif defined(_M_IX86) || defined(__i386__)
#    define ARCH_X86 1
#    define ARCH_NAME "x86"
#elif defined(_M_ARM64) || defined(__aarch64__)
#    define ARCH_ARM64 1
#    define ARCH_NAME "ARM64"
#elif defined(_M_ARM) || defined(__arm__)
#    define ARCH_ARM 1
#    define ARCH_NAME "ARM"
#else
#    define ARCH_UNKNOWN 1
#    define ARCH_NAME "Unknown Architecture"
#endif

/*==============================================================================
  4. 特性检测宏
==============================================================================*/
// C++特性检测
#ifdef __cplusplus
// C++11特性
#    if CPP_STANDARD >= 201103L
#        define HAVE_CPP11 1
#        define HAVE_NULLPTR 1
#        define HAVE_AUTO 1
#        define HAVE_RANGED_FOR 1
#        define HAVE_LAMBDA 1
#        define HAVE_CONSTEXPR 1
#        define HAVE_NOEXCEPT 1
#        define HAVE_OVERRIDE 1
#        define HAVE_FINAL 1
#    endif

// C++14特性
#    if CPP_STANDARD >= 201402L
#        define HAVE_CPP14 1
#        define HAVE_GENERIC_LAMBDAS 1
#        define HAVE_AUTO_RETURN_TYPE 1
#    endif

// C++17特性
#    if CPP_STANDARD >= 201703L
#        define HAVE_CPP17 1
#        define HAVE_STD_BYTE 1
#        define HAVE_STD_OPTIONAL 1
#        define HAVE_STD_VARIANT 1
#        define HAVE_STD_ANY 1
#        define HAVE_STD_STRING_VIEW 1
#    endif

// C++20特性
#    if CPP_STANDARD >= 202002L
#        define HAVE_CPP20 1
#        define HAVE_CONCEPTS 1
#        define HAVE_SPACESHIP 1
#        define HAVE_RANGES 1
#    endif
#else
// C特性检测
#    if C_STANDARD >= 201112L
#        define HAVE_C11 1
#        define HAVE_STATIC_ASSERT 1
#        define HAVE_GENERIC 1
#        define HAVE_ANONYMOUS_STRUCTS 1
#    endif

#    if C_STANDARD >= 199901L
#        define HAVE_C99 1
#        define HAVE_RESTRICT 1
#        define HAVE_INLINE 1
#        define HAVE_VLA 1
#    endif
#endif

// 编译器特定特性检测
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#    define HAVE_ATTRIBUTE_ALWAYS_INLINE 1
#    define HAVE_ATTRIBUTE_PACKED 1
#    define HAVE_ATTRIBUTE_ALIGNED 1
#    define HAVE_BUILTIN_EXPECT 1
#endif

#if defined(COMPILER_MSVC)
#    define HAVE_FORCEINLINE 1
#    define HAVE_DECLSPEC_ALIGN 1
#    define HAVE_DECLSPEC_SELECTANY 1
#endif

/*==============================================================================
  5. 实用工具宏
==============================================================================*/
// 连接两个宏
#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

// 字符串化
#define STRINGIZE_IMPL(x) #x
#define STRINGIZE(x) STRINGIZE_IMPL(x)

// 计算数组元素数量
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// 检测是否为常量表达式
#if defined(__cplusplus)
#    if __cplusplus >= 201103L
#        define IS_CONSTEXPR(expr) noexcept(expr)
#    else
#        define IS_CONSTEXPR(expr) false
#    endif
#else
#    define IS_CONSTEXPR(expr) false
#endif

// 静态断言（跨标准）
#if defined(HAVE_STATIC_ASSERT) || defined(__cpp_static_assert)
#    define STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#elif defined(__cplusplus) && __cplusplus >= 201103L
#    define STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#    define STATIC_ASSERT(condition, message) \
        typedef char CONCAT(static_assert_, __LINE__)[(condition) ? 1 : -1]
#endif

// 不可达代码标记
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#    define UNREACHABLE() __builtin_unreachable()
#elif defined(COMPILER_MSVC)
#    define UNREACHABLE() __assume(0)
#else
#    define UNREACHABLE() \
        do {              \
        } while (0)
#endif

// 分支预测提示
#if defined(HAVE_BUILTIN_EXPECT)
#    define LIKELY(x) __builtin_expect(!!(x), 1)
#    define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#    define LIKELY(x) (x)
#    define UNLIKELY(x) (x)
#endif

void print_compiler_info(void);


///////////////////////////////////////////////////////////////////////////////

// 定义WEAK宏
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#    define WEAK_FUNC __attribute__((weak))
#elif defined(COMPILER_MSVC)
#    define WEAK_FUNC
// Weak attribute is not fully supported in MSVC.
#else
#    define WEAK_FUNC
#endif


///////////////////////////////////////////////////////////////////////////////
// 定义编译器相关的一些宏
// 建议内联的宏
#define CA_SUGGEST_INLINE inline
// 强制内联的宏
#if defined(__GNUC__) || defined(__clang__)
#define CA_FORCE_INLINE inline __attribute__((always_inline))
#else
#define CA_FORCE_INLINE inline
#endif

// 对于尽可能inline的情况下使用
#if defined(__GNUC__) // GNU编译器
    #define LIKELY_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER) // Microsoft Visual C++
    #define LIKELY_INLINE __forceinline
#else
    #define LIKELY_INLINE inline
#endif


#endif   // !COMPILER_DETECT_H
