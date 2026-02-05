/*
 * @file test.h
 * @brief libca 测试框架头文件 v2.2
 * 
 * 简洁、类型安全的C语言单元测试框架
 * 兼容 xmake rule("em_test")
 */

#ifndef LIBCA_CORE_TEST_H
#define LIBCA_CORE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* 依赖 em_base 头文件 */
#include "../em_base/macro_util.h"

/* ============================================================
 * 配置宏
 * ============================================================ */

#ifndef TEST_EPSILON_F32
#define TEST_EPSILON_F32  1e-6f
#endif

#ifndef TEST_EPSILON_F64
#define TEST_EPSILON_F64  1e-6
#endif

/* ============================================================
 * 测试用例注册（兼容原版设计）
 * ============================================================ */

typedef struct {
    const char* name;
    void (*func)(void);
} test_t;

/* 全局统计变量（保持原名称以兼容） */
extern int total_tests;
extern int passed_tests;
extern int failed_tests;
extern int current_test_failed;

/* MSVC section 定义 */
#if defined(_MSC_VER)
#    pragma section(".test$a", read)
#    pragma section(".test$m", read)
#    pragma section(".test$z", read)
#    define TEST_CASE_ALLOC __declspec(allocate(".test$m"))
#else
#    define TEST_CASE_ALLOC __attribute__((used, section("test_array")))
#endif

/* TEST_CASE 宏（保持原设计） */
#define TEST_CASE(name)                                                                        \
    static void          CA_SAFE_NAME(test_func_)(void);                                       \
    static const test_t  CA_SAFE_NAME(test_data_) = {CA_MAKE_STRING(name), CA_SAFE_NAME(test_func_)}; \
    static TEST_CASE_ALLOC const test_t* CA_SAFE_NAME(test_ptr_) = &CA_SAFE_NAME(test_data_); \
    static void          CA_SAFE_NAME(test_func_)(void)

/* GCC/Clang 的外部符号 */
#if !defined(_MSC_VER)
extern const test_t* __start_test_array[] __attribute__((visibility("hidden")));
extern const test_t* __stop_test_array[] __attribute__((visibility("hidden")));
#endif

/* 测试运行器 */
int run_tests(void);

/* ============================================================
 * 插件架构
 * ============================================================ */

/* 插件回调函数类型 */
typedef void (*test_plugin_init_fn)(void);
typedef void (*test_plugin_cleanup_fn)(void);
typedef void (*test_plugin_suite_start_fn)(int test_count);
typedef void (*test_plugin_suite_end_fn)(int passed, int failed);
typedef void (*test_plugin_test_start_fn)(const char* test_name);
typedef void (*test_plugin_test_end_fn)(const char* test_name, int passed);
typedef void (*test_plugin_assert_fail_fn)(const char* file, int line, const char* expr);

/* 插件结构体 */
typedef struct {
    const char* name;                           // 插件名称
    test_plugin_init_fn init;                   // 初始化函数（注册回调）
    test_plugin_cleanup_fn cleanup;             // 清理函数
    test_plugin_suite_start_fn suite_start;     // 测试套件开始
    test_plugin_suite_end_fn suite_end;         // 测试套件结束
    test_plugin_test_start_fn test_start;       // 单个测试开始
    test_plugin_test_end_fn test_end;           // 单个测试结束
    test_plugin_assert_fail_fn assert_fail;     // 断言失败
    void* user_data;                            // 用户数据
} test_plugin_t;

/* 插件section定义 */
#if defined(_MSC_VER)
#    pragma section(".tplugin$a", read)
#    pragma section(".tplugin$m", read)
#    pragma section(".tplugin$z", read)
#    define TEST_PLUGIN_ALLOC __declspec(allocate(".tplugin$m"))
#else
#    define TEST_PLUGIN_ALLOC __attribute__((used, section("test_plugin")))
#endif

/* 
 * 插件自动注册宏
 * 用法：TEST_PLUGIN_REGISTER(my_plugin, my_plugin_init)
 * 插件需要在init函数中设置自己的回调函数
 */
