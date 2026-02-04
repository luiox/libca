/**
 * @file mem_view.h
 * @author canrad (1517807724@qq.com)
 * @brief 内存轻量级视图，不对内存有所有权
 * 针对裸机驱动中的缓冲区解析工具，默认读取以小端方式，带_be是大端序
 * 主要是用于由用户维护接收缓冲区，而驱动只管解析的情况
 * @version 0.1
 * @date 2026-02-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_UTIL_MEM_VIEW_H
#define LIBCA_EM_UTIL_MEM_VIEW_H

#include "../em_base/datatype.h"
#include "../em_base/compiler_compat.h"
#include <string.h>

// 定义
typedef struct mem_view{
    u8 *ptr;
    const u8 *limit;
} mem_view_t;

/**
 * @brief 初始化：直接绑定 buffer
 * @param self 指向 mem_view_t 的实例
 * @param buf 缓冲区起始地址
 * @param len 缓冲区长度 (字节)
 */
static CA_INLINE void mem_view_init(mem_view_t *self, u8 *buf, u16 len) {
    self->ptr = buf;
    self->limit = buf + len;
}

/**
 * @brief 获取剩余字节数
 * @param self 指向 mem_view_t 的实例
 * @return 剩余可读字节数
 */
static CA_INLINE u16 mem_view_remain(const mem_view_t *self) {
    return (u16)(self->limit - self->ptr);
}

/**
 * @brief 内部检查：判断剩余字节是否不少于 size
 * @param self 指向 mem_view_t 的实例
 * @param size 需要的字节数
 * @return true 如果剩余 >= size
 * @note 该函数为内部基础函数，外部使用时可直接调用或使用 _safe 系列
 */
static CA_INLINE bool mem_view_has(const mem_view_t *self, u16 size) {
    return (self->ptr + size <= self->limit);
}

/**
 * @brief 安全跳过 n 个字节（带边界检查）
 * @param self 指向 mem_view_t 的实例
 * @param n 要跳过的字节数
 * @return true 跳过成功并将游标前进 n 字节；false 数据不足，游标不变
 */
static CA_INLINE bool mem_view_skip_safe(mem_view_t *self, u16 n) {
    if (!mem_view_has(self, n)) {
        return false; // 溢出保护
    }
    self->ptr += n;
    return true;
}

/**
 * @brief 不安全跳过 n 个字节（不做边界检查，适用于调用者已确保足够数据以换取性能）
 * @warning 在可用字节 < n 时调用将导致未定义行为
 */
static CA_INLINE void mem_view_skip(mem_view_t *self, u16 n) {
    self->ptr += n;
}
/* ---------------------- Unsafe (高性能，可能 UB) ---------------------- */
/**
 * @brief 读取 8 位无符号数（不做边界检查）
 * @warning 在可用字节 < 1 时行为未定义，建议使用 mem_view_read_u8_safe
 */
static CA_INLINE u8 mem_view_read_u8(mem_view_t *self) {
    return *self->ptr++;
}

/**
 * @brief 读取 16 位无符号数（小端，Low-High），不做边界检查
 * @warning 在可用字节 < 2 时行为未定义，建议使用 mem_view_read_u16_safe
 */
static CA_INLINE u16 mem_view_read_u16(mem_view_t *self) {
    u16 val = (u16)(((u16)self->ptr[1] << 8) | self->ptr[0]);
    self->ptr += 2;
    return val;
}

/**
 * @brief 读取 32 位无符号数（小端），不做边界检查
 * @warning 在可用字节 < 4 时行为未定义，建议使用 mem_view_read_u32_safe
 */
static CA_INLINE u32 mem_view_read_u32(mem_view_t *self) {
    u32 val = (u32)(((u32)self->ptr[3] << 24) | 
                    ((u32)self->ptr[2] << 16) | 
                    ((u32)self->ptr[1] << 8)  | 
                    self->ptr[0]);
    self->ptr += 4;
    return val;
}

/**
 * @brief 不安全的批量读取 len 字节到 dst（不做边界检查，适用于调用者已确保有足够数据）
 * @warning 在可用字节 < len 时调用将导致未定义行为
 */
static CA_INLINE void mem_view_read_buf(mem_view_t *self, u8 *dst, u16 len) {
    memcpy(dst, self->ptr, len);
    self->ptr += len;
}

/* ---------------------- Unsafe (大端) ---------------------- */
/**
 * @brief 读取 16 位无符号数（大端，High-Low），不做边界检查
 */
static CA_INLINE u16 mem_view_read_u16_be(mem_view_t *self) {
    u16 val = (u16)(((u16)self->ptr[0] << 8) | self->ptr[1]);
    self->ptr += 2;
    return val;
}

/**
 * @brief 读取 32 位无符号数（大端），不做边界检查
 */
static CA_INLINE u32 mem_view_read_u32_be(mem_view_t *self) {
    u32 val = (u32)(((u32)self->ptr[0] << 24) | 
                    ((u32)self->ptr[1] << 16) | 
                    ((u32)self->ptr[2] << 8)  | 
                    self->ptr[3]);
    self->ptr += 4;
    return val;
}

/* ---------------------- Unsafe 窥探 (不移动游标) ---------------------- */
/**
 * @brief 窥探指定偏移量的字节 (不移动 ptr)
 * @param self mem view
 * @param offset 相对于当前 ptr 的偏移
 * @return 偏移处的字节（调用者需保证 offset < mem_view_remain(self)）
 */
