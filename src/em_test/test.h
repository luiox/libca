#ifndef LIBCA_CORE_TEST_H
#define LIBCA_CORE_TEST_H

#define LIBCA_TEST_SUCCESS 0
#define LIBCA_TEST_FAILURE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../em_base/macro_util.h"

// 关闭clangformat对宏的格式化
// clang-format off
#define TEST_ASSERT_EQUAL_INT(expected, actual)                                                \
    do {                                                                                       \
        if ((expected) != (actual)) {                                                          \
            printf("Test failed: %s:%d, expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT(expected, actual)                                               \
    do {                                                                                       \
        if ((expected) != (actual)) {                                                          \
            printf("Test failed: %s:%d, expected %u, got %u\n", __FILE__, __LINE__, (unsigned int)(expected), (unsigned int)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT(condition)                                                                 \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            printf("Test failed: %s:%d, condition %s is false\n", __FILE__, __LINE__, #condition);     \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define DEFAULT_EPSILON 0.000001

#define TEST_ASSERT_EQUAL_FLOAT_(expected, actual, epsilon)                                     \
    do {                                                                                       \
        if (fabs((expected) - (actual)) > (epsilon)) {                                         \
            printf("Test failed: %s:%d, expected %f, got %f\n", __FILE__, __LINE__, (double)(expected), (double)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual) TEST_ASSERT_EQUAL_FLOAT_(expected, actual, DEFAULT_EPSILON)

#define TEST_ASSERT_EQUAL_STRING(expected, actual)                                             \
    do {                                                                                       \
        if (strcmp((expected), (actual)) != 0) {                                               \
            printf("Test failed: %s:%d, expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, (expected), (actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_NOT_EQUAL_INT(expected, actual)                                            \
    do {                                                                                       \
        if ((expected) == (actual)) {                                                          \
            printf("Test failed: %s:%d, expected %d to not equal %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_NOT_EQUAL_UINT(expected, actual)                                           \
    do {                                                                                       \
        if ((expected) == (actual)) {                                                          \
            printf("Test failed: %s:%d, expected %u to not equal %u\n", __FILE__, __LINE__, (unsigned int)(expected), (unsigned int)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_TRUE(condition)                                                            \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            printf("Test failed: %s:%d, expected true but got false\n", __FILE__, __LINE__);           \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_FALSE(condition)                                                           \
    do {                                                                                       \
        if ((condition)) {                                                                     \
            printf("Test failed: %s:%d, expected false but got true\n", __FILE__, __LINE__);           \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_NULL(pointer)                                                              \
    do {                                                                                       \
        if ((pointer) != NULL) {                                                               \
            printf("Test failed: %s:%d, expected NULL pointer\n", __FILE__, __LINE__);                 \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_NOT_NULL(pointer)                                                          \
    do {                                                                                       \
        if ((pointer) == NULL) {                                                               \
            printf("Test failed: %s:%d, expected non-NULL pointer\n", __FILE__, __LINE__);             \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_EQUAL_PTR(expected, actual)                                                \
    do {                                                                                       \
        if ((expected) != (actual)) {                                                          \
            printf("Test failed: %s:%d, expected pointer %p, got %p\n", __FILE__, __LINE__, (void*)(expected), (void*)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, size)                                       \
    do {                                                                                       \
        if (memcmp((expected), (actual), (size)) != 0) {                                       \
            printf("Test failed: %s:%d, memory mismatch\n", __FILE__, __LINE__);                       \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_INT_WITHIN(min, max, actual)                                               \
    do {                                                                                       \
        if ((actual) < (min) || (actual) > (max)) {                                            \
            printf("Test failed: %s:%d, expected %d to be within [%d, %d]\n", __FILE__, __LINE__, (int)(actual), (int)(min), (int)(max)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_UINT_WITHIN(min, max, actual)                                              \
    do {                                                                                       \
        if ((actual) < (min) || (actual) > (max)) {                                            \
            printf("Test failed: %s:%d, expected %u to be within [%u, %u]\n", __FILE__, __LINE__, (unsigned int)(actual), (unsigned int)(min), (unsigned int)(max)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX(expected, actual)                                                \
    do {                                                                                       \
        if ((expected) != (actual)) {                                                          \
            printf("Test failed: %s:%d, expected 0x%X, got 0x%X\n", __FILE__, __LINE__, (unsigned int)(expected), (unsigned int)(actual)); \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

// 定义测试用例结构体
typedef struct
{
    const char* name;
    void (*func)();
} test_t;

extern int total_tests;

extern int passed_tests;

extern int failed_tests;

extern int current_test_failed;

// clang-format off
#if defined(_MSC_VER)
#    pragma section(".test$a", read)
#    pragma section(".test$m", read)
#    pragma section(".test$z", read)
#    define TEST_CASE_ALLOC __declspec(allocate(".test$m"))
#else
#    define TEST_CASE_ALLOC __attribute__((used, section("test_array")))
#endif

// 定义一个宏来声明测试用例
#define TEST_CASE(name)                                                                        \
    static void          CA_SAFE_NAME(test_func_)(void);                         \
    static const test_t  CA_SAFE_NAME(test_data_) = {CA_MAKE_STRING(name), CA_SAFE_NAME(test_func_)}; \
    static TEST_CASE_ALLOC const test_t* CA_SAFE_NAME(test_ptr_) = &CA_SAFE_NAME(test_data_); \
    static void          CA_SAFE_NAME(test_func_)(void)
// clang-format on

#if !defined(_MSC_VER)
extern const test_t* __start_test_array[] __attribute__((visibility("hidden")));
extern const test_t* __stop_test_array[] __attribute__((visibility("hidden")));
#endif

// 测试运行器
int run_tests();

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_CORE_TEST_H