#define TEST_PLUGIN_REGISTER(plugin_name, init_func)                                           \
    static void init_func(void);                                                               \
    static const test_plugin_t test_plugin_data_##plugin_name = {                              \
        CA_MAKE_STRING(plugin_name),                                                           \
        init_func,                                                                             \
        NULL, NULL, NULL, NULL, NULL, NULL,                                                    \
        NULL                                                                                   \
    };                                                                                         \
    static TEST_PLUGIN_ALLOC const test_plugin_t* test_plugin_ptr_##plugin_name = &test_plugin_data_##plugin_name

/* GCC/Clang 插件section外部符号 */
#if !defined(_MSC_VER)
extern const test_plugin_t* __start_test_plugin[] __attribute__((visibility("hidden")));
extern const test_plugin_t* __stop_test_plugin[] __attribute__((visibility("hidden")));
#endif

/* 插件回调设置API（在插件init函数中调用） */
void test_plugin_set_suite_start(test_plugin_suite_start_fn fn);
void test_plugin_set_suite_end(test_plugin_suite_end_fn fn);
void test_plugin_set_test_start(test_plugin_test_start_fn fn);
void test_plugin_set_test_end(test_plugin_test_end_fn fn);
void test_plugin_set_assert_fail(test_plugin_assert_fail_fn fn);

/* ============================================================
 * 断言宏（全部带 TEST_ 前缀，防止命名冲突）
 * ============================================================ */

