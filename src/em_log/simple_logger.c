#include "simple_logger.h"
#include <stdarg.h>
#include <stdio.h> // 仅用于 vsnprintf

static slog_output_fn_t g_out_fn = NULL;

void slog_init(slog_output_fn_t out_fn) {
    g_out_fn = out_fn;
}

// 唯一的底层函数
void _slog_printf(const char *fmt, ...) {
    if (g_out_fn == NULL) return;

    char buf[SLOG_BUFFER_SIZE];
    va_list args;
    
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // 防止缓冲区溢出截断导致的越界
    if (len > sizeof(buf) - 1) {
        len = sizeof(buf) - 1;
    }

    g_out_fn((u8 *)buf, (usize)len);
}
