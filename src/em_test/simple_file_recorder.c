/*
 * @file simple_file_recorder.c
 * @brief 简单文件输出插件实现（使用新的插件自动注册机制）
 */

#include "simple_file_recorder.h"
#include "test.h"
#include <string.h>

static FILE* g_fp = NULL;
static int g_tests_run = 0;
static int g_tests_passed = 0;

/* 插件回调函数 */
static void file_suite_start(int test_count) {
    if (g_fp) {
        fprintf(g_fp, "Test Suite Started: %d tests\n", test_count);
        fprintf(g_fp, "================================\n");
    }
}

static void file_suite_end(int passed, int failed) {
    if (g_fp) {
        fprintf(g_fp, "================================\n");
        fprintf(g_fp, "Results: %d passed, %d failed\n", passed, failed);
        fclose(g_fp);
        g_fp = NULL;
    }
}

static void file_test_start(const char* test_name) {
    if (g_fp) {
        fprintf(g_fp, "[RUN] %s\n", test_name);
    }
}

static void file_test_end(const char* test_name, int passed) {
    if (g_fp) {
        fprintf(g_fp, "[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
    }
}

/* 插件初始化函数 - 在TEST_PLUGIN_REGISTER中指定 */
static void file_recorder_init(void) {
    /* 设置回调函数 */
    test_plugin_set_suite_start(file_suite_start);
    test_plugin_set_suite_end(file_suite_end);
    test_plugin_set_test_start(file_test_start);
    test_plugin_set_test_end(file_test_end);
    
    /* 打开文件 */
    const char* filepath = "test_report.txt";
    int append = 0;
    
    g_fp = fopen(filepath, append ? "a" : "w");
    if (g_fp == NULL) {
        /* 打开失败，不设置回调 */
        test_plugin_set_suite_start(NULL);
        test_plugin_set_suite_end(NULL);
        test_plugin_set_test_start(NULL);
        test_plugin_set_test_end(NULL);
    }
}

/* 自动注册插件 - 只需一行 */
TEST_PLUGIN_REGISTER(file_recorder, file_recorder_init);
