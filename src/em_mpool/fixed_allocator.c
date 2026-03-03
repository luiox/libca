#include "fixed_allocator.h"

#include "../em_base/debug.h"

/**
 * @brief 判断一个值是否为 2 的幂
 * @param value 待判断值
 * @return true 是 2 的幂，false 不是
 */
static bool fixed_allocator_is_power_of_two(usize value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

/**
 * @brief 将数值向上按 alignment 对齐
 * @param value 原值
 * @param alignment 对齐值（2 的幂）
 * @return 对齐后的值
 */
static usize fixed_allocator_align_up(usize value, usize alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

/**
 * @brief 判断指针是否是当前池中的合法块起始地址
 * @param self 分配器对象
 * @param block 待检查块地址
 * @return true 合法，false 非法
 */
static bool fixed_allocator_is_block_in_pool(const fixed_allocator_t *self, const void *block)
{
    const u8 *start = (const u8 *)self->memory;
    const u8 *end = start + self->block_stride * self->block_count;
    const u8 *ptr = (const u8 *)block;

    if (ptr < start || ptr >= end) {
        return false;
    }

    return ((usize)(ptr - start) % self->block_stride) == 0U;
}

i32 fixed_allocator_init(fixed_allocator_t *self,
                         void *memory,
                         usize memory_size,
                         usize block_size,
                         usize block_count,
                         usize alignment)
{
    param_check(self != NULL);
    if (self == NULL || memory == NULL) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    if (memory_size == 0U || block_size == 0U || block_count == 0U) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    if (!fixed_allocator_is_power_of_two(alignment) || alignment < sizeof(void *)) {
        return FIXED_ALLOCATOR_ERR_INVALID_ALIGN;
    }

    usize real_block_size = block_size;
    if (real_block_size < sizeof(lifo_node_t)) {
        real_block_size = sizeof(lifo_node_t);
    }

    usize stride = fixed_allocator_align_up(real_block_size, alignment);
    if (stride == 0U) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    usize misalign = ((usize)memory) & (alignment - 1U);
    usize adjust = (misalign == 0U) ? 0U : (alignment - misalign);
    if (adjust >= memory_size) {
        return FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM;
    }

    usize usable = memory_size - adjust;
    if (block_count > ((usize)-1) / stride) {
        return FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM;
    }

    usize required = stride * block_count;
    if (usable < required) {
        return FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM;
    }

    self->memory = (u8 *)memory + adjust;
    self->memory_size = memory_size;
    self->block_size = block_size;
    self->block_stride = stride;
    self->block_count = block_count;

    fixed_allocator_reset(self);
    return FIXED_ALLOCATOR_OK;
}

void fixed_allocator_destroy(fixed_allocator_t *self)
{
    param_check(self != NULL);
    if (self == NULL) {
        return;
    }

    self->memory = NULL;
    self->memory_size = 0U;
    self->block_size = 0U;
    self->block_stride = 0U;
    self->block_count = 0U;
    self->free_count = 0U;
    lifo_init(&self->free_blocks);
}

void *fixed_allocator_alloc(fixed_allocator_t *self)
{
    param_check(self != NULL);
    if (self == NULL || self->memory == NULL) {
        return NULL;
    }

    lifo_node_t *node = lifo_pop(&self->free_blocks);
    if (node == NULL) {
        return NULL;
    }

    self->free_count--;
    return (void *)node;
}

i32 fixed_allocator_free(fixed_allocator_t *self, void *block)
{
    param_check(self != NULL);
    if (self == NULL || block == NULL || self->memory == NULL) {
        return FIXED_ALLOCATOR_ERR_INVALID_PARAM;
    }

    if (!fixed_allocator_is_block_in_pool(self, block)) {
        return FIXED_ALLOCATOR_ERR_OUT_OF_POOL;
    }

    lifo_push(&self->free_blocks, (lifo_node_t *)block);
    self->free_count++;
    return FIXED_ALLOCATOR_OK;
}

void fixed_allocator_reset(fixed_allocator_t *self)
{
    param_check(self != NULL);
    if (self == NULL || self->memory == NULL) {
        return;
    }

    lifo_init(&self->free_blocks);
    for (usize i = self->block_count; i > 0U; i--) {
        u8 *block = (u8 *)self->memory + (i - 1U) * self->block_stride;
        lifo_push(&self->free_blocks, (lifo_node_t *)block);
    }
    self->free_count = self->block_count;
}

usize fixed_allocator_available(const fixed_allocator_t *self)
{
    if (self == NULL) {
        return 0U;
    }
    return self->free_count;
}

usize fixed_allocator_capacity(const fixed_allocator_t *self)
{
    if (self == NULL) {
        return 0U;
    }
    return self->block_count;
}

usize fixed_allocator_used(const fixed_allocator_t *self)
{
    if (self == NULL || self->block_count < self->free_count) {
        return 0U;
    }
    return self->block_count - self->free_count;
}

#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(fixed_allocator_init_and_basic)
{
    u8 memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 8U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);
    TEST_ASSERT_EQUAL_UINT(8U, (u32)fixed_allocator_capacity(&allocator));
    TEST_ASSERT_EQUAL_UINT(8U, (u32)fixed_allocator_available(&allocator));
    TEST_ASSERT_EQUAL_UINT(0U, (u32)fixed_allocator_used(&allocator));

    void *p = fixed_allocator_alloc(&allocator);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT(7U, (u32)fixed_allocator_available(&allocator));
    TEST_ASSERT_EQUAL_UINT(1U, (u32)fixed_allocator_used(&allocator));

    ret = fixed_allocator_free(&allocator, p);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);
    TEST_ASSERT_EQUAL_UINT(8U, (u32)fixed_allocator_available(&allocator));
}

