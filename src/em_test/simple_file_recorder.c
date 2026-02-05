/*
 * @file simple_file_recorder.c
 * @brief 简单文件输出插件实现
 */

#include "simple_file_recorder.h"
#include "test.h"
#include <string.h>

static FILE* g_fp = NULL;
static test_output_fn g_original_output = NULL;

/* 同时输出到终端和文件的回调 */
static void test_output_to_both(const char* msg) {
    /* 输出到终端（通过原始回调） */
    if (g_original_output) {
        g_original_output(msg);
    } else {
        printf("%s", msg);
    }
    
    /* 输出到文件 */
    if (g_fp != NULL) {
        fprintf(g_fp, "%s", msg);
        fflush(g_fp);
    }
}

int test_file_recorder_init(const char* filepath, int append) {
    if (filepath == NULL) {
        return -1;
    }
    
    /* 关闭之前的文件（如果有） */
    test_file_recorder_close();
    
    /* 打开新文件 */
    const char* mode = append ? "a" : "w";
    g_fp = fopen(filepath, mode);
    if (g_fp == NULL) {
        return -1;
    }
    
    /* 保存原始输出回调 */
    g_original_output = NULL;  /* test.c 中需要实现获取当前回调的函数 */
    
    /* 设置新的输出回调 */
    test_set_output(test_output_to_both);
    
    return 0;
}

void test_file_recorder_close(void) {
    if (g_fp != NULL) {
        fclose(g_fp);
        g_fp = NULL;
    }
    
    /* 恢复原始输出回调 */
    test_set_output(NULL);
}

FILE* test_file_recorder_get_fp(void) {
    return g_fp;
}
