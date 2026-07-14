/// @file fixed_allocator.h
/// @author canrad (1517807724@qq.com)
/// @brief 固定大小块内存池分配器
/// @version 0.1
/// @date 2026-03-03
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_MPOOL_FIXED_ALLOCATOR_H
#define LIBCA_EM_MPOOL_FIXED_ALLOCATOR_H

#include <em_base/datatype.h>
#include <em_util/lifo.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIXED_ALLOCATOR_OK                  (0)
#define FIXED_ALLOCATOR_ERR_INVALID_PARAM   (-1)
#define FIXED_ALLOCATOR_ERR_INVALID_ALIGN   (-2)
#define FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM  (-3)
#define FIXED_ALLOCATOR_ERR_OUT_OF_POOL     (-4)
#define FIXED_ALLOCATOR_ERR_DOUBLE_FREE     (-5)

typedef struct fixed_allocator{
    void *memory;
    usize total_memory_size;
    usize usable_memory_size;
    usize block_size;
    usize block_stride;
    usize block_count;
    usize free_count;
    u8 *alloc_bitmap;
    usize alloc_bitmap_size;
    lifo_t free_blocks;
}fixed_allocator_t;

/// @brief 使用外部内存初始化固定块分配器（不依赖 malloc）
/// @param self 分配器对象
/// @param memory 外部内存起始地址
/// @param memory_size 外部内存总大小（字节）
/// @param block_size 每个块的有效载荷大小（字节）
/// @param block_count 块数量
/// @param alignment 块对齐（必须为 2 的幂，且不小于指针大小）
/// @return FIXED_ALLOCATOR_OK 成功，其他值表示失败
i32 fixed_allocator_init(fixed_allocator_t* self,
                          void* memory,
                          usize memory_size,
                          usize block_size,
                          usize block_count,
                          usize alignment);

/// @brief 销毁分配器元数据（不释放外部内存）
/// @param self 分配器对象
void fixed_allocator_destroy(fixed_allocator_t* self);

/// @brief 分配一个内存块
/// @param self 分配器对象
/// @return 成功返回块地址，失败返回 NULL
void* fixed_allocator_alloc(fixed_allocator_t* self);

/// @brief 释放一个已分配块（O(1)）
/// @param self 分配器对象
/// @param block 待释放块地址
/// @return FIXED_ALLOCATOR_OK 成功，其他值表示失败
i32 fixed_allocator_free(fixed_allocator_t* self, void* block);

/// @brief 重置分配器，将全部块重新标记为空闲
/// @param self 分配器对象
void fixed_allocator_reset(fixed_allocator_t* self);

/// @brief 查询空闲块数量
/// @param self 分配器对象
/// @return 空闲块数量
usize fixed_allocator_available(const fixed_allocator_t* self);

/// @brief 查询总块数量
/// @param self 分配器对象
/// @return 总块数量
usize fixed_allocator_capacity(const fixed_allocator_t* self);

/// @brief 查询已使用块数量
/// @param self 分配器对象
/// @return 已使用块数量
usize fixed_allocator_used(const fixed_allocator_t* self);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MPOOL_FIXED_ALLOCATOR_H