TEST_CASE(fixed_allocator_alloc_exhausted)
{
    u8 memory[128];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 4U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void *p1 = fixed_allocator_alloc(&allocator);
    void *p2 = fixed_allocator_alloc(&allocator);
    void *p3 = fixed_allocator_alloc(&allocator);
    void *p4 = fixed_allocator_alloc(&allocator);
    void *p5 = fixed_allocator_alloc(&allocator);

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);
    TEST_ASSERT_NOT_NULL(p4);
    TEST_ASSERT_NULL(p5);
    TEST_ASSERT_EQUAL_UINT(0U, (u32)fixed_allocator_available(&allocator));
}

TEST_CASE(fixed_allocator_lifo_behavior)
{
    u8 memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 8U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void *a = fixed_allocator_alloc(&allocator);
    void *b = fixed_allocator_alloc(&allocator);
    void *c = fixed_allocator_alloc(&allocator);

    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, fixed_allocator_free(&allocator, b));
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, fixed_allocator_free(&allocator, c));

    void *x = fixed_allocator_alloc(&allocator);
    void *y = fixed_allocator_alloc(&allocator);

    TEST_ASSERT_EQUAL_PTR(c, x);
    TEST_ASSERT_EQUAL_PTR(b, y);

    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, fixed_allocator_free(&allocator, a));
}

TEST_CASE(fixed_allocator_invalid_param)
{
    u8 memory[64];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 2U, 3U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_ERR_INVALID_ALIGN, ret);

    ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 2U, 4U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_ERR_INVALID_ALIGN, ret);

    ret = fixed_allocator_init(&allocator, memory, 24U, 16U, 2U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_ERR_NOT_ENOUGH_MEM, ret);
}

TEST_CASE(fixed_allocator_free_out_of_pool_and_reset)
{
    u8 memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 24U, 4U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void *p1 = fixed_allocator_alloc(&allocator);
    void *p2 = fixed_allocator_alloc(&allocator);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);

    u8 outside[24] = {0};
    ret = fixed_allocator_free(&allocator, outside);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_ERR_OUT_OF_POOL, ret);

    fixed_allocator_reset(&allocator);
    TEST_ASSERT_EQUAL_UINT(4U, (u32)fixed_allocator_available(&allocator));
    TEST_ASSERT_EQUAL_UINT(0U, (u32)fixed_allocator_used(&allocator));
}

#endif
