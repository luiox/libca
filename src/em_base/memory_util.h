#ifndef LIBCA_EM_BASE_MEMORY_UTIL_H
#define LIBCA_EM_BASE_MEMORY_UTIL_H

#include "datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内存设置（按字节填充）
 * @param dest 目标内存指针
 * @param val 要填充的字节值
 * @param size 填充长度
 * @return 目标内存指针。若 dest 为 NULL 则返回 NULL。
 */
void* mem_set(void* dest, u8 val, usize size);

/**
 * @brief 内存清零
 * @param dest 目标内存指针
 * @param size 清零长度
 */
static inline void mem_zero(void* dest, usize size)
{
    mem_set(dest, 0, size);
}

/**
 * @brief 内存拷贝
 * @note 遵循标准语义，不处理重叠。如果有重叠风险请使用 mem_move。
 * @param dest 目标地址
 * @param src 源地址
 * @param size 长度
 * @return 目标地址
 */
void* mem_cpy(void* restrict dest, const void* restrict src, usize size);

/**
 * @brief 内存移动（安全处理内存重叠）
 * @param dest 目标地址
 * @param src 源地址
 * @param size 长度
 * @return 目标地址
 */
void* mem_move(void* dest, const void* src, usize size);

/**
 * @brief 内存比较
 * @param s1 内存块1
 * @param s2 内存块2
 * @param size 长度
 * @return 0表示相等，第一个不同字节之差表示大小
 */
i32 mem_cmp(const void* s1, const void* s2, usize size);

/**
 * @brief 内存查找（按字节查找）
 * @param buf 内存缓冲区
 * @param val 要查找的字节值
 * @param size 查找长度
 * @return 找到的字节地址，未找到返回 NULL
 */
void* mem_find_byte(const void* buf, u8 val, usize size);

/**
 * @brief 检查内存是否全部为某个值
 * @param buf 内存缓冲区
 * @param val 要检查的字节值
 * @param size 检查长度
 * @return 如果全部匹配返回 true，否则返回 false
 */
bool mem_is_all_val(const void* buf, u8 val, usize size);

/**
 * @brief 内存交换（交换两块互不重叠的内存内容）
 * @param s1 内存块1
 * @param s2 内存块2
 * @param size 长度
 */
void mem_swap(void* s1, void* s2, usize size);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_BASE_MEMORY_UTIL_H
