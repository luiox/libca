/* Auto-migrated from src/em_base/macro_util.c test blocks */
#include "macro_util.h"

#include <em_test/test.h>
#include <string.h>

TEST_CASE(test_macro_util_basic) {
    // CA_MAKE_STRING
    TEST_ASSERT_EQUAL_STRING("hello", CA_MAKE_STRING(hello));
    TEST_ASSERT_EQUAL_STRING("123", CA_MAKE_STRING(123));

    // CA_VA_NUM_ARGS
    TEST_ASSERT_EQUAL_INT(1, CA_VA_NUM_ARGS(a));
    TEST_ASSERT_EQUAL_INT(2, CA_VA_NUM_ARGS(a, b));
    TEST_ASSERT_EQUAL_INT(3, CA_VA_NUM_ARGS(a, b, c));
    TEST_ASSERT_EQUAL_INT(10, CA_VA_NUM_ARGS(1,2,3,4,5,6,7,8,9,0));
}

TEST_CASE(test_macro_util_connect) {
    // 测试 2 个参数的连接
    int CA_CONNECT(val, 1) = 111; // val1 = 111
    TEST_ASSERT_EQUAL_INT(111, val1);

    // 测试 3 个参数的连接
    int CA_CONNECT(val, 2, 3) = 222; // val23 = 222
    TEST_ASSERT_EQUAL_INT(222, val23);
}

// 辅助宏用于测试 CA_EVAL
#define TEST_FUNC_1(x)       (x)
#define TEST_FUNC_2(x, y)    ((x) + (y))
#define TEST_FUNC_3(x, y, z) ((x) + (y) + (z))

TEST_CASE(test_macro_util_eval) {
    // CA_EVAL 应该根据参数数量选择 TEST_FUNC_1, TEST_FUNC_2, 或者 TEST_FUNC_3
    
    int r1 = CA_EVAL(TEST_FUNC_, 10);
    TEST_ASSERT_EQUAL_INT(10, r1);

    int r2 = CA_EVAL(TEST_FUNC_, 10, 20);
    TEST_ASSERT_EQUAL_INT(30, r2);

    int r3 = CA_EVAL(TEST_FUNC_, 10, 20, 30);
    TEST_ASSERT_EQUAL_INT(60, r3);
}

