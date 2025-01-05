#ifdef LIBCA_USE_TEST

#ifndef LIBCA_CORE_TEST_H
#define LIBCA_CORE_TEST_H

#define LIBCA_TEST_SUCCESS 0
#define LIBCA_TEST_FAILURE 1

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

// 定义测试用例结构体
typedef struct
{
    const char* name;
    void (*func)();
} test_t;

extern int total_tests;

extern int passed_tests;

// 全局测试用例数组
extern test_t* tests;

// 测试用例数量
extern int num_tests;

extern int cur_num;

#    define COUNT_TEST(_name)                                       \
        void __attribute__((constructor(101))) _name##_count_test() \
        {                                                           \
            do {                                                    \
                num_tests++;                                        \
            } while (0);                                            \
        }

// 定义一个宏来注册测试用例
#    define REGISTER_TEST(_name)                                       \
        void __attribute__((constructor(103))) _name##_register_test() \
        {                                                              \
            do {                                                       \
                extern void _name##_test();                            \
                tests[cur_num].name = #_name;                    \
                tests[cur_num].func = _name##_test;              \
                cur_num++;\
            } while (0);                                               \
        }

// 定义一个宏来声明测试用例
#    define TEST_CASE(name) \
        void name##_test(); \
        COUNT_TEST(name)    \
        REGISTER_TEST(name) \
        void name##_test()

static void __attribute__((constructor(102))) create_test_table()
{
    test_t* t = malloc(sizeof(test_t) * num_tests);
    if (t == NULL) {
        printf("Failed to allocate memory for test table\n");
        exit(1);
    }
    // 初始化测试表的内存
    tests = t;
}

// 测试运行器
static void run_tests()
{
    printf("num_tests = %d\n", num_tests);
    for (int i = 0; i < num_tests; i++) {
        if(tests[i].name == NULL){
            printf("Test %d is NULL,func = %p\n", i, tests[i].func);
            continue;
        }
        printf("Running test %d: %s\n",i, tests[i].name);
        tests[i].func();
    }
}

// #ifdef LIBCA_TEST_CONFIG_WITH_MAIN


// #endif   // LIBCA_TEST_CONFIG_WITH_MAIN


#endif   // !LIBCA_CORE_TEST_H

#endif // LIBCA_USE_TEST
