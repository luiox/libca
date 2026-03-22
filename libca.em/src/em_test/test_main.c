/*
 * @file test_main.c
 * @brief 测试框架默认 main 函数
 * 
 * 当使用 xmake rule("em_test") 并设置 use_default_main=true 时
 * 此文件会被自动包含，提供默认的 main 函数
 */

#include "test.h"
#include <stdio.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    return run_tests();
}
