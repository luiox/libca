#include "fixed_allocator.h"

#include <em_base/debug.h>
#include <em_base/memory_util.h>

/// @brief 判断一个值是否为 2 的幂
/// @param value 待判断值
/// @return true 是 2 的幂，false 不是
static bool fixed_allocator_is_power_of_two(usize value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

/// @brief 将数值向上按 alignment 对齐
/// @param value 原值
/// @param alignment 对齐值（2 的幂）
/// @return 对齐后的值
static usize fixed_allocator_align_up(usize value, usize alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

/// @brief 将地址按 alignment 向上对齐
/// @param ptr 原始地址
/// @param alignment 对齐值（2 的幂）
/// @return 对齐后的地址
static u8* fixed_allocator_align_ptr_up(u8* ptr, usize alignment)
{
    return (u8*)fixed_allocator_align_up((usize)ptr, alignment);
}

/// @brief 计算块在池中的索引
/// @param self 分配器对象
/// @param block 块地址
/// @return 块索引
static usize fixed_allocator_block_index(const fixed_allocator_t* self, const void* block)
{
    return ((usize)((const u8*)block - (const u8*)self->memory)) / self->block_stride;
}

/// @brief 读取分配位图中的一个 bit
/// @param self 分配器对象
/// @param index 块索引
/// @return true 表示已分配，false 表示空闲
static bool fixed_allocator_bitmap_get(const fixed_allocator_t* self, usize index)
{
    usize byte_index = index >> 3;
    u8    mask       = (u8)(1U << (index & 7U));
    return (self->alloc_bitmap[byte_index] & mask) != 0U;
}

/// @brief 将分配位图中的一个 bit 置 1
/// @param self 分配器对象
/// @param index 块索引
static void fixed_allocator_bitmap_set(fixed_allocator_t* self, usize index)
{
    usize byte_index               = index >> 3;
    u8    mask                     = (u8)(1U << (index & 7U));
    self->alloc_bitmap[byte_index] = (u8)(self->alloc_bitmap[byte_index] | mask);
}

/// @brief 将分配位图中的一个 bit 清 0
/// @param self 分配器对象
/// @param index 块索引
static void fixed_allocator_bitmap_clear(fixed_allocator_t* self, usize index)
{
    usize byte_index               = index >> 3;
    u8    mask                     = (u8)(1U << (index & 7U));
    self->alloc_bitmap[byte_index] = (u8)(self->alloc_bitmap[byte_index] & (u8)(~mask));
}

/// @brief 判断指针是否是当前池中的合法块起始地址
/// @param self 分配器对象
/// @param block 待检查块地址
/// @return true 合法，false 非法
static bool fixed_allocator_is_block_in_pool(const fixed_allocator_t* self, const void* block)
{
    const u8* start = (const u8*)self->memory;
    const u8* end   = start + self->block_stride * self->block_count;
    const u8* ptr   = (const u8*)block;

    if (ptr < start || ptr >= end) {
        return false;
    }

    return ((usize)(ptr - start) % self->block_stride) == 0U;
}

i32 fixed_allocator_init(fixed_allocator_t* self, void* memory, usize memory_size, usize block_size,
                         usize block_count, usize alignment)
{
    param_check(self != NULL);
    if (self == NULL || memory == NULL) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    if (memory_size == 0U || block_size == 0U || block_count == 0U) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    if (!fixed_allocator_is_power_of_two(alignment) || alignment < sizeof(void*)) {
        return FIXED_ALLOCATOR_ERR_INVALID_ALIGN;
    }

    usize real_block_size = block_size;
    if (real_block_size < sizeof(lifo_node_t)) {
        real_block_size = sizeof(lifo_node_t);
    }

    usize bitmap_size = (block_count + 7U) >> 3;
    if (bitmap_size == 0U) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    u8* raw_start    = (u8*)memory;
    u8* raw_end      = raw_start + memory_size;
    u8* bitmap_start = raw_start;
    u8* block_start  = fixed_allocator_align_ptr_up(bitmap_start + bitmap_size, alignment);

    if (block_start >= raw_end) {
        return FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM;
    }

    usize stride = fixed_allocator_align_up(real_block_size, alignment);
    if (stride == 0U) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    usize usable = (usize)(raw_end - block_start);
    if (block_count > ((usize)-1) / stride) {
        return FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM;
    }

    usize required = stride * block_count;
    if (usable < required) {
        return FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM;
    }

    self->memory             = block_start;
    self->total_memory_size  = memory_size;
    self->usable_memory_size = usable;
    self->block_size         = block_size;
    self->block_stride       = stride;
    self->block_count        = block_count;
    self->alloc_bitmap       = bitmap_start;
    self->alloc_bitmap_size  = bitmap_size;

    fixed_allocator_reset(self);
    return FIXED_ALLOCATOR_OK;
}

void fixed_allocator_destroy(fixed_allocator_t* self)
{
    param_check(self != NULL);
    if (self == NULL) {
        return;
    }

    self->memory             = NULL;
    self->total_memory_size  = 0U;
    self->usable_memory_size = 0U;
    self->block_size         = 0U;
    self->block_stride       = 0U;
    self->block_count        = 0U;
    self->free_count         = 0U;
    self->alloc_bitmap       = NULL;
    self->alloc_bitmap_size  = 0U;
    lifo_init(&self->free_blocks);
}

void* fixed_allocator_alloc(fixed_allocator_t* self)
{
    param_check(self != NULL);
    if (self == NULL || self->memory == NULL) {
        return NULL;
    }

    lifo_node_t* node = lifo_pop(&self->free_blocks);
    if (node == NULL) {
        return NULL;
    }

    fixed_allocator_bitmap_set(self, fixed_allocator_block_index(self, node));
    self->free_count--;
    return (void*)node;
}

i32 fixed_allocator_free(fixed_allocator_t* self, void* block)
{
    param_check(self != NULL);
    if (self == NULL || block == NULL || self->memory == NULL) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    if (!fixed_allocator_is_block_in_pool(self, block)) {
        return FIXED_ALLOCATOR_ERR_OUT_OF_POOL;
    }

    if (self->free_count >= self->block_count) {
        return FIXED_ALLOCATOR_ERR_DOUBLE_FREE;
    }

    usize index = fixed_allocator_block_index(self, block);
    if (!fixed_allocator_bitmap_get(self, index)) {
        return FIXED_ALLOCATOR_ERR_DOUBLE_FREE;
    }

    fixed_allocator_bitmap_clear(self, index);

    lifo_push(&self->free_blocks, (lifo_node_t*)block);
    self->free_count++;
    return FIXED_ALLOCATOR_OK;
}

void fixed_allocator_reset(fixed_allocator_t* self)
{
    param_check(self != NULL);
    if (self == NULL || self->memory == NULL) {
        return;
    }

    lifo_init(&self->free_blocks);
    if (self->alloc_bitmap != NULL && self->alloc_bitmap_size > 0U) {
        mem_set(self->alloc_bitmap, 0, self->alloc_bitmap_size);
    }

    for (usize i = self->block_count; i > 0U; i--) {
        u8* block = (u8*)self->memory + (i - 1U) * self->block_stride;
        lifo_push(&self->free_blocks, (lifo_node_t*)block);
    }
    self->free_count = self->block_count;
}

usize fixed_allocator_available(const fixed_allocator_t* self)
{
    if (self == NULL) {
        return 0U;
    }
    return self->free_count;
}

usize fixed_allocator_capacity(const fixed_allocator_t* self)
{
    if (self == NULL) {
        return 0U;
    }
    return self->block_count;
}

usize fixed_allocator_used(const fixed_allocator_t* self)
{
    if (self == NULL || self->block_count < self->free_count) {
        return 0U;
    }
    return self->block_count - self->free_count;
}
