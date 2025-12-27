#ifndef LIBCA_CORE_TEST_H
#define LIBCA_CORE_TEST_H

#define LIBCA_TEST_SUCCESS 0
#define LIBCA_TEST_FAILURE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define TEST_ASSERT_EQUAL_INT(expected, actual)                                                \
    do {                                                                                       \
        if ((expected) != (actual)) {                                                          \
            printf("Test failed: line %d, expected %d, got %d\n", __LINE__, expected, actual); \
        }                                                                                      \
    } while (0)

#define DEFAULT_EPSILON 0.000001

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual, epsilon)                                     \
    do {                                                                                       \
        if (fabs(expected) - (actual) > epsilon) {                                             \
            printf("Test failed: line %d, expected %d, got %d\n", __LINE__, expected, actual); \
        }                                                                                      \
    } while (0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual)                                             \
    do {                                                                                       \
        if (strcmp((expected), (actual)) != 0) {                                               \
            printf("Test failed: line %d, expected %d, got %d\n", __LINE__, expected, actual); \
        }                                                                                      \
    } while (0)

// 定义测试用例结构体
typedef struct
{
    const char* name;
    void (*func)();
} test_t;

extern int total_tests;

extern int passed_tests;

#if defined(_MSC_VER)
#    pragma section(".test$a", read)
#    pragma section(".test$m", read)
#    pragma section(".test$z", read)
#    define TEST_CASE_ALLOC __declspec(allocate(".test$m"))
#else
#    define TEST_CASE_ALLOC __attribute__((used, section("test_array")))
#endif

// 定义一个宏来声明测试用例
#define TEST_CASE(name)                                            \
    static void          name##_test();                            \
    static const test_t  name##_data = {#name, name##_test};       \
    TEST_CASE_ALLOC const test_t* name##_ptr = &name##_data;       \
    static void          name##_test()

#if !defined(_MSC_VER)
extern const test_t __start_test_array[];
extern const test_t __stop_test_array[];
#endif

// 测试运行器
static void run_tests()
{
#if defined(_MSC_VER)
    extern const test_t* const _test_start;
    extern const test_t* const _test_stop;
    const test_t** begin = (const test_t**)&_test_start;
    const test_t** end   = (const test_t**)&_test_stop;

    int count = 0;
    // 计算个数
    for (const test_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL) {
            count++;
        }
    }
    printf("num_tests = %d\n", count);

    for (const test_t** it = begin; it <= end; it++) {
        if (*it == NULL || (*it)->name == NULL) {
            continue;
        }
        printf("Running test: %s\n", (*it)->name);
        (*it)->func();
    }
#else
    const test_t *begin = __start_test_array;
    const test_t *end   = __stop_test_array;
    
    int count = 0;
    for (const test_t* t = begin; t < end; t++) {
        if (t->name != NULL) count++;
    }
    printf("num_tests = %d\n", count);

    for (const test_t* t = begin; t < end; t++) {
        if (t->name == NULL) continue;
        printf("Running test: %s\n", t->name);
        t->func();
    }
#endif
}

// #ifdef LIBCA_TEST_CONFIG_WITH_MAIN


// #endif   // LIBCA_TEST_CONFIG_WITH_MAIN


#endif   // !LIBCA_CORE_TEST_H
