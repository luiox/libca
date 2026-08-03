#include "../../cpu_adapter.h"

#if USE_CUSTOM_CPU_ADAPTER

/* PTHREAD_MUTEX_RECURSIVE 等常量需 _GNU_SOURCE 才在 strict c99 下暴露（glibc）。
   必须在所有系统头之前定义。本文件仅在 Linux 平台编译。 */
#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <pthread.h>

static pthread_mutex_t g_log_mutex;
static bool g_log_mutex_init = false;

void local_cpu_enter_critical(void)
{
    if (!g_log_mutex_init) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        /* Use recursive mutex to allow nested enter/exit in same thread */
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&g_log_mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        g_log_mutex_init = true;
    }
    pthread_mutex_lock(&g_log_mutex);
}

void local_cpu_exit_critical(void)
{
    pthread_mutex_unlock(&g_log_mutex);
}

#endif // USE_CUSTOM_CPU_ADAPTER
