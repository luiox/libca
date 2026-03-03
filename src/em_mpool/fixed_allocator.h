/**
 * @file fixed_allocator.h
 * @author canrad (1517807724@qq.com)
 * @brief 固定大小块内存池分配器
 * @version 0.1
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_MPOOL_FIXED_ALLOCATOR_H
#define LIBCA_EM_MPOOL_FIXED_ALLOCATOR_H

#include "../em_base/datatype.h"

typedef struct fixed_allocator{

}fixed_allocator_t;

// 使用外部提供的内存区域初始化分配器（不拥有内存）
i32 fixed_allocator_init(fixed_allocator_t* self,
                          void* memory,
                          usize block_size,
                          usize block_count,
                          usize alignment);

// 销毁分配器（释放内部资源）
void fixed_allocator_destroy(fixed_allocator_t* self);

// 分配一个内存块，成功返回指针，失败返回 NULL
void* fixed_allocator_alloc(fixed_allocator_t* self);

// 释放之前分配的内存块
void fixed_allocator_free(fixed_allocator_t* self, void* block);

// 重置分配器，将所有块标记为空闲
void fixed_allocator_reset(fixed_allocator_t* self);

// 查询剩余空闲块数量
usize fixed_allocator_available(const fixed_allocator_t* self);

#endif // !LIBCA_EM_MPOOL_FIXED_ALLOCATOR_H
