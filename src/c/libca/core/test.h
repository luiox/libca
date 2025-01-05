#ifndef LIBCA_CORE_TEST_H
#define LIBCA_CORE_TEST_H

#define LIBCA_TEST_SUCCESS 0
#define LIBCA_TEST_FAILURE 1

#ifdef LIBCA_TEST_CONFIG_WITH_MAIN

#    include <stdio.h>
#    include <string.h>
#    include <stdlib.h>
#    include <math.h>

#    define TEST_ASSERT_EQUAL_INT(expected, actual)                                                \
        do {                                                                                       \
            if ((expected) != (actual)) {                                                          \
                printf("Test failed: line %d, expected %d, got %d\n", __LINE__, expected, actual); \
            }                                                                                      \
        } while (0)

#    define DEFAULT_EPSILON 0.000001

#    define TEST_ASSERT_EQUAL_FLOAT(expected, actual, epsilon)                                     \
        do {                                                                                       \
            if (fabs(expected) - (actual) > epsilon) {                                             \
                printf("Test failed: line %d, expected %d, got %d\n", __LINE__, expected, actual); \
            }                                                                                      \
        } while (0)

#    define TEST_ASSERT_EQUAL_STRING(expected, actual)                                             \
        do {                                                                                       \
            if (strcmp((expected), (actual)) != 0) {                                               \
                printf("Test failed: line %d, expected %d, got %d\n", __LINE__, expected, actual); \
            }                                                                                      \
        } while (0)

int total_tests  = 0;
int passed_tests = 0;

// 定义测试用例结构体
typedef struct
{
    const char* name;
    void (*func)();
} test_t;

// 全局测试用例数组
test_t* tests;

// 测试用例数量
int num_tests = 0;

#    define COUNT_TEST(_name)                                       \
        void __attribute__((constructor(101))) _name##_count_test() \
        {                                                           \
            do {                                                    \
                num_tests++;                                        \
            } while (0);                                            \
        }

static void __attribute__((constructor(102))) create_test_table()
{
    tests = malloc(sizeof(test_t) * num_tests);
    if (tests == NULL) {
        printf("Failed to allocate memory for test table\n");
        exit(1);
    }
}

// 定义一个宏来注册测试用例
#    define REGISTER_TEST(_name)                                       \
        void __attribute__((constructor(103))) _name##_register_test() \
        {                                                              \
            do {                                                       \
                extern void _name##_test();                            \
                tests[num_tests - 1].name = #_name;                    \
                tests[num_tests - 1].func = _name##_test;              \
            } while (0);                                               \
        }

// 定义一个宏来声明测试用例
#    define TEST_CASE(name) \
        void name##_test(); \
        COUNT_TEST(name)    \
        REGISTER_TEST(name) \
        void name##_test()

TEST_CASE(test_module)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
}

// 测试运行器
static void run_tests()
{
    for (int i = 0; i < num_tests; i++) {
        printf("Running test: %s\n", tests[i].name);
        tests[i].func();
    }
}

int main(int argc, char** argv)
{
    printf("Start running tests...\n");
    run_tests();
    free(tests);
    return LIBCA_TEST_SUCCESS;
}

#endif   // LIBCA_TEST_CONFIG_WITH_MAIN


#endif   // !LIBCA_CORE_TEST_H
