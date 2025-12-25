#include "debug.h"
#include <stdio.h>
#include "compiler_helper.h"
#include "datatype.h"

static u8   g_debug_level = PRINT_LEVEL_DEFAULT;
static char g_print_buffer[PRINT_BUFFER_SIZE];

WEAK_FUNC void hw_puts_output(const char* str)
{
    unused_param(str);
}

void ca_puts(const char* str)
{
    // 调用具体实现
    hw_puts_output(str);
}

void ca_set_print_level(i8 level)
{
    g_debug_level = level;
}

void ca_printf(i8 level, const char* fmt, ...)
{
    if (level < g_debug_level) {
        return;
    }

    va_list args;

    va_start(args, fmt);

    vsprintf(g_print_buffer, fmt, args);

    va_end(args);

    ca_puts((const char*)g_print_buffer);
}

void ca_println(i8 level, const char* str)
{
    if (level < g_debug_level) {
        return;
    }
    ca_puts(str);
    ca_puts(PRINT_NEWLINE);
}

void ca_dprintf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    vsprintf(g_print_buffer, fmt, args);

    va_end(args);

    ca_puts((const char*)g_print_buffer);
}
