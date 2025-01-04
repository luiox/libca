#include <libca/log.h>
#include <stdio.h>

logger_t g_logger;

static const char *level_strings[] = {
   "DEBUG", "INFO", "WARN", "ERROR"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif

void log_init(void* buf, uint32_t buf_size)
{
    g_logger.buf = buf;
    g_logger.buf_size=buf_size;
}

void log_log(int32_t level, const char *file, int32_t line, const char *fmt, ...)
{
    char buf[16];
//     fprintf(
//     g_logger.buf, "%s %-5s %s:%d: ",
//     buf, level_strings[level], file, line);
//   vfprintf(ev->udata, ev->fmt, ev->ap);
//   fprintf(ev->udata, "\n");
//   fflush(ev->udata);
}
