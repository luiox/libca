#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

static void (*g_hw_puts_output)(const char* str) = NULL;
// 内部打印缓冲区
static char g_print_buffer[PRINT_BUFFER_SIZE];

void debug_init(void (*hw_puts_output)(const char* str))
{
    g_hw_puts_output = hw_puts_output;
}

void debug_puts(const char* str)
{
    // 调用具体实现
    g_hw_puts_output(str);
}

void debug_printf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    vsprintf(g_print_buffer, fmt, args);

    va_end(args);

    debug_puts((const char*)g_print_buffer);
}

#if TEST_ENABLE
#include "../em_test/test.h"
#include <string.h>

// 测试用的捕获缓冲区
static char test_last_msg[PRINT_BUFFER_SIZE];

static void test_hw_puts_cb(const char* s)
{
    // 拷贝到捕获缓冲区，并打印到控制台
    strncpy(test_last_msg, s, sizeof(test_last_msg) - 1);
    test_last_msg[sizeof(test_last_msg) - 1] = '\0';
    printf("HW输出: %s\n", s);
}

// 测试 debug_puts 与 debug_printf
TEST_CASE(debug_puts_and_printf)
{
    memset(test_last_msg, 0, sizeof(test_last_msg));

    debug_init(test_hw_puts_cb);

    debug_puts("abc");
    TEST_ASSERT_EQUAL_STRING("abc", test_last_msg);

    debug_printf("hello %d", 123);
    TEST_ASSERT_EQUAL_STRING("hello 123", test_last_msg);
}

#endif // TEST_ENABLE
