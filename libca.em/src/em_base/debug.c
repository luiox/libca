#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

void debug_default_output(const char* str)
{
    // 默认我们什么也不做
    // 这样子可以避免不debug_init时候调用debug_puts导致的空指针问题
}

static void (*g_hw_puts_output)(const char* str) = debug_default_output;
// 内部打印缓冲区
static char g_print_buffer[CA_PRINT_BUFFER_SIZE];

void debug_init(void (*hw_puts_output)(const char* str))
{
    param_check(hw_puts_output != NULL);
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

    vsnprintf(g_print_buffer, sizeof(g_print_buffer), fmt, args);

    va_end(args);

    debug_puts((const char*)g_print_buffer);
}
