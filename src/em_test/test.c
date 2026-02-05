/*
 * @file test.c
 * @brief libca 测试框架实现 v2.2
 */

#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

/* ============================================================
 * 全局统计（保持原名称）
 * ============================================================ */

int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;
int current_test_failed = 0;

/* 断言统计（新增） */
static int g_assert_passed = 0;
static int g_assert_failed = 0;

/* ============================================================
 * Section 起止标记（关键！按照原始代码）
 * ============================================================ */

#if defined(_MSC_VER)
__declspec(allocate(".test$a")) const test_t* _test_start = NULL;
__declspec(allocate(".test$z")) const test_t* _test_stop  = NULL;
#else
TEST_CASE_ALLOC const test_t* _test_dummy = NULL;
#endif

/* ============================================================
 * 输出系统
 * ============================================================ */

typedef void (*test_output_fn)(const char* msg);
static test_output_fn g_output_fn = NULL;

static void test_default_output(const char* msg) {
    printf("%s", msg);
}

static void test_output(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (g_output_fn) {
        g_output_fn(buf);
    } else {
        test_default_output(buf);
    }
}

/* ============================================================
 * 断言实现
 * ============================================================ */

void test_assert(const char* file, int line, const char* expr, int passed) {
    if (!passed) {
        test_output("  ✗ %s:%d: ASSERT(%s)\n", file, line, expr);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_u8(const char* file, int line, const char* expr_e, const char* expr_a, uint8_t expected, uint8_t actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_U8(%s, %s) failed: expected %u (0x%02X), got %u (0x%02X)\n",
                    file, line, expr_e, expr_a, expected, expected, actual, actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_i8(const char* file, int line, const char* expr_e, const char* expr_a, int8_t expected, int8_t actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_I8(%s, %s) failed: expected %d (0x%02X), got %d (0x%02X)\n",
                    file, line, expr_e, expr_a, expected, (uint8_t)expected, actual, (uint8_t)actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_u16(const char* file, int line, const char* expr_e, const char* expr_a, uint16_t expected, uint16_t actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_U16(%s, %s) failed: expected %u (0x%04X), got %u (0x%04X)\n",
                    file, line, expr_e, expr_a, expected, expected, actual, actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_i16(const char* file, int line, const char* expr_e, const char* expr_a, int16_t expected, int16_t actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_I16(%s, %s) failed: expected %d (0x%04X), got %d (0x%04X)\n",
                    file, line, expr_e, expr_a, expected, (uint16_t)expected, actual, (uint16_t)actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_u32(const char* file, int line, const char* expr_e, const char* expr_a, uint32_t expected, uint32_t actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_U32(%s, %s) failed: expected %lu (0x%08lX), got %lu (0x%08lX)\n",
                    file, line, expr_e, expr_a, (unsigned long)expected, (unsigned long)expected, 
                    (unsigned long)actual, (unsigned long)actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_i32(const char* file, int line, const char* expr_e, const char* expr_a, int32_t expected, int32_t actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_I32(%s, %s) failed: expected %ld (0x%08lX), got %ld (0x%08lX)\n",
                    file, line, expr_e, expr_a, (long)expected, (unsigned long)expected, 
                    (long)actual, (unsigned long)actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_f32(const char* file, int line, const char* expr_e, const char* expr_a, float expected, float actual, float epsilon) {
    float diff = expected > actual ? expected - actual : actual - expected;
    if (diff > epsilon) {
        test_output("  ✗ %s:%d: EXPECT_EQ_F32(%s, %s) failed: expected %f, got %f (diff: %f, epsilon: %f)\n",
                    file, line, expr_e, expr_a, expected, actual, diff, epsilon);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_f64(const char* file, int line, const char* expr_e, const char* expr_a, double expected, double actual, double epsilon) {
    double diff = expected > actual ? expected - actual : actual - expected;
    if (diff > epsilon) {
        test_output("  ✗ %s:%d: EXPECT_EQ_F64(%s, %s) failed: expected %f, got %f (diff: %f, epsilon: %f)\n",
                    file, line, expr_e, expr_a, expected, actual, diff, epsilon);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_bool(const char* file, int line, const char* expr_e, const char* expr_a, bool expected, bool actual) {
    if (expected != actual) {
        test_output("  ✗ %s:%d: EXPECT_EQ_BOOL(%s, %s) failed: expected %s, got %s\n",
                    file, line, expr_e, expr_a, expected ? "true" : "false", actual ? "true" : "false");
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_null(const char* file, int line, const char* expr, const void* ptr) {
    if (ptr != NULL) {
        test_output("  ✗ %s:%d: EXPECT_NULL(%s) failed: pointer is not NULL (%p)\n",
                    file, line, expr, ptr);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_not_null(const char* file, int line, const char* expr, const void* ptr) {
    if (ptr == NULL) {
        test_output("  ✗ %s:%d: EXPECT_NOT_NULL(%s) failed: pointer is NULL\n",
                    file, line, expr);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_str(const char* file, int line, const char* expr_e, const char* expr_a, const char* expected, const char* actual) {
    if (expected == NULL || actual == NULL) {
        if (expected != actual) {
            test_output("  ✗ %s:%d: EXPECT_EQ_STR(%s, %s) failed: NULL pointer comparison\n",
                        file, line, expr_e, expr_a);
            g_assert_failed++;
            current_test_failed = 1;
        } else {
            g_assert_passed++;
        }
    } else if (strcmp(expected, actual) != 0) {
        test_output("  ✗ %s:%d: EXPECT_EQ_STR(%s, %s) failed: expected \"%s\", got \"%s\"\n",
                    file, line, expr_e, expr_a, expected, actual);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

void test_assert_eq_mem(const char* file, int line, const char* expr_p1, const char* expr_p2, const void* p1, const void* p2, size_t size) {
    if (p1 == NULL || p2 == NULL) {
        test_output("  ✗ %s:%d: EXPECT_EQ_MEM(%s, %s) failed: NULL pointer\n",
                    file, line, expr_p1, expr_p2);
        g_assert_failed++;
        current_test_failed = 1;
    } else if (memcmp(p1, p2, size) != 0) {
        test_output("  ✗ %s:%d: EXPECT_EQ_MEM(%s, %s) failed: memory mismatch at %zu bytes\n",
                    file, line, expr_p1, expr_p2, size);
        g_assert_failed++;
        current_test_failed = 1;
    } else {
        g_assert_passed++;
    }
}

/* ============================================================
 * 插件管理
 * ============================================================ */

/* 插件section起止标记 */
#if defined(_MSC_VER)
__declspec(allocate(".tplugin$a")) const test_plugin_t* _plugin_start = NULL;
__declspec(allocate(".tplugin$z")) const test_plugin_t* _plugin_stop  = NULL;
#else
TEST_PLUGIN_ALLOC const test_plugin_t* _plugin_dummy = NULL;
#endif

/* 当前激活的插件回调 */
static test_plugin_suite_start_fn g_plugin_suite_start = NULL;
static test_plugin_suite_end_fn g_plugin_suite_end = NULL;
static test_plugin_test_start_fn g_plugin_test_start = NULL;
static test_plugin_test_end_fn g_plugin_test_end = NULL;
static test_plugin_assert_fail_fn g_plugin_assert_fail = NULL;

/* 注册插件回调 */
void test_plugin_set_suite_start(test_plugin_suite_start_fn fn) {
    g_plugin_suite_start = fn;
}

void test_plugin_set_suite_end(test_plugin_suite_end_fn fn) {
    g_plugin_suite_end = fn;
}

void test_plugin_set_test_start(test_plugin_test_start_fn fn) {
    g_plugin_test_start = fn;
}

void test_plugin_set_test_end(test_plugin_test_end_fn fn) {
    g_plugin_test_end = fn;
}

void test_plugin_set_assert_fail(test_plugin_assert_fail_fn fn) {
    g_plugin_assert_fail = fn;
}

/* 初始化所有插件 */
static void test_plugins_init(void) {
#if defined(_MSC_VER)
    const test_plugin_t** begin = (const test_plugin_t**)&_plugin_start;
    const test_plugin_t** end   = (const test_plugin_t**)&_plugin_stop;
    
    for (const test_plugin_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL && (*it)->init != NULL) {
            (*it)->init();
        }
    }
#else
    const test_plugin_t** begin = (const test_plugin_t**)__start_test_plugin;
    const test_plugin_t** end   = (const test_plugin_t**)__stop_test_plugin;
    
    for (const test_plugin_t** p = begin; p < end; p++) {
        if (*p != NULL && (*p)->name != NULL && (*p)->init != NULL) {
            (*p)->init();
        }
    }
#endif
}

/* 清理所有插件 */
static void test_plugins_cleanup(void) {
#if defined(_MSC_VER)
    const test_plugin_t** begin = (const test_plugin_t**)&_plugin_start;
    const test_plugin_t** end   = (const test_plugin_t**)&_plugin_stop;
    
    for (const test_plugin_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL && (*it)->cleanup != NULL) {
            (*it)->cleanup();
        }
    }
#else
    const test_plugin_t** begin = (const test_plugin_t**)__start_test_plugin;
    const test_plugin_t** end   = (const test_plugin_t**)__stop_test_plugin;
    
    for (const test_plugin_t** p = begin; p < end; p++) {
        if (*p != NULL && (*p)->name != NULL && (*p)->cleanup != NULL) {
            (*p)->cleanup();
        }
    }
#endif
}

/* ============================================================
 * 测试运行器（按照原始代码逻辑）
 * ============================================================ */

int run_tests(void) {
    total_tests = 0;
    passed_tests = 0;
    failed_tests = 0;
    g_assert_passed = 0;
    g_assert_failed = 0;
    
    /* 初始化所有插件 */
    test_plugins_init();
    
    /* 通知插件测试套件开始 */
    if (g_plugin_suite_start) {
        g_plugin_suite_start(0);  // 暂时传0，后面更新
    }

#if defined(_MSC_VER)
    const test_t** begin = (const test_t**)&_test_start;
    const test_t** end   = (const test_t**)&_test_stop;

    // 计算个数
    for (const test_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL) {
            total_tests++;
        }
    }
    test_output("num_tests = %d\n", total_tests);
    
    /* 更新测试数量 */
    if (g_plugin_suite_start) {
        g_plugin_suite_start(total_tests);
    }

    for (const test_t** it = begin; it <= end; it++) {
        if (*it == NULL || (*it)->name == NULL) {
            continue;
        }
        test_output("Running test: %s\n", (*it)->name);
        
        /* 通知插件测试开始 */
        if (g_plugin_test_start) {
            g_plugin_test_start((*it)->name);
        }
        
        current_test_failed = 0;
        (*it)->func();
        
        /* 通知插件测试结束 */
        if (g_plugin_test_end) {
            g_plugin_test_end((*it)->name, current_test_failed ? 0 : 1);
        }
        
        if (current_test_failed) {
            failed_tests++;
        } else {
            passed_tests++;
        }
    }
#else
    const test_t** begin = (const test_t**)__start_test_array;
    const test_t** end   = (const test_t**)__stop_test_array;

    for (const test_t** t = begin; t < end; t++) {
        if (*t != NULL && (*t)->name != NULL)
            total_tests++;
    }
    test_output("num_tests = %d\n", total_tests);
    
    /* 更新测试数量 */
    if (g_plugin_suite_start) {
        g_plugin_suite_start(total_tests);
    }

    for (const test_t** t = begin; t < end; t++) {
        if (*t == NULL || (*t)->name == NULL)
            continue;
        test_output("Running test: %s\n", (*t)->name);
        
        /* 通知插件测试开始 */
        if (g_plugin_test_start) {
            g_plugin_test_start((*t)->name);
        }
        
        current_test_failed = 0;
        (*t)->func();
        
        /* 通知插件测试结束 */
        if (g_plugin_test_end) {
            g_plugin_test_end((*t)->name, current_test_failed ? 0 : 1);
        }
        
        if (current_test_failed) {
            failed_tests++;
        } else {
            passed_tests++;
        }
    }
#endif

    /* 通知插件测试套件结束 */
    if (g_plugin_suite_end) {
        g_plugin_suite_end(passed_tests, failed_tests);
    }
    
    /* 清理所有插件 */
    test_plugins_cleanup();

    test_output("\nTests finished: %d total, %d passed, %d failed (assertions: %d passed, %d failed)\n", 
                total_tests, passed_tests, failed_tests, g_assert_passed, g_assert_failed);
    return failed_tests;
}

/* ============================================================
 * 自测试
 * ============================================================ */

#if TEST_ENABLE

#if TEST_SELF_MAIN
TEST_CASE(test_module)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
}
#endif // TEST_SELF_MAIN

#endif // TEST_ENABLE
