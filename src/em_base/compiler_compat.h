/**
 * @file compiler_compat.h
 * @author canrad (1517807724@qq.com)
 * @brief 负责编译器宏相关的兼容层定义
 * @version 0.1
 * @date 2025-11-02
 * @update
 * 2026-01-31 第一次明确编译器统一宏的标准
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef LIBCA_EM_BASE_COMPILER_COMPAT_H
#define LIBCA_EM_BASE_COMPILER_COMPAT_H

/* ==============================================================================
 * Compiler Compatibility Layer (编译器兼容层)
 * 支持列表: GCC, Clang, ARM Compiler 5/6, MSVC
 * ============================================================================== */

/* ---------------- 1. 强内联 ---------------- */
// 保证函数一定内联，不生成函数体，用于高频小函数
#ifndef CA_INLINE
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_INLINE static inline __attribute__((always_inline))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)   // ARM Compiler 5 / 6
#        define CA_INLINE __attribute__((always_inline)) static inline
#    elif defined(_MSC_VER)
#        define CA_INLINE __forceinline
#    else
#        define CA_INLINE static inline
#    endif
#endif

/* ---------------- 2. 弱符号 ---------------- */
// 允许用户在其他文件重新定义该函数，覆盖库中的默认实现
#ifndef CA_WEAK
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_WEAK __attribute__((weak))
#    elif defined(__CC_ARM) || \
        (defined(__ARMCC_VERSION) && (__ARMCC_VERSION < 6000000)) /* ARM Compiler 5 */
#        define CA_WEAK __weak
#    elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6000000) /* ARM Compiler 6 */
#        define CA_WEAK __attribute__((weak))
#    elif defined(_MSC_VER)
// MSVC 不支持 weak symbol，通常通过链接器选项或宏定义实现，这里给个空宏防止报错
#        define CA_WEAK
#    else
// 不支持的编译器，我也不知道怎么发警告
#        define CA_WEAK
#    endif
#endif

/* ---------------- 3. 链接段放置 ---------------- */
// 把变量/函数放到指定的段，例如 .noinit 或 .ram_code
#ifndef CA_SECTION
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_SECTION(name) __attribute__((section(#name)))
#    elif defined(__ICCARM__) || defined(__ICCRX__)
#        define CA_SECTION(name) __attribute__((section(#name)))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#        define CA_SECTION(name) __attribute__((section(#name)))
#    elif defined(_MSC_VER)
#        define CA_SECTION(name) __declspec(allocate(#name))
#    else
#        define CA_SECTION(name)
#    endif
#endif

/* ---------------- 4. 必须对齐 ---------------- */
#ifndef CA_ALIGNED
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_ALIGNED(n) __attribute__((aligned(n)))
#    elif defined(__ICCARM__) || defined(__ICCRX__)
#        define CA_ALIGNED(n) __attribute__((aligned(n)))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#        define CA_ALIGNED(n) __attribute__((aligned(n)))
#    elif defined(_MSC_VER)
#        define CA_ALIGNED(n) __declspec(align(n))
#    else
#        define CA_ALIGNED(n)
#    endif
#endif

/* ---------------- 6. 结构体紧凑打包 ---------------- */
// 取消结构体填充字节，严格按 1 字节对齐 (用于通信协议解析)
// 注意：不适用MSVC
#ifndef CA_PACKED
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_PACKED __attribute__((packed))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#        define CA_PACKED __attribute__((packed))   // 或 __packed
#    elif defined(_MSC_VER)
#        define CA_PACKED
// MSVC 比较麻烦需要按照下面这样子写
/*
#if defined(_MSC_VER)
    #pragma pack(push, 1)
#endif

typedef struct {
    u8 head;
    u16 len;
} CA_PACKED pkt_t;  // GCC 下这句生效，MSVC 下这句是空的，靠外层的 Pragma 生效

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif
*/
#    else
#        define BASE_PACKED
#    endif
#endif

#endif   // !LIBCA_EM_BASE_COMPILER_COMPAT_H
