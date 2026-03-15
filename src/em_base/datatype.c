#include "datatype.h"

#if TEST_ENABLE
#include "em_test/test.h"
#include <limits.h>

TEST_CASE(test_datatype_sizes) {
    // 验证定长类型大小
    TEST_ASSERT_EQUAL_INT(1, sizeof(u8));
    TEST_ASSERT_EQUAL_INT(1, sizeof(i8));
    TEST_ASSERT_EQUAL_INT(2, sizeof(u16));
    TEST_ASSERT_EQUAL_INT(2, sizeof(i16));
    TEST_ASSERT_EQUAL_INT(4, sizeof(u32));
    TEST_ASSERT_EQUAL_INT(4, sizeof(i32));
    TEST_ASSERT_EQUAL_INT(4, sizeof(f32));
    TEST_ASSERT_EQUAL_INT(8, sizeof(f64));

#ifdef HAS_INT64
    TEST_ASSERT_EQUAL_INT(8, sizeof(u64));
    TEST_ASSERT_EQUAL_INT(8, sizeof(i64));
#endif
}

TEST_CASE(test_datatype_macros) {
    // 验证 array_size
    int arr[10];
    TEST_ASSERT_EQUAL_INT(10, array_size(arr));
    
    char arr2[5];
    TEST_ASSERT_EQUAL_INT(5, array_size(arr2));

    // 验证 is_unsigned_v
    // 注意：is_unsigned_v 对于小于 int 的类型（如 u8, u16）在 C 语言中会发生整型提升
    // 导致被提升为 signed int，从而判断失效。仅对 u32/u64 (>= int) 有效。
    u32 vu32 = 0;
    i32 vi32 = 0;
    
    // 注意：宏 is_unsigned_v(a) 实现是 (a >= 0 && ~a >= 0)
    // 只要 a 是无符号数 0， ~0 是 all 1s (max unsigned)，也是 >=0
    // 如果 a 是有符号数 0， ~0 是 -1， -1 < 0
    TEST_ASSERT_TRUE(is_unsigned_v(vu32));
    TEST_ASSERT_FALSE(is_unsigned_v(vi32));
    
    // 验证 is_unsigned_t
    TEST_ASSERT_TRUE(is_unsigned_t(u8));
    TEST_ASSERT_FALSE(is_unsigned_t(i8));
    TEST_ASSERT_TRUE(is_unsigned_t(u16));
    TEST_ASSERT_FALSE(is_unsigned_t(i16));
    TEST_ASSERT_TRUE(is_unsigned_t(u32));
    TEST_ASSERT_FALSE(is_unsigned_t(i32));
}

#endif
