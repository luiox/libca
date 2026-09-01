/* Auto-migrated from src/em_mpool/fixed_allocator.c test blocks */
#include "fixed_allocator.h"
#include <em_base/debug.h>
#include <em_base/memory_util.h>


#include <em_test/test.h>

TEST_CASE(fixed_allocator_init_and_basic)
{
    u8                memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 8U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);
    TEST_ASSERT_EQUAL_UINT((u32)sizeof(memory), (u32)allocator.total_memory_size);
    TEST_ASSERT(allocator.usable_memory_size >= allocator.block_stride * allocator.block_count);
    TEST_ASSERT_EQUAL_UINT(8U, (u32)fixed_allocator_capacity(&allocator));
    TEST_ASSERT_EQUAL_UINT(8U, (u32)fixed_allocator_available(&allocator));
    TEST_ASSERT_EQUAL_UINT(0U, (u32)fixed_allocator_used(&allocator));

    void* p = fixed_allocator_alloc(&allocator);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT(7U, (u32)fixed_allocator_available(&allocator));
    TEST_ASSERT_EQUAL_UINT(1U, (u32)fixed_allocator_used(&allocator));

    ret = fixed_allocator_free(&allocator, p);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);
    TEST_ASSERT_EQUAL_UINT(8U, (u32)fixed_allocator_available(&allocator));
}

TEST_CASE(fixed_allocator_alloc_exhausted)
{
    u8                memory[128];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 4U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void* p1 = fixed_allocator_alloc(&allocator);
    void* p2 = fixed_allocator_alloc(&allocator);
    void* p3 = fixed_allocator_alloc(&allocator);
    void* p4 = fixed_allocator_alloc(&allocator);
    void* p5 = fixed_allocator_alloc(&allocator);

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);
    TEST_ASSERT_NOT_NULL(p4);
    TEST_ASSERT_NULL(p5);
    TEST_ASSERT_EQUAL_UINT(0U, (u32)fixed_allocator_available(&allocator));
}

TEST_CASE(fixed_allocator_lifo_behavior)
{
    u8                memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 16U, 8U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void* a = fixed_allocator_alloc(&allocator);
    void* b = fixed_allocator_alloc(&allocator);
    void* c = fixed_allocator_alloc(&allocator);

    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, fixed_allocator_free(&allocator, b));
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, fixed_allocator_free(&allocator, c));

    void* x = fixed_allocator_alloc(&allocator);
    void* y = fixed_allocator_alloc(&allocator);

    TEST_ASSERT_EQUAL_PTR(c, x);
    TEST_ASSERT_EQUAL_PTR(b, y);

    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, fixed_allocator_free(&allocator, a));
}

TEST_CASE(fixed_allocator_invalid_param)
{
    u8                memory[64];
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
    u8                memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 24U, 4U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void* p1 = fixed_allocator_alloc(&allocator);
    void* p2 = fixed_allocator_alloc(&allocator);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);

    u8 outside[24] = {0};
    ret            = fixed_allocator_free(&allocator, outside);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_ERR_OUT_OF_POOL, ret);

    fixed_allocator_reset(&allocator);
    TEST_ASSERT_EQUAL_UINT(4U, (u32)fixed_allocator_available(&allocator));
    TEST_ASSERT_EQUAL_UINT(0U, (u32)fixed_allocator_used(&allocator));
}

TEST_CASE(fixed_allocator_double_free)
{
    u8                memory[256];
    fixed_allocator_t allocator;

    i32 ret = fixed_allocator_init(&allocator, memory, sizeof(memory), 24U, 4U, 8U);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    void* p = fixed_allocator_alloc(&allocator);
    TEST_ASSERT_NOT_NULL(p);

    ret = fixed_allocator_free(&allocator, p);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_OK, ret);

    ret = fixed_allocator_free(&allocator, p);
    TEST_ASSERT_EQUAL_INT(FIXED_ALLOCATOR_ERR_DOUBLE_FREE, ret);
}
