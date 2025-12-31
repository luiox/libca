#include "async.h"
#include <string.h>

void async_init(async_t* async, void* buffer, u32 size, void (*on_notify)(void)) {
    if (!async || !buffer) return;
    
    ringbuffer_init(&async->rb, (u8*)buffer, size);
    async->on_notify = on_notify;
    async->lock = NULL;
    async->unlock = NULL;
}

bool async_submit(async_t* async, async_work_func_t func, void* arg) {
    if (!async || !func) return false;

    async_work_item_t item;
    item.func = func;
    item.arg = arg;

    if (async->lock) {
        async->lock();
    } else {
        EM_CPU_ENTER_CRITICAL();
    }

    // 检查空间是否足够 (sizeof(async_work_item_t))
    if (ringbuffer_free(&async->rb) < sizeof(async_work_item_t)) {
        if (async->unlock) {
            async->unlock();
        } else {
            EM_CPU_EXIT_CRITICAL();
        }
        return false;
    }

    // 写入队列
    u32 written = ringbuffer_write(&async->rb, (u8*)&item, sizeof(async_work_item_t));
    
    if (async->unlock) {
        async->unlock();
    } else {
        EM_CPU_EXIT_CRITICAL();
    }

    if (written == sizeof(async_work_item_t)) {
        if (async->on_notify) {
            async->on_notify();
        }
        return true;
    }

    return false;
}

u32 async_process(async_t* async, u32 max_items) {
    if (!async) return 0;

    u32 processed_count = 0;
    async_work_item_t item;
    bool has_item = true;

    while (has_item) {
        if (max_items > 0 && processed_count >= max_items) {
            break;
        }

        if (async->lock) {
            async->lock();
        } else {
            EM_CPU_ENTER_CRITICAL();
        }

        if (ringbuffer_used(&async->rb) >= sizeof(async_work_item_t)) {
            ringbuffer_read(&async->rb, (u8*)&item, sizeof(async_work_item_t));
            has_item = true;
        } else {
            has_item = false;
        }

        if (async->unlock) {
            async->unlock();
        } else {
            EM_CPU_EXIT_CRITICAL();
        }

        if (has_item && item.func) {
            item.func(item.arg);
            processed_count++;
        }
    }

    return processed_count;
}

u32 async_pending_count(async_t* async) {
    if (!async) return 0;
    return ringbuffer_used(&async->rb) / sizeof(async_work_item_t);
}

#if TEST_ENABLE

#include "../em_test/test.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>

// 为测试定义全局临界区模拟 Port
static CRITICAL_SECTION g_test_port_cs;
#undef EM_CPU_ENTER_CRITICAL
#undef EM_CPU_EXIT_CRITICAL
#define EM_CPU_ENTER_CRITICAL() EnterCriticalSection(&g_test_port_cs)
#define EM_CPU_EXIT_CRITICAL()  LeaveCriticalSection(&g_test_port_cs)

// 模拟任务执行的计数器
static volatile int g_task_count = 0;
static HANDLE g_semaphore = NULL;
static async_t g_async_test;
static u8 g_async_buf[1024];

// 任务函数
static void task_handler(void* arg) {
    (void)arg;
    InterlockedIncrement(&g_task_count);
}

// 通知回调
static void on_async_notify(void) {
    if (g_semaphore) {
        ReleaseSemaphore(g_semaphore, 1, NULL);
    }
}

// --- 场景 1: 专用工作线程 (模拟 RTOS) ---
static unsigned __stdcall worker_thread_func(void* arg) {
    async_t* async = (async_t*)arg;
    while (1) {
        // 等待通知
        WaitForSingleObject(g_semaphore, INFINITE);
        // 处理所有任务
        async_process(async, 0);
    }
    return 0;
}

TEST_CASE(test_async_worker_thread) {
    g_task_count = 0;
    g_semaphore = CreateSemaphore(NULL, 0, 100, NULL);
    InitializeCriticalSection(&g_test_port_cs);
    
    // 测试默认使用 EM_CPU_ENTER_CRITICAL (lock/unlock 为 NULL)
    async_init(&g_async_test, g_async_buf, sizeof(g_async_buf), on_async_notify);
    
    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, worker_thread_func, &g_async_test, 0, NULL);
    
    // 提交 10 个任务
    static int args[10];
    for (int i = 0; i < 10; i++) {
        args[i] = i;
        async_submit(&g_async_test, task_handler, &args[i]);
    }
    
    // 等待一段时间让线程处理
    Sleep(100);
    
    TEST_ASSERT_EQUAL_INT(10, g_task_count);
    
    TerminateThread(thread, 0);
    CloseHandle(thread);
    CloseHandle(g_semaphore);
    DeleteCriticalSection(&g_test_port_cs);
    g_semaphore = NULL;
}
#endif // _WIN32

// --- 场景 2: 主线程轮询 (模拟裸机) ---
TEST_CASE(test_async_main_polling) {
    g_task_count = 0;
    InitializeCriticalSection(&g_test_port_cs);
    
    async_init(&g_async_test, g_async_buf, sizeof(g_async_buf), NULL);
    
    // 提交 10 个任务
    static int args[10];
    for (int i = 0; i < 10; i++) {
        args[i] = i;
        async_submit(&g_async_test, task_handler, &args[i]);
    }
    
    // 模拟主循环轮询，每次只处理 3 个任务
    u32 processed;
    
    processed = async_process(&g_async_test, 3);
    TEST_ASSERT_EQUAL_UINT(3, processed);
    TEST_ASSERT_EQUAL_INT(3, g_task_count);
    
    processed = async_process(&g_async_test, 3);
    TEST_ASSERT_EQUAL_UINT(3, processed);
    TEST_ASSERT_EQUAL_INT(6, g_task_count);
    
    processed = async_process(&g_async_test, 10); // 处理剩余所有
    TEST_ASSERT_EQUAL_UINT(4, processed);
    TEST_ASSERT_EQUAL_INT(10, g_task_count);
    
    // 队列应该空了
    processed = async_process(&g_async_test, 10);
    TEST_ASSERT_EQUAL_UINT(0, processed);

    DeleteCriticalSection(&g_test_port_cs);
}

static int g_custom_lock_count = 0;
static void custom_lock(void) {
    g_custom_lock_count++;
    EnterCriticalSection(&g_test_port_cs);
}
static void custom_unlock(void) {
    LeaveCriticalSection(&g_test_port_cs);
}

TEST_CASE(test_async_custom_lock) {
    g_task_count = 0;
    g_custom_lock_count = 0;
    InitializeCriticalSection(&g_test_port_cs);
    
    async_init(&g_async_test, g_async_buf, sizeof(g_async_buf), NULL);
    g_async_test.lock = custom_lock;
    g_async_test.unlock = custom_unlock;
    
    int arg = 123;
    async_submit(&g_async_test, task_handler, &arg);
    
    // submit 应该调用了一次 lock
    TEST_ASSERT_EQUAL_INT(1, g_custom_lock_count);
    
    async_process(&g_async_test, 1);
    
    // process 内部也会调用 lock
    TEST_ASSERT_EQUAL_INT(2, g_custom_lock_count);
    TEST_ASSERT_EQUAL_INT(1, g_task_count);

    DeleteCriticalSection(&g_test_port_cs);
}

#endif // TEST_ENABLE
