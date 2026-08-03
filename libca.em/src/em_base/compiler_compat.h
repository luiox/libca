/// @file compiler_compat.h
/// @author canrad (1517807724@qq.com)
/// @brief 负责编译器宏相关的兼容层定义
/// @version 0.1
/// @date 2025-11-02
/// @update
/// 2026-01-31 第一次明确编译器统一宏的标准
///
/// @copyright Copyright (c) 2026
///
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
#        define CA_INLINE inline __attribute__((always_inline))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)   // ARM Compiler 5 / 6
#        define CA_INLINE __attribute__((always_inline)) inline
#    elif defined(_MSC_VER)
#        define CA_INLINE __forceinline
#    else
#        define CA_INLINE inline
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

/* ---------------- 5. 结构体紧凑打包 ---------------- */
// 取消结构体填充字节，严格按 1 字节对齐 (用于通信协议解析)
// 注意：不适用MSVC，需要特殊处理
#ifndef CA_PACKED
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_PACKED __attribute__((packed))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#        define CA_PACKED __packed   // 为了兼容AC5编译器，我们尽可能使用__packed而不是__attribute__((packed))
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
#        define CA_PACKED
#    endif
#endif

/* ---------------- 6. 防止被链接器优化 ---------------- */
// 标记为“已使用”，防止链接器因未显式调用而删除（常用于自动初始化/命令注册）
// 配合 CA_SECTION 使用
// 注意：不适用MSVC，需要特殊处理
#ifndef CA_USED
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_USED __attribute__((used))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#        define CA_USED __attribute__((used))
#    elif defined(_MSC_VER)
#        define CA_USED /* MSVC 需要通过 #pragma section/comment 或 linker 参数处理 */
#    else
#        define CA_USED
#    endif
#endif

/* ---------------- 7. 无返回值 ---------------- */
// 告知编译器函数不会返回（用于死循环/复位/断言失败）
// 有助于消除 "control reaches end of non-void function" 警告
#ifndef CA_NO_RETURN
#    if defined(__cplusplus) && (__cplusplus >= 201103L)
#        define CA_NO_RETURN [[noreturn]]
#    elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#        define CA_NO_RETURN _Noreturn
#    elif defined(__GNUC__) || defined(__clang__)
#        define CA_NO_RETURN __attribute__((noreturn))
#    elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#        define CA_NO_RETURN __attribute__((noreturn))
#    elif defined(_MSC_VER)
#        define CA_NO_RETURN __declspec(noreturn)
#    else
#        define CA_NO_RETURN
#    endif
#endif

/* ---------------- 8. 分支预测优化 ---------------- */
// 提示编译器该条件极大概率为真(LIKELY)或假(UNLIKELY)
#ifndef CA_LIKELY
#    if defined(__GNUC__) || defined(__clang__)
#        define CA_LIKELY(x) __builtin_expect(!!(x), 1)
#        define CA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#    else
#        define CA_LIKELY(x) (x)
#        define CA_UNLIKELY(x) (x)
#    endif
#endif

/* ---------------- 9. 数学常量兼容 ---------------- */
// POSIX 的 M_PI / M_PI_2 等宏并非 C 标准：glibc 在 _GNU_SOURCE 或 gnuXX 标准下暴露，
// 但 strict c99/c11 下不暴露；MSVC 需 _USE_MATH_DEFINES。em 库 CI 用 gcc --toolchain=gcc
// 走 strict c99，导致 em_motion 等模块编译失败。这里在平台未定义时补标准值，
// 与 math.h 的定义一致（double），用 #ifndef 保证平台已有定义时不覆盖。
// 使用前需 #include <math.h>（数学函数）并经 datatype.h 链路拿到本头。
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#    define M_PI_2 1.57079632679489661923
#endif
#ifndef M_PI_4
#    define M_PI_4 0.78539816339744830962
#endif

#endif   // !LIBCA_EM_BASE_COMPILER_COMPAT_H
