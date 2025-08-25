#ifndef TEST_HELPER_H
#define TEST_HELPER_H
#include <stdio.h>


// 断言宏，断言失败时自动累加failure_test_case_cnt
#define ASSERT_TRUE(expr)                                                  \
    do {                                                                  \
        if (!(expr)) {                                                    \
            printf("ASSERT_TRUE failed: %s (line %d)\n", #expr, __LINE__); \
            failure_test_case_cnt++;                                      \
        }                                                                 \
    } while (0)

#define ASSERT_EQ(a, b)                                                         \
    do {                                                                       \
        if ((a) != (b)) {                                                      \
            printf("ASSERT_EQ failed: %s == %s (line %d), left=%d, right=%d\n", \
                   #a, #b, __LINE__, (int)(a), (int)(b));                     \
            failure_test_case_cnt++;                                           \
        }                                                                      \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                                                 \
    do {                                                                                   \
        if (strcmp((a), (b)) != 0) {                                                       \
            printf("ASSERT_STR_EQ failed: %s == %s (line %d), left=\"%s\", right=\"%s\"\n", \
                   #a, #b, __LINE__, (a), (b));                                           \
            failure_test_case_cnt++;                                                       \
        }                                                                                  \
    } while (0)

#define TEST_CASE_BEGIN(func_name)                     \
    void func_name()                                   \
    {                                                  \
        int failure_test_case_cnt = 0;                 \
        printf("\n[%s]: Running test case\n", #func_name); \
        /* 初始化测试环境 */

#define TEST_CASE_END(func_name)                                                            \
    if (failure_test_case_cnt == 0) {                                                       \
        printf("\033[32mTest case [%s] passed!\033[0m\n", #func_name);                    \
    }                                                                                       \
    else {                                                                                  \
        printf("\033[31mTest case [%s] failed with %d errors!\033[0m\n", #func_name, failure_test_case_cnt); \
    }                                                                                       \
    }

#endif   // TEST_HELPER_H
