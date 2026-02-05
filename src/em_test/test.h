#ifndef LIBCA_CORE_TEST_H
#define LIBCA_CORE_TEST_H

#define LIBCA_TEST_SUCCESS 0
#define LIBCA_TEST_FAILURE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "../em_base/macro_util.h"

// 关闭clangformat对宏的格式化
// clang-format off
#define TEST_ASSERT_EQUAL_INT(expected, actual)                                                \
    TEST_ASSERT_COMPARE((expected), (actual), ((expected) != (actual)), #expected " == " #actual, "%d", "%d", "Test failed: %s:%d, expected %d, got %d\n", (int)(expected), (int)(actual))

#define TEST_ASSERT_EQUAL_UINT(expected, actual)                                               \
    TEST_ASSERT_COMPARE((expected), (actual), ((expected) != (actual)), #expected " == " #actual, "%u", "%u", "Test failed: %s:%d, expected %u, got %u\n", (unsigned int)(expected), (unsigned int)(actual))

#define TEST_ASSERT(condition)                                                                 \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        if (!(condition)) {                                                                    \
            if (test_is_structured_output_enabled()) {                                         \
                test_assert_failed_detail(__FILE__, __LINE__, #condition, NULL, NULL);        \
            }                                                                                  \
            printf("Test failed: %s:%d, condition %s is false\n", __FILE__, __LINE__, #condition);     \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

#define DEFAULT_EPSILON 0.000001

/*
 * 公共失败处理宏：
 * - 如果启用了结构化输出，调用 test_assert_failed_detail 上报失败（包含期望/实际文本）
 * - 打印简洁失败信息到 stdout
 * - 设置 current_test_failed 标志
 * printf_fmt 必须以 "%s:%d," 作为前缀以便传入 __FILE__, __LINE__
 */
#define TEST__REPORT_FAIL(expr, expected_str, actual_str, printf_fmt, ...)                     \
    do {                                                                                       \
        if (test_is_structured_output_enabled()) {                                             \
            test_assert_failed_detail(__FILE__, __LINE__, (expr), (expected_str), (actual_str)); \
        }                                                                                      \
        printf((printf_fmt), __FILE__, __LINE__, __VA_ARGS__);                                 \
        current_test_failed = 1;                                                               \
    } while (0)

/* 简化 snprintf 到期望/实际字符串 */
#define TEST__SNPRINTF(buf, fmt, val)                                                           \
    do { snprintf((buf), sizeof(buf), (fmt), (val)); } while (0)

/* 输出封装：统一上报并打印（方便将来替换输出实现） */
void test_report_failure(const char* expression, const char* expected_str, const char* actual_str, const char* printf_fmt, ...);
void test_print(const char* fmt, ...);

/* 通用比较宏模板：
 * expected, actual: 表达式
 * check_expr: 当为真时表示断言失败（例如 (expected) != (actual)）
 * expr_str: 表达式字符串，送入结构化上报
 * e_fmt/a_fmt: 用于 snprintf 构造期望/实际的格式字符串（例如 "%d"）
 * printf_fmt/...: 最终打印使用的格式与参数（printf 风格），注意 printf_fmt 前两参数为 "%s:%d,"
 */
#define TEST_ASSERT_COMPARE(expected, actual, check_expr, expr_str, e_fmt, a_fmt, printf_fmt, ...) \
    do {                                                                                           \
        test_assertion_inc();                                                                      \
        if (check_expr) {                                                                          \
            char __e_buf[128]; char __a_buf[128];                                                  \
            TEST__SNPRINTF(__e_buf, (e_fmt), (expected));                                          \
            TEST__SNPRINTF(__a_buf, (a_fmt), (actual));                                            \
            test_report_failure((expr_str), __e_buf, __a_buf, (printf_fmt), __VA_ARGS__);           \
        }                                                                                          \
    } while (0)

#define TEST_ASSERT_EQUAL_FLOAT_(expected, actual, epsilon)                                         \
    do {                                                                                           \
        test_assertion_inc();                                                                      \
        if (fabs((expected) - (actual)) > (epsilon)) {                                             \
            char __e_buf[64]; char __a_buf[64];                                                    \
            TEST__SNPRINTF(__e_buf, "%f", (double)(expected));                                   \
            TEST__SNPRINTF(__a_buf, "%f", (double)(actual));                                     \
            TEST_ASSERT_COMPARE((expected), (actual), fabs((expected) - (actual)) > (epsilon), #expected " == " #actual, "%f", "%f", "Test failed: %s:%d, expected %f, got %f\n", (double)(expected), (double)(actual)); \
        }                                                                                          \
    } while (0)

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual) TEST_ASSERT_EQUAL_FLOAT_(expected, actual, DEFAULT_EPSILON)

#define TEST_ASSERT_EQUAL_STRING(expected, actual)                                             \
    TEST_ASSERT_COMPARE((expected), (actual), (strcmp((expected), (actual)) != 0), #expected " == " #actual, "%s", "%s", "Test failed: %s:%d, expected \"%s\", got \"%s\"\n", (expected), (actual))

#define TEST_ASSERT_NOT_EQUAL_INT(expected, actual)                                            \
    TEST_ASSERT_COMPARE((expected), (actual), ((expected) == (actual)), #expected " != " #actual, "%d", "%d", "Test failed: %s:%d, expected %d to not equal %d\n", (int)(expected), (int)(actual))

#define TEST_ASSERT_NOT_EQUAL_UINT(expected, actual)                                           \
    TEST_ASSERT_COMPARE((expected), (actual), ((expected) == (actual)), #expected " != " #actual, "%u", "%u", "Test failed: %s:%d, expected %u to not equal %u\n", (unsigned int)(expected), (unsigned int)(actual))
#define TEST_ASSERT_TRUE(condition)                                                            \
    TEST_ASSERT_COMPARE((condition), (condition), (!(condition)), #condition " is true", "true", "false", "Test failed: %s:%d, expected true but got false\n")

#define TEST_ASSERT_FALSE(condition)                                                           \
    TEST_ASSERT_COMPARE((condition), (condition), ((condition)), #condition " is false", "false", "true", "Test failed: %s:%d, expected false but got true\n")

#define TEST_ASSERT_NULL(pointer)                                                              \
    TEST_ASSERT_COMPARE((pointer), (pointer), ((pointer) != NULL), #pointer " == NULL", "NULL", "non-NULL", "Test failed: %s:%d, expected NULL pointer\n")

#define TEST_ASSERT_NOT_NULL(pointer)                                                          \
    TEST_ASSERT_COMPARE((pointer), (pointer), ((pointer) == NULL), #pointer " != NULL", "non-NULL", "NULL", "Test failed: %s:%d, expected non-NULL pointer\n")

#define TEST_ASSERT_EQUAL_PTR(expected, actual)                                                \
    TEST_ASSERT_COMPARE((expected), (actual), ((expected) != (actual)), #expected " == " #actual, "%p", "%p", "Test failed: %s:%d, expected pointer %p, got %p\n", (void*)(expected), (void*)(actual))

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, size)                                       \
    TEST_ASSERT_COMPARE((expected), (actual), (memcmp((expected), (actual), (size)) != 0), "memcmp(...) == 0", "expected memory", "actual memory", "Test failed: %s:%d, memory mismatch\n")

#define TEST_ASSERT_INT_WITHIN(min, max, actual)                                               \
    TEST_ASSERT_COMPARE((actual), (actual), ((actual) < (min) || (actual) > (max)), #actual " in [" #min "," #max "]", "%d", "%d", "Test failed: %s:%d, expected %d to be within [%d, %d]\n", (int)(actual), (int)(min), (int)(max))

#define TEST_ASSERT_UINT_WITHIN(min, max, actual)                                              \
    TEST_ASSERT_COMPARE((actual), (actual), ((actual) < (min) || (actual) > (max)), #actual " in [" #min "," #max "]", "%u", "%u", "Test failed: %s:%d, expected %u to be within [%u, %u]\n", (unsigned int)(actual), (unsigned int)(min), (unsigned int)(max))

#define TEST_ASSERT_EQUAL_HEX(expected, actual)                                                \
    TEST_ASSERT_COMPARE((expected), (actual), ((expected) != (actual)), #expected " == " #actual, "0x%X", "0x%X", "Test failed: %s:%d, expected 0x%X, got 0x%X\n", (unsigned int)(expected), (unsigned int)(actual))

/*
 * 精确类型断言宏
 * 避免C语言整型提升导致的问题，强制按指定类型比较
 * 适用于u8/u16/u32/i8/i16/i32等明确位宽的类型
 */

// 无符号8位
#define TEST_ASSERT_EQUAL_U8(expected, actual)                                                 \
    TEST_ASSERT_COMPARE((expected), (actual), ((uint8_t)(expected) != (uint8_t)(actual)), #expected " == " #actual, "%u (0x%02X)", "%u (0x%02X)", "Test failed: %s:%d, expected u8 %u (0x%02X), got %u (0x%02X)\n", (unsigned int)(uint8_t)(expected), (unsigned int)(uint8_t)(expected), (unsigned int)(uint8_t)(actual), (unsigned int)(uint8_t)(actual))

// 无符号16位
#define TEST_ASSERT_EQUAL_U16(expected, actual)                                                \
    TEST_ASSERT_COMPARE((expected), (actual), ((uint16_t)(expected) != (uint16_t)(actual)), #expected " == " #actual, "%u (0x%04X)", "%u (0x%04X)", "Test failed: %s:%d, expected u16 %u (0x%04X), got %u (0x%04X)\n", (unsigned int)(uint16_t)(expected), (unsigned int)(uint16_t)(expected), (unsigned int)(uint16_t)(actual), (unsigned int)(uint16_t)(actual))

// 无符号32位
#define TEST_ASSERT_EQUAL_U32(expected, actual)                                                \
    TEST_ASSERT_COMPARE((expected), (actual), ((uint32_t)(expected) != (uint32_t)(actual)), #expected " == " #actual, "%lu (0x%08lX)", "%lu (0x%08lX)", "Test failed: %s:%d, expected u32 %lu (0x%08lX), got %lu (0x%08lX)\n", (unsigned long)(uint32_t)(expected), (unsigned long)(uint32_t)(expected), (unsigned long)(uint32_t)(actual), (unsigned long)(uint32_t)(actual))

// 有符号8位
#define TEST_ASSERT_EQUAL_I8(expected, actual)                                                 \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        int8_t _e = (int8_t)(expected);                                                        \
        int8_t _a = (int8_t)(actual);                                                          \
        if (_e != _a) {                                                                        \
            char _e_buf[32]; char _a_buf[32];                                                  \
            snprintf(_e_buf, sizeof(_e_buf), "%d (0x%02X)", (int)_e, (unsigned int)(uint8_t)_e); \
            snprintf(_a_buf, sizeof(_a_buf), "%d (0x%02X)", (int)_a, (unsigned int)(uint8_t)_a); \
            if (test_is_structured_output_enabled()) {                                         \
                test_assert_failed_detail(__FILE__, __LINE__, #expected " == " #actual, _e_buf, _a_buf); \
            }                                                                                  \
            printf("Test failed: %s:%d, expected i8 %d (0x%02X), got %d (0x%02X)\n",           \
                   __FILE__, __LINE__, (int)_e, (unsigned int)(uint8_t)_e,                   \
                   (int)_a, (unsigned int)(uint8_t)_a);                                        \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

// 有符号16位
#define TEST_ASSERT_EQUAL_I16(expected, actual)                                                \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        int16_t _e = (int16_t)(expected);                                                      \
        int16_t _a = (int16_t)(actual);                                                        \
        if (_e != _a) {                                                                        \
            char __e_buf[32]; char __a_buf[32];                                                \
            TEST__SNPRINTF(__e_buf, "%d (0x%04X)", (int)_e, (unsigned int)(uint16_t)_e);     \
            TEST__SNPRINTF(__a_buf, "%d (0x%04X)", (int)_a, (unsigned int)(uint16_t)_a);     \
            TEST__REPORT_FAIL(#expected " == " #actual, __e_buf, __a_buf,                     \
                             "Test failed: %s:%d, expected i16 %d (0x%04X), got %d (0x%04X)\n", (int)_e, (unsigned int)(uint16_t)_e, (int)_a, (unsigned int)(uint16_t)_a); \
        }                                                                                      \
    } while (0)

// 有符号32位
#define TEST_ASSERT_EQUAL_I32(expected, actual)                                                \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        int32_t _e = (int32_t)(expected);                                                      \
        int32_t _a = (int32_t)(actual);                                                        \
        if (_e != _a) {                                                                        \
            char __e_buf[48]; char __a_buf[48];                                                \
            TEST__SNPRINTF(__e_buf, "%ld (0x%08lX)", (long)_e);                              \
            TEST__SNPRINTF(__a_buf, "%ld (0x%08lX)", (long)_a);                              \
            TEST__REPORT_FAIL(#expected " == " #actual, __e_buf, __a_buf,                     \
                             "Test failed: %s:%d, expected i32 %ld (0x%08lX), got %ld (0x%08lX)\n", (long)_e, (unsigned long)(uint32_t)_e, (long)_a, (unsigned long)(uint32_t)_a); \
        }                                                                                      \
    } while (0)

/*
 * 位宽比较宏（处理符号混用场景）
 * 强制按指定位宽的无符号类型比较，忽略符号差异
 * 适用于需要比较二进制内容而非数值大小的场景
 */

// 8位无符号比较（处理有符号/无符号8位值混用）
#define TEST_ASSERT_EQUAL_U8_BITS(value1, value2)                                              \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        uint8_t _v1 = (uint8_t)(value1);                                                       \
        uint8_t _v2 = (uint8_t)(value2);                                                       \
        if (_v1 != _v2) {                                                                      \
            char _e_buf[64]; char _a_buf[64];                                                  \
            snprintf(_e_buf, sizeof(_e_buf), "0x%02X (decimal: %u, signed: %d)", (unsigned int)_v1, (unsigned int)_v1, (int)(int8_t)_v1); \
            snprintf(_a_buf, sizeof(_a_buf), "0x%02X (decimal: %u, signed: %d)", (unsigned int)_v2, (unsigned int)_v2, (int)(int8_t)_v2); \
            if (test_is_structured_output_enabled()) {                                         \
                test_assert_failed_detail(__FILE__, __LINE__, #value1 " == " #value2, _e_buf, _a_buf); \
            }                                                                                  \
            printf("Test failed: %s:%d, 8-bit mismatch: 0x%02X != 0x%02X "                    \
                   "(decimal: %u != %u, signed: %d != %d)\n",                                  \
                   __FILE__, __LINE__, (unsigned int)_v1, (unsigned int)_v2,                 \
                   (unsigned int)_v1, (unsigned int)_v2, (int)(int8_t)_v1, (int)(int8_t)_v2);  \
            current_test_failed = 1;                                                           \
        }                                                                                      \
    } while (0)

// 16位无符号比较
#define TEST_ASSERT_EQUAL_U16_BITS(value1, value2)                                             \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        uint16_t _v1 = (uint16_t)(value1);                                                     \
        uint16_t _v2 = (uint16_t)(value2);                                                     \
        if (_v1 != _v2) {                                                                      \
            char __e_buf[64]; char __a_buf[64];                                                \
            TEST__SNPRINTF(__e_buf, "0x%04X (decimal: %u, signed: %d)", (unsigned int)_v1, (unsigned int)_v1, (int)(int16_t)_v1); \
            TEST__SNPRINTF(__a_buf, "0x%04X (decimal: %u, signed: %d)", (unsigned int)_v2, (unsigned int)_v2, (int)(int16_t)_v2); \
            TEST__REPORT_FAIL(#value1 " == " #value2, __e_buf, __a_buf,                          \
                             "Test failed: %s:%d, 16-bit mismatch: 0x%04X != 0x%04X (decimal: %u != %u, signed: %d != %d)\n", (unsigned int)_v1, (unsigned int)_v2, (unsigned int)_v1, (unsigned int)_v2, (int)(int16_t)_v1, (int)(int16_t)_v2); \
        }                                                                                      \
    } while (0)

// 32位无符号比较
#define TEST_ASSERT_EQUAL_U32_BITS(value1, value2)                                             \
    do {                                                                                       \
        test_assertion_inc();                                                                  \
        uint32_t _v1 = (uint32_t)(value1);                                                     \
        uint32_t _v2 = (uint32_t)(value2);                                                     \
        if (_v1 != _v2) {                                                                      \
            char __e_buf[64]; char __a_buf[64];                                                \
            TEST__SNPRINTF(__e_buf, "0x%08lX (decimal: %lu, signed: %ld)", (unsigned long)_v1, (unsigned long)_v1, (long)(int32_t)_v1); \
            TEST__SNPRINTF(__a_buf, "0x%08lX (decimal: %lu, signed: %ld)", (unsigned long)_v2, (unsigned long)_v2, (long)(int32_t)_v2); \
            TEST__REPORT_FAIL(#value1 " == " #value2, __e_buf, __a_buf,                          \
                             "Test failed: %s:%d, 32-bit mismatch: 0x%08lX != 0x%08lX (decimal: %lu != %lu, signed: %ld != %ld)\n", (unsigned long)_v1, (unsigned long)_v2, (unsigned long)_v1, (unsigned long)_v2, (long)(int32_t)_v1, (long)(int32_t)_v2); \
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

/* ==================== 结构化输出系统 ==================== */

/**
 * @brief 启用结构化输出系统
 * 
 * 调用此函数后，测试框架会自动将测试事件发送到所有配置的输出目标
 * 如果不调用，则保持原有的 printf 输出行为（向后兼容）
 */

#include "test_output.h"

/**
 * @brief 初始化测试套件（用于结构化输出）
 * @param suite_name 测试套件名称
 */
void test_suite_begin(const char* suite_name);

/**
 * @brief 结束测试套件（用于结构化输出）
 */
void test_suite_end(void);

/**
 * @brief 通知单个测试开始（用于结构化输出）
 * @param test_name 测试名称
 * @param file 源文件
 * @param line 行号
 */
void test_case_begin(const char* test_name, const char* file, uint32_t line);

/**
 * @brief 通知单个测试结束（用于结构化输出）
 * @param passed 是否通过
 */
void test_case_end(bool passed);

/**
 * @brief 通知断言失败详情（用于结构化输出）
 */
void test_assert_failed_detail(const char* file, uint32_t line, 
                                const char* expression,
                                const char* expected, 
                                const char* actual);

/**
 * @brief 设置是否使用结构化输出
 * @param enable true启用，false禁用（默认false，向后兼容）
 */
void test_set_structured_output(bool enable);

/**
 * @brief 获取结构化输出启用状态
 */
bool test_is_structured_output_enabled(void);

/**
 * @brief 增加当前测试的断言计数（每个断言调用时都应调用）
 */
void test_assertion_inc(void);

#endif   // !LIBCA_CORE_TEST_H

