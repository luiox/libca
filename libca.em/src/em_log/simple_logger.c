#include "simple_logger.h"
#include <em_format/format.h>
#include <stdarg.h>
#include <stdio.h>   // 仅用于 vsnprintf


static slog_output_fn_t g_out_fn = NULL;

#if SLOG_ENABLE_RUNTIME_LEVEL
static u8 g_runtime_level = LOG_LEVEL_DEBUG;
#endif

/// @brief 统一格式化入口，可在标准 vsnprintf 与 fast_vsnprintf 之间切换
static i32 slog_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args)
{
#if SLOG_USE_FAST_VSNPRINTF
    return fmt_vsnprintf(buf, buf_size, fmt, args);
#else
    int len = vsnprintf(buf, buf_size, fmt, args);
    if (len < 0) {
        return -1;
    }
    return (i32)len;
#endif
}

void slog_init(slog_output_fn_t out_fn)
{
    SLOG_LOCK_ENTER();
    g_out_fn = out_fn;
    SLOG_LOCK_EXIT();
}

#if SLOG_ENABLE_RUNTIME_LEVEL
void slog_set_runtime_level(u8 level)
{
    if (level > LOG_LEVEL_DEBUG) {
        level = LOG_LEVEL_DEBUG;
    }
    g_runtime_level = level;
}

u8 slog_get_runtime_level(void)
{
    return g_runtime_level;
}

bool slog_should_log(u8 level)
{
    return g_runtime_level >= level;
}
#endif

// 唯一的底层函数
void _slog_printf(const char* fmt, ...)
{
    if (fmt == NULL) {
        return;
    }

    SLOG_LOCK_ENTER();
    if (g_out_fn == NULL) {
        SLOG_LOCK_EXIT();
        return;
    }

    char    buf[SLOG_BUFFER_SIZE];
    va_list args;

    va_start(args, fmt);
    i32 len = slog_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0) {
        SLOG_LOCK_EXIT();
        return;
    }

    // 防止缓冲区溢出截断导致的越界
    if ((usize)len > sizeof(buf) - 1U) {
        len = (i32)(sizeof(buf) - 1U);
    }

    g_out_fn((const u8*)buf, (usize)len);
    SLOG_LOCK_EXIT();
}
