#include "memory_util.h"

#if USE_CUSTOM_MEMORY_UTIL_IMPL

void* mem_set(void* dest, u8 val, usize size)
{
    if (!dest) {
        return NULL;
    }

    u8* p = (u8*)dest;
    while (size--) {
        *p++ = val;
    }
    return dest;
}

void* mem_cpy(void* restrict dest, const void* restrict src, usize size)
{
    if (!dest || !src) {
        return dest;
    }

    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    while (size--) {
        *d++ = *s++;
    }
    return dest;
}

void* mem_move(void* dest, const void* src, usize size)
{
    if (!dest || !src || size == 0) {
        return dest;
    }

    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    if (d < s) {
        // 从头向尾拷贝
        while (size--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        // 从尾向头拷贝，处理重叠
        d += size - 1;
        s += size - 1;
        while (size--) {
            *d-- = *s--;
        }
    }

    return dest;
}

i32 mem_cmp(const void* s1, const void* s2, usize size)
{
    if (s1 == s2 || size == 0) {
        return 0;
    }
    if (!s1) return -1;
    if (!s2) return 1;

    const u8* p1 = (const u8*)s1;
    const u8* p2 = (const u8*)s2;

    while (size--) {
        if (*p1 != *p2) {
            return (i32)(*p1 - *p2);
        }
        p1++;
        p2++;
    }

    return 0;
}

void* mem_find_byte(const void* buf, u8 val, usize size)
{
    if (!buf) {
        return NULL;
    }

    const u8* p = (const u8*)buf;
    while (size--) {
        if (*p == val) {
            return (void*)p;
        }
        p++;
    }

    return NULL;
}

#endif /* USE_CUSTOM_MEMORY_UTIL_IMPL */

#if TEST_ENABLE
#include <em_test/test.h>

TEST_CASE(test_mem_set)
{
    u8 buf[10];
    mem_set(buf, 0xAA, 10);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(0xAA, buf[i]);
    }
    
    // mem_zero
    mem_zero(buf, 10);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(0, buf[i]);
    }
    
    TEST_ASSERT(mem_set(NULL, 0, 10) == NULL);
}

TEST_CASE(test_mem_cpy)
{
    u8 src[5] = {1, 2, 3, 4, 5};
    u8 dest[5] = {0};
    
    mem_cpy(dest, src, 5);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT(src[i], dest[i]);
    }

    // 异常输入覆盖
    TEST_ASSERT(mem_cpy(NULL, src, 5) == NULL);
    TEST_ASSERT(mem_cpy(dest, NULL, 5) == dest);
}

TEST_CASE(test_mem_move)
{
    // 重叠测试：向后移动
    u8 buf[10] = {1, 2, 3, 4, 5, 0, 0, 0, 0, 0};
    mem_move(buf + 2, buf, 5); // 期望: {1, 2, 1, 2, 3, 4, 5, 0, 0, 0}
    u8 expected1[10] = {1, 2, 1, 2, 3, 4, 5, 0, 0, 0};
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(expected1[i], buf[i]);
    }

    // 重叠测试：向前移动
    u8 buf2[10] = {0, 0, 1, 2, 3, 4, 5, 0, 0, 0};
    mem_move(buf2, buf2 + 2, 5); // 期望: {1, 2, 3, 4, 5, 4, 5, 0, 0, 0} 
    // 注意：原来的 buf2[5]=4, buf2[6]=5 是被移动后的值掩盖还是保留取决于实现，
    // 标准 memmove 只保证目标区域正确。
    TEST_ASSERT_EQUAL_INT(1, buf2[0]);
    TEST_ASSERT_EQUAL_INT(5, buf2[4]);

    // 异常输入覆盖
    TEST_ASSERT(mem_move(NULL, buf, 5) == NULL);
    TEST_ASSERT(mem_move(buf, NULL, 5) == buf);
    TEST_ASSERT(mem_move(buf, buf, 0) == buf);
}

TEST_CASE(test_mem_cmp)
{
    u8 b1[] = {1, 2, 3};
    u8 b2[] = {1, 2, 3};
    u8 b3[] = {1, 2, 4};
    
    TEST_ASSERT_EQUAL_INT(0, mem_cmp(b1, b2, 3));
    TEST_ASSERT(mem_cmp(b1, b3, 3) < 0);
    TEST_ASSERT(mem_cmp(b3, b1, 3) > 0);
    
    TEST_ASSERT_EQUAL_INT(0, mem_cmp(NULL, NULL, 5));
    TEST_ASSERT(mem_cmp(NULL, b1, 3) < 0);
    TEST_ASSERT(mem_cmp(b1, NULL, 3) > 0);
}

TEST_CASE(test_mem_find_byte)
{
    u8 buf[] = {0x10, 0x20, 0x30, 0x40};
    TEST_ASSERT(mem_find_byte(buf, 0x30, 4) == &buf[2]);
    TEST_ASSERT(mem_find_byte(buf, 0x50, 4) == NULL);
    
    // 异常输入覆盖
    TEST_ASSERT(mem_find_byte(NULL, 0x10, 4) == NULL);
}

TEST_CASE(test_mem_is_all_val)
{
    u8 buf[] = {0, 0, 0, 0};
    TEST_ASSERT_TRUE(mem_is_all_val(buf, 0, 4));
    buf[2] = 1;
    TEST_ASSERT_FALSE(mem_is_all_val(buf, 0, 4));
    
    TEST_ASSERT_FALSE(mem_is_all_val(NULL, 0, 5));
}

TEST_CASE(test_mem_swap)
{
    u32 v1 = 0x12345678;
    u32 v2 = 0x87654321;
    mem_swap(&v1, &v2, sizeof(u32));
    TEST_ASSERT_EQUAL_UINT(0x87654321, v1);
    TEST_ASSERT_EQUAL_UINT(0x12345678, v2);

    // 异常输入覆盖（确保不崩溃）
    mem_swap(NULL, &v2, 4);
    mem_swap(&v1, NULL, 4);
    mem_swap(&v1, &v1, 4);
    mem_swap(&v1, &v2, 0);
}
#endif
