/// @file memory_util.h
/// @author Canrad
/// @brief 内存操作工具的封装
/// @version 0.2
/// @date 2026-01-17
/// @update 2026-01-31 明确已有的内存操作函数的行为和语义
/// @update 2026-03-05 增加 USE_CUSTOM_MEMORY_UTIL_IMPL 开关，支持标准库内联实现
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_BASE_MEMORY_UTIL_H
#define LIBCA_EM_BASE_MEMORY_UTIL_H

#include "datatype.h"

/* 使用自定义实现开关：
 * - 未定义或定义为 0：使用标准库内联实现（推荐，性能更优）
 * - 定义为 1：使用自定义实现（适用于无标准库环境的嵌入式场景）
 */
#ifndef USE_CUSTOM_MEMORY_UTIL_IMPL
#define USE_CUSTOM_MEMORY_UTIL_IMPL 0
#endif

#if !USE_CUSTOM_MEMORY_UTIL_IMPL
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if USE_CUSTOM_MEMORY_UTIL_IMPL
/* ==================== 自定义实现版本 ==================== */

/// @brief 内存设置（按字节填充）
/// @param dest 目标内存指针
/// @param val 要填充的字节值
/// @param size 填充长度
/// @return 目标内存指针。若 dest 为 NULL 则返回 NULL。
void* mem_set(void* dest, u8 val, usize size);

/// @brief 内存拷贝
/// @note 遵循标准语义，不处理重叠。如果有重叠风险请使用 mem_move。
/// @param dest 目标地址
/// @param src 源地址
/// @param size 长度
/// @return 目标地址
void* mem_cpy(void* restrict dest, const void* restrict src, usize size);

/// @brief 内存移动（安全处理内存重叠）
/// @param dest 目标地址
/// @param src 源地址
/// @param size 长度
/// @return 目标地址
void* mem_move(void* dest, const void* src, usize size);

/// @brief 内存比较
/// @param s1 内存块1
/// @param s2 内存块2
/// @param size 长度
/// @return 0表示相等，第一个不同字节之差表示大小
i32 mem_cmp(const void* s1, const void* s2, usize size);

/// @brief 内存查找（按字节查找）
/// @param buf 内存缓冲区
/// @param val 要查找的字节值
/// @param size 查找长度
/// @return 找到的字节地址，未找到返回 NULL
void* mem_find_byte(const void* buf, u8 val, usize size);

#else
/* ==================== 标准库内联实现版本 ==================== */

/// @brief 内存设置（按字节填充）
/// @param dest 目标内存指针
/// @param val 要填充的字节值
/// @param size 填充长度
/// @return 目标内存指针。若 dest 为 NULL 则返回 NULL。
static inline void* mem_set(void* dest, u8 val, usize size)
{
    if (!dest) {
        return NULL;
    }
    return memset(dest, (int)val, size);
}

/// @brief 内存拷贝
/// @note 遵循标准语义，不处理重叠。如果有重叠风险请使用 mem_move。
/// @param dest 目标地址
/// @param src 源地址
/// @param size 长度
/// @return 目标地址。若 dest 或 src 为 NULL 则返回 dest。
static inline void* mem_cpy(void* restrict dest, const void* restrict src, usize size)
{
    if (!dest || !src) {
        return dest;
    }
    return memcpy(dest, src, size);
}

/// @brief 内存移动（安全处理内存重叠）
/// @param dest 目标地址
/// @param src 源地址
/// @param size 长度
/// @return 目标地址。若 dest 或 src 为 NULL 或 size 为 0 则返回 dest。
static inline void* mem_move(void* dest, const void* src, usize size)
{
    if (!dest || !src || size == 0) {
        return dest;
    }
    return memmove(dest, src, size);
}

/// @brief 内存比较
/// @param s1 内存块1
/// @param s2 内存块2
/// @param size 长度
/// @return 0表示相等，第一个不同字节之差表示大小。
///         若 s1 为 NULL 且 s2 不为 NULL 返回 -1，反之返回 1。
///         若两者都为 NULL 或 size 为 0 返回 0。
static inline i32 mem_cmp(const void* s1, const void* s2, usize size)
{
    if (s1 == s2 || size == 0) {
        return 0;
    }
    if (!s1) return -1;
    if (!s2) return 1;
    return (i32)memcmp(s1, s2, size);
}

/// @brief 内存查找（按字节查找）
/// @param buf 内存缓冲区
/// @param val 要查找的字节值
/// @param size 查找长度
/// @return 找到的字节地址，未找到或 buf 为 NULL 返回 NULL
static inline void* mem_find_byte(const void* buf, u8 val, usize size)
{
    if (!buf) {
        return NULL;
    }
    return memchr(buf, (int)val, size);
}

#endif /* USE_CUSTOM_MEMORY_UTIL_IMPL */

/* ==================== 始终使用自定义实现的函数（无标准库等价物） ==================== */

/// @brief 检查内存是否全部为某个值
/// @param buf 内存缓冲区
/// @param val 要检查的字节值
/// @param size 检查长度
/// @return 如果全部匹配返回 true，否则返回 false
static inline bool mem_is_all_val(const void* buf, u8 val, usize size)
{
    if (!buf || size == 0) {
        return false;
    }

    const u8* p = (const u8*)buf;
    while (size--) {
        if (*p++ != val) {
            return false;
        }
    }
    return true;
}

/// @brief 内存交换（交换两块互不重叠的内存内容）
/// @param s1 内存块1
/// @param s2 内存块2
/// @param size 长度
static inline void mem_swap(void* s1, void* s2, usize size)
{
    if (!s1 || !s2 || s1 == s2 || size == 0) {
        return;
    }

    u8* p1 = (u8*)s1;
    u8* p2 = (u8*)s2;

    while (size--) {
        u8 temp = *p1;
        *p1++ = *p2;
        *p2++ = temp;
    }
}

/// @brief 内存清零
/// @param dest 目标内存指针
/// @param size 清零长度
static inline void mem_zero(void* dest, usize size)
{
    mem_set(dest, 0, size);
}

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_BASE_MEMORY_UTIL_H
