#include "../../cpu_adapter.h"

#if USE_CUSTOM_CPU_ADAPTER

#include <stdbool.h>
#include <windows.h>

static CRITICAL_SECTION g_log_cs;
static bool g_log_cs_init = false;

void local_cpu_enter_critical(void)
{
    if (!g_log_cs_init) {
        InitializeCriticalSection(&g_log_cs);
        g_log_cs_init = true;
    }
    EnterCriticalSection(&g_log_cs);
}

void local_cpu_exit_critical(void)
{
    LeaveCriticalSection(&g_log_cs);
}

#endif // USE_CUSTOM_CPU_ADAPTER
