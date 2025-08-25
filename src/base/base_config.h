/**
 * @file base_config.h
 * @author canrad (1517807724@qq.com)
 * @brief 这个文件包含了base内所有的配置内容
 *   文件结构为：平台相关配置、模块配置、通用内容的定义
 *
 * @version 0.1
 * @date 2024-08-16
 * 
 */
#ifndef LIBCA_BASE_CONFIG_H
#define LIBCA_BASE_CONFIG_H

///////////////////////////////////////////////////////////////////////////////

// debug模块的配置
// 使用串口打印信息，debug模块需要定义这个宏为1
#define USE_DEBUG_MODE 1
// 使用调试断言，需要定义这个宏为1
#define USE_DEBUG_ASSERT 1
// 使用参数检查，需要定义这个宏为1
#define USE_PARAM_CHECK 1


///////////////////////////////////////////////////////////////////////////////

// 相关通用的内容定义
// 这些内容一般不需要修改

// 标准库整数类型头文件
#include <stdint.h>
// 标准库bool类型头文件
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


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



#endif   // !LIBCA_BASE_CONFIG_H