/* 通用断言 */
#define TEST_ASSERT(cond) \
    test_assert(__FILE__, __LINE__, #cond, (cond) ? 1 : 0)

/* 8位整数 */
#define TEST_EXPECT_EQ_U8(expected, actual) \
    test_assert_eq_u8(__FILE__, __LINE__, #expected, #actual, (uint8_t)(expected), (uint8_t)(actual))

#define TEST_EXPECT_EQ_I8(expected, actual) \
    test_assert_eq_i8(__FILE__, __LINE__, #expected, #actual, (int8_t)(expected), (int8_t)(actual))

/* 16位整数 */
#define TEST_EXPECT_EQ_U16(expected, actual) \
    test_assert_eq_u16(__FILE__, __LINE__, #expected, #actual, (uint16_t)(expected), (uint16_t)(actual))

#define TEST_EXPECT_EQ_I16(expected, actual) \
    test_assert_eq_i16(__FILE__, __LINE__, #expected, #actual, (int16_t)(expected), (int16_t)(actual))

/* 32位整数 */
#define TEST_EXPECT_EQ_U32(expected, actual) \
    test_assert_eq_u32(__FILE__, __LINE__, #expected, #actual, (uint32_t)(expected), (uint32_t)(actual))

#define TEST_EXPECT_EQ_I32(expected, actual) \
    test_assert_eq_i32(__FILE__, __LINE__, #expected, #actual, (int32_t)(expected), (int32_t)(actual))

/* 浮点数（带默认精度） */
#define TEST_EXPECT_EQ_F32(expected, actual) \
    test_assert_eq_f32(__FILE__, __LINE__, #expected, #actual, (float)(expected), (float)(actual), TEST_EPSILON_F32)

#define TEST_EXPECT_EQ_F64(expected, actual) \
    test_assert_eq_f64(__FILE__, __LINE__, #expected, #actual, (double)(expected), (double)(actual), TEST_EPSILON_F64)

/* 浮点数（自定义精度） */
#define TEST_EXPECT_EQ_F32_E(expected, actual, eps) \
    test_assert_eq_f32(__FILE__, __LINE__, #expected, #actual, (float)(expected), (float)(actual), (float)(eps))

#define TEST_EXPECT_EQ_F64_E(expected, actual, eps) \
    test_assert_eq_f64(__FILE__, __LINE__, #expected, #actual, (double)(expected), (double)(actual), (double)(eps))

/* 布尔 */
#define TEST_EXPECT_EQ_BOOL(expected, actual) \
    test_assert_eq_bool(__FILE__, __LINE__, #expected, #actual, (bool)(expected), (bool)(actual))

/* 指针 */
#define TEST_EXPECT_NULL(ptr) \
    test_assert_null(__FILE__, __LINE__, #ptr, (ptr))

#define TEST_EXPECT_NOT_NULL(ptr) \
    test_assert_not_null(__FILE__, __LINE__, #ptr, (ptr))

/* 字符串 */
#define TEST_EXPECT_EQ_STR(expected, actual) \
    test_assert_eq_str(__FILE__, __LINE__, #expected, #actual, (expected), (actual))

/* 内存 */
#define TEST_EXPECT_EQ_MEM(p1, p2, size) \
    test_assert_eq_mem(__FILE__, __LINE__, #p1, #p2, (p1), (p2), (size_t)(size))

/* TRUE/FALSE */
#define TEST_EXPECT_EQ_TRUE(actual) \
    test_assert(__FILE__, __LINE__, #actual " is true", (actual) ? 1 : 0)

#define TEST_EXPECT_EQ_FALSE(actual) \
    test_assert(__FILE__, __LINE__, #actual " is false", (actual) ? 0 : 1)

/* ============================================================
 * 断言函数声明（由 test.c 实现）
 * ============================================================ */

void test_assert(const char* file, int line, const char* expr, int passed);

void test_assert_eq_u8(const char* file, int line, const char* expr_e, const char* expr_a, uint8_t expected, uint8_t actual);
void test_assert_eq_i8(const char* file, int line, const char* expr_e, const char* expr_a, int8_t expected, int8_t actual);
void test_assert_eq_u16(const char* file, int line, const char* expr_e, const char* expr_a, uint16_t expected, uint16_t actual);
void test_assert_eq_i16(const char* file, int line, const char* expr_e, const char* expr_a, int16_t expected, int16_t actual);
void test_assert_eq_u32(const char* file, int line, const char* expr_e, const char* expr_a, uint32_t expected, uint32_t actual);
void test_assert_eq_i32(const char* file, int line, const char* expr_e, const char* expr_a, int32_t expected, int32_t actual);

void test_assert_eq_f32(const char* file, int line, const char* expr_e, const char* expr_a, float expected, float actual, float epsilon);
void test_assert_eq_f64(const char* file, int line, const char* expr_e, const char* expr_a, double expected, double actual, double epsilon);

void test_assert_eq_bool(const char* file, int line, const char* expr_e, const char* expr_a, bool expected, bool actual);

void test_assert_null(const char* file, int line, const char* expr, const void* ptr);
void test_assert_not_null(const char* file, int line, const char* expr, const void* ptr);

void test_assert_eq_str(const char* file, int line, const char* expr_e, const char* expr_a, const char* expected, const char* actual);
void test_assert_eq_mem(const char* file, int line, const char* expr_p1, const char* expr_p2, const void* p1, const void* p2, size_t size);

/* ============================================================
 * 兼容旧 API（全部映射到新的 TEST_ 前缀宏）
 * ============================================================ */

/* 旧宏映射到新宏（保留详细失败报告） */
#define TEST_ASSERT_EQUAL_INT(e, a)     TEST_EXPECT_EQ_I32(e, a)
#define TEST_ASSERT_EQUAL_UINT(e, a)    TEST_EXPECT_EQ_U32(e, a)
#define TEST_ASSERT_EQUAL_FLOAT(e, a)   TEST_EXPECT_EQ_F32(e, a)
#define TEST_ASSERT_EQUAL_STRING(e, a)  TEST_EXPECT_EQ_STR(e, a)
#define TEST_ASSERT_NOT_EQUAL_INT(e, a) TEST_ASSERT((e) != (a))
#define TEST_ASSERT_NOT_EQUAL_UINT(e, a) TEST_ASSERT((e) != (a))
#define TEST_ASSERT_TRUE(c)             TEST_EXPECT_EQ_TRUE(c)
#define TEST_ASSERT_FALSE(c)            TEST_EXPECT_EQ_FALSE(c)
#define TEST_ASSERT_NULL(p)             TEST_EXPECT_NULL(p)
#define TEST_ASSERT_NOT_NULL(p)         TEST_EXPECT_NOT_NULL(p)
#define TEST_ASSERT_EQUAL_PTR(e, a)     TEST_ASSERT((void*)(e) == (void*)(a))
#define TEST_ASSERT_EQUAL_MEMORY(e, a, s) TEST_EXPECT_EQ_MEM(e, a, s)
#define TEST_ASSERT_INT_WITHIN(min, max, a) TEST_ASSERT((a) >= (min) && (a) <= (max))
#define TEST_ASSERT_UINT_WITHIN(min, max, a) TEST_ASSERT((a) >= (min) && (a) <= (max))
#define TEST_ASSERT_EQUAL_HEX(e, a)     TEST_ASSERT((e) == (a))

#ifdef __cplusplus
}
#endif

#endif /* LIBCA_CORE_TEST_H */
