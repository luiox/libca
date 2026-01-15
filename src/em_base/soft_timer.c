#include "soft_timer.h"
#include <stddef.h>

static time_get_fn_t g_ms_provider = NULL;
static time_get_fn_t g_us_provider = NULL;
static volatile timestamp_t g_manual_ms_tick = 0;

void time_set_ms_provider(time_get_fn_t provider) {
    g_ms_provider = provider;
}

void time_update_tick_ms(timestamp_t ms_delta) {
    g_manual_ms_tick += ms_delta;
}

void time_set_us_provider(time_get_fn_t provider) {
    g_us_provider = provider;
}

timestamp_t time_get_ms(void) {
    if (g_ms_provider) {
        return g_ms_provider();
    }
    return g_manual_ms_tick;
}

timestamp_t time_get_us(void) {
    if (g_us_provider) {
        return g_us_provider();
    }
    // 如果没有 us 提供者，退而求其次返回 ms * 1000
    return time_get_ms() * 1000;
}

#if TEST_ENABLE

#include "../em_test/test.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>

static u32 win32_get_ms(void) {
    return GetTickCount();
}

// 模拟一个后台更新时间的线程
static void __cdecl timer_update_thread(void* arg) {
    (void)arg;
    while (1) {
        // 如果没有设置 provider，我们就手动更新 tick
        // 这里模拟每 10ms 更新一次
        time_update_tick_ms(10);
        Sleep(10);
    }
}

TEST_CASE(soft_timer_win32_sim)
{
    printf("Running Win32 soft_timer simulation...\n");
    
    // 1. 测试外部 Provider 模式 (直接对接 Win32 API)
    time_set_ms_provider((time_get_cb_t)win32_get_ms);
    
    soft_timer_t st;
    soft_timer_set(&st, 100); // 设置 100ms 超时
    
    TEST_ASSERT_EQUAL_INT(false, soft_timer_is_timeout(&st));
    Sleep(150);
    TEST_ASSERT_EQUAL_INT(true, soft_timer_is_timeout(&st));
    
    // 2. 测试手动更新模式 (模拟 SysTick)
    time_set_ms_provider(NULL); // 切换回手动模式
    time_update_tick_ms(0);     // 重置手动计数
    
    soft_timer_set(&st, 50);
    
    // 启动一个线程来模拟异步更新 tick
    HANDLE thread_handle = (HANDLE)_beginthread(timer_update_thread, 0, NULL);
    
    Sleep(100); // 等待线程更新 tick
    TEST_ASSERT_EQUAL_INT(true, soft_timer_is_timeout(&st));
    
    // 注意：实际项目中线程应该被正确关闭，这里仅作演示
    TerminateThread(thread_handle, 0);
}

#else
TEST_CASE(soft_timer_win32_sim)
{
    printf("Notice: soft_timer_win32_sim requires Windows environment, skipping...\n");
}
#endif

#endif


