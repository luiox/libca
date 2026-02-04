/**
 * @file mem_view.h
 * @author canrad (1517807724@qq.com)
 * @brief 内存轻量级视图，不对内存有所有权
 * 针对裸机驱动中的缓冲区解析工具
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

// 初始化：直接绑定 buffer
static CA_INLINE void mem_view_init(mem_view_t *v, u8 *buf, u16 len) {
    v->ptr = buf;
    v->limit = buf + len;
}

// 获取剩余长度
static CA_INLINE u16 mem_view_remain(const mem_view_t *v) {
    return (u16)(v->limit - v->ptr);
}

// 检查剩余长度是否足够 (内部基础函数)
static CA_INLINE bool mem_view_has(const mem_view_t *v, u16 size) {
    return (v->ptr + size <= v->limit);
}

// 跳过 n 个字节
// 返回: true 跳过成功, false 数据不够跳(此时通常意味着包不完整)
static CA_INLINE bool mem_view_skip(mem_view_t *v, u16 n) {
    if (!mem_view_has(v, n)) {
        return false; // 溢出保护
    }
    v->ptr += n;
    return true;
}

// 8位读取 (无大小端之分)
static CA_INLINE u8 mem_view_read_u8(mem_view_t *v) {
    return *v->ptr++;
}

// 16位读取 (默认小端序: Low-High)
// 对应协议中的: ... MSL, MSH ...
static CA_INLINE u16 mem_view_read_u16(mem_view_t *v) {
    u16 val;
    // 先读到的 ptr[0] 是低位
    val = ((u16)v->ptr[1] << 8) | v->ptr[0];
    v->ptr += 2;
    return val;
}

// 32位读取 (默认小端序)
static CA_INLINE u32 mem_view_read_u32(mem_view_t *v) {
    u32 val;
    val = ((u32)v->ptr[3] << 24) | 
          ((u32)v->ptr[2] << 16) | 
          ((u32)v->ptr[1] << 8)  | 
          v->ptr[0];
    v->ptr += 4;
    return val;
}

// 批量读取 (相当于 memcpy，自动处理游标)
static CA_INLINE void mem_view_read_buf(mem_view_t *v, u8 *dst, u16 len) {
    if (mem_view_has(v, len)) {
        memcpy(dst, v->ptr, len);
        v->ptr += len;
    }
}

// 16位读取 (大端序: High-Low)
// 对应协议中的: ... MSH, MSL ... (Modbus, TCP/IP)
static CA_INLINE u16 mem_view_read_u16_be(mem_view_t *v) {
    u16 val;
    // 先读到的 ptr[0] 是高位
    val = ((u16)v->ptr[0] << 8) | v->ptr[1];
    v->ptr += 2;
    return val;
}

// 32位读取 (大端序)
static CA_INLINE u32 mem_view_read_u32_be(mem_view_t *v) {
    u32 val;
    val = ((u32)v->ptr[0] << 24) | 
          ((u32)v->ptr[1] << 16) | 
          ((u32)v->ptr[2] << 8)  | 
          v->ptr[3];
    v->ptr += 4;
    return val;
}

// 窥探指定偏移量的字节 (不移动 ptr)
// offset=0 表示看当前的 ptr[0]
static CA_INLINE u8 mem_view_peek_u8(const mem_view_t *v, u16 offset) {
    // 调用者需确保 offset < mem_view_remain(v)，否则行为未定义
    return v->ptr[offset];
}

// 窥探 16位 (默认小端)
static CA_INLINE u16 mem_view_peek_u16(const mem_view_t *v, u16 offset) {
    u16 val = ((u16)v->ptr[offset+1] << 8) | v->ptr[offset];
    return val;
}

// 窥探 16位 (大端)
static CA_INLINE u16 mem_view_peek_u16_be(const mem_view_t *v, u16 offset) {
    u16 val = ((u16)v->ptr[offset] << 8) | v->ptr[offset+1];
    return val;
}

#endif // !LIBCA_EM_UTIL_MEM_VIEW_H
