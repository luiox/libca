/*
 * @file simple_file_recorder.c
 * @brief 简单文件输出插件实现（使用新的插件自动注册机制）
 */

#include "test.h"
#include <stdio.h>
#include <stdlib.h>

static FILE* g_fp = NULL;

/* 插件回调函数 */
static void file_suite_start(int test_count)
{
    /* 从环境变量获取文件名，默认 test_report.txt */
    const char* filepath = getenv("EM_TEST_REPORT_FILE");
    if (filepath == NULL) {
        filepath = "test_report.txt";
    }

    g_fp = fopen(filepath, "w");
    if (g_fp == NULL) {
        fprintf(stderr,
                "Warning: em_test file_recorder plugin failed to open report file '%s'.\n",
                filepath);
        /* 打开失败，不设置回调 */
        return;
    }

    fprintf(g_fp, "Test Suite Started: %d tests\n", test_count);
    fprintf(g_fp, "================================\n");
}

static void file_test_start(const char* name)
{
    if (g_fp) {
        fprintf(g_fp, "[RUN] %s\n", name);
    }
}

static void file_test_end(const char* name, int passed)
{
    if (g_fp) {
        fprintf(g_fp, "[%s] %s\n", passed ? "PASS" : "FAIL", name);
    }
}

static void file_suite_end(int passed, int failed)
{
    if (g_fp) {
        fprintf(g_fp, "================================\n");
        fprintf(g_fp, "Results: %d passed, %d failed\n", passed, failed);
        fclose(g_fp);
        g_fp = NULL;
    }
}

/* 插件初始化函数 - 在TEST_PLUGIN_REGISTER中指定 */
static void file_recorder_init(void)
{
    /* 设置回调函数 */
    test_plugin_set_suite_start(file_suite_start);
    test_plugin_set_test_start(file_test_start);
    test_plugin_set_test_end(file_test_end);
    test_plugin_set_suite_end(file_suite_end);
}

/* 自动注册插件 - 只需一行 */
TEST_PLUGIN_REGISTER(file_recorder, file_recorder_init);