static CA_INLINE u8 mem_view_peek_u8(const mem_view_t *self, u16 offset) {
    return self->ptr[offset];
}

/**
 * @brief 窥探 16 位（小端，不移动游标）
 */
static CA_INLINE u16 mem_view_peek_u16(const mem_view_t *self, u16 offset) {
    return (u16)(((u16)self->ptr[offset+1] << 8) | self->ptr[offset]);
}

/**
 * @brief 窥探 16 位（大端，不移动游标）
 */
static CA_INLINE u16 mem_view_peek_u16_be(const mem_view_t *self, u16 offset) {
    return (u16)(((u16)self->ptr[offset] << 8) | self->ptr[offset+1]);
}

/* ---------------------- Safe variants (返回 bool，失败时不改变游标，也不写出参) ---------------------- */
/**
 * @brief 安全读取 8 位
 * @return true 成功（out 已写入且游标前进）；false 失败（数据不足，out 未写入，游标不变）
 */
static CA_INLINE bool mem_view_read_u8_safe(mem_view_t *self, u8 *out) {
    if (!mem_view_has(self, 1)) {
        return false;
    }
    *out = *self->ptr++;
    return true;
}

/**
 * @brief 安全读取 16 位（小端）
 */
static CA_INLINE bool mem_view_read_u16_safe(mem_view_t *self, u16 *out) {
    if (!mem_view_has(self, 2)) {
        return false;
    }
    *out = (u16)(((u16)self->ptr[1] << 8) | self->ptr[0]);
    self->ptr += 2;
    return true;
}

/**
 * @brief 安全读取 32 位（小端）
 */
static CA_INLINE bool mem_view_read_u32_safe(mem_view_t *self, u32 *out) {
    if (!mem_view_has(self, 4)) {
        return false;
    }
    *out = (u32)(((u32)self->ptr[3] << 24) | 
                 ((u32)self->ptr[2] << 16) | 
                 ((u32)self->ptr[1] << 8)  | 
                 self->ptr[0]);
    self->ptr += 4;
    return true;
}

/* ---------------------- Safe (big-endian) ---------------------- */
/**
 * @brief 安全读取 16 位（大端）
 */
static CA_INLINE bool mem_view_read_u16_be_safe(mem_view_t *self, u16 *out) {
    if (!mem_view_has(self, 2)) {
        return false;
    }
    *out = (u16)(((u16)self->ptr[0] << 8) | self->ptr[1]);
    self->ptr += 2;
    return true;
}

/**
 * @brief 安全读取 32 位（大端）
 */
static CA_INLINE bool mem_view_read_u32_be_safe(mem_view_t *self, u32 *out) {
    if (!mem_view_has(self, 4)) {
        return false;
    }
    *out = (u32)(((u32)self->ptr[0] << 24) | 
                 ((u32)self->ptr[1] << 16) | 
                 ((u32)self->ptr[2] << 8)  | 
                 self->ptr[3]);
    self->ptr += 4;
    return true;
}

/**
 * @brief 安全批量读取
 */
static CA_INLINE bool mem_view_read_buf_safe(mem_view_t *self, u8 *dst, u16 len) {
    if (!mem_view_has(self, len)) {
        return false;
    }
    memcpy(dst, self->ptr, len);
    self->ptr += len;
    return true;
}

/**
 * @brief 安全窥探 8 位（不移动游标）
 */
static CA_INLINE bool mem_view_peek_u8_safe(const mem_view_t *self, u16 offset, u8 *out) {
    if (offset + 1 > mem_view_remain(self)) {
        return false;
    }
    *out = self->ptr[offset];
    return true;
}

/**
 * @brief 安全窥探 16 位（小端，不移动游标）
 */
static CA_INLINE bool mem_view_peek_u16_safe(const mem_view_t *self, u16 offset, u16 *out) {
    if (offset + 2 > mem_view_remain(self)) {
        return false;
    }
    *out = (u16)(((u16)self->ptr[offset+1] << 8) | self->ptr[offset]);
    return true;
}

/**
 * @brief 安全窥探 16 位（大端，不移动游标）
 */
static CA_INLINE bool mem_view_peek_u16_be_safe(const mem_view_t *self, u16 offset, u16 *out) {
    if (offset + 2 > mem_view_remain(self)) {
        return false;
    }
    *out = (u16)(((u16)self->ptr[offset] << 8) | self->ptr[offset+1]);
    return true;
}

/**
 * @brief 安全窥探 32 位（小端，不移动游标）
 */
static CA_INLINE bool mem_view_peek_u32_safe(const mem_view_t *self, u16 offset, u32 *out) {
    if (offset + 4 > mem_view_remain(self)) {
        return false;
    }
    *out = (u32)(((u32)self->ptr[offset+3] << 24) | 
                 ((u32)self->ptr[offset+2] << 16) | 
                 ((u32)self->ptr[offset+1] << 8)  | 
                 self->ptr[offset]);
    return true;
}

/**
 * @brief 安全窥探 32 位（大端，不移动游标）
 */
static CA_INLINE bool mem_view_peek_u32_be_safe(const mem_view_t *self, u16 offset, u32 *out) {
    if (offset + 4 > mem_view_remain(self)) {
        return false;
    }
    *out = (u32)(((u32)self->ptr[offset] << 24) | 
                 ((u32)self->ptr[offset+1] << 16) | 
                 ((u32)self->ptr[offset+2] << 8)  | 
                 self->ptr[offset+3]);
    return true;
}

#endif // !LIBCA_EM_UTIL_MEM_VIEW_H
