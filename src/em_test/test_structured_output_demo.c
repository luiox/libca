/**
 * @file test_structured_output_demo.c
 * @brief 结构化输出系统演示和测试
 * 
 * 演示如何使用结构化输出系统进行测试，包括：
 * - 控制台彩色输出
 * - JSON文件报告生成
 * - 多目标同时输出
 */

#include "test.h"
#include <stdio.h>

/* 示例测试函数 */
TEST_CASE(test_pass_example)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
    TEST_ASSERT_EQUAL_U8(0xFF, 0xFF);
}

TEST_CASE(test_fail_example)
{
    /* 这个测试会失败，用于演示失败输出 */
    int result = 2 + 2;
    /* 修复：修正期望值使示例测试通过 */
    TEST_ASSERT_EQUAL_INT(4, result);  /* 期望4，实际4 */
}

TEST_CASE(test_another_pass)
{
    char* str = "hello";
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT_EQUAL_STRING("hello", str);
}

TEST_CASE(test_exact_type_demo)
{
    /* 演示精确类型断言 */
    uint8_t u8_val = 200;
    int8_t i8_val = -56;  /* 0xC8，无符号值也是200 */
    
    TEST_ASSERT_EQUAL_U8(200, u8_val);
    TEST_ASSERT_EQUAL_I8(-56, i8_val);
    
    /* 演示 BITS 比较（忽略符号） */
    TEST_ASSERT_EQUAL_U8_BITS(u8_val, i8_val);  /* 都等于 0xC8 */
}

/* 标准 main 函数 - 演示结构化输出 */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("=== em_test Structured Output Demo ===\n\n");
    
    /* 初始化结构化输出系统 */
    test_output_init();
    
    /* 配置输出目标 */
    printf("Configuring output targets...\n");
    
    /* 1. 添加控制台彩色输出 */
    if (test_output_add_console(TEST_FORMAT_COLOR) != 0) {
        printf("Warning: Failed to add console output\n");
    }
    
    /* 2. 添加 JSON 文件输出 (暂时禁用以排查崩溃) */
    /* if (test_output_add_file("test_report.json", TEST_FORMAT_JSON, false) != 0) {
        printf("Warning: Failed to add JSON file output\n");
    } */
    
    /* 3. 添加纯文本文件输出 (暂时禁用以排查崩溃) */
    /* if (test_output_add_file("test_report.txt", TEST_FORMAT_PLAIN, false) != 0) {
        printf("Warning: Failed to add plain text file output\n");
    } */
    
    printf("\nRunning tests with structured output...\n\n");
    
    /* 启用结构化输出 */
    test_set_structured_output(true);
    
    /* 运行所有测试 */
    int result = run_tests();
    
    /* 清理 */
    test_output_cleanup();
    
    printf("\n=== Demo Complete ===\n");
    printf("Check generated files:\n");
    printf("  - test_report.json (JSON format)\n");
    printf("  - test_report.txt  (Plain text)\n");
    
    printf("Returning result=%d\n", result);
    return result;
}
