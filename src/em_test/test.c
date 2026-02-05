/**
 * @file test.c
 * @brief em_test 核心实现 - 集成结构化输出系统
 */

#include "test.h"
#include <time.h>

#if defined(_WIN32)
    #include <windows.h>
#endif

/* ==================== 全局变量 ==================== */

int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;
int current_test_failed = 0;

#if defined(_MSC_VER)
__declspec(allocate(".test$a")) const test_t* _test_start = NULL;
__declspec(allocate(".test$z")) const test_t* _test_stop  = NULL;
#else
TEST_CASE_ALLOC const test_t* _test_dummy = NULL;
#endif

/* ==================== 结构化输出相关 ==================== */

static bool g_use_structured_output = false;
static uint64_t g_suite_start_time = 0;
static uint64_t g_test_start_time = 0;
static test_case_info_t g_current_test_info = {0};
static uint32_t g_current_assertion_count = 0;

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t get_timestamp_ms(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    return (ull.QuadPart / 10000) - 11644473600000ULL;
#elif defined(__MACH__) && defined(__APPLE__)
    /* macOS */
    #include <mach/mach_time.h>
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t time = mach_absolute_time();
    return (time * timebase.numer / timebase.denom) / 1000000;
#elif defined(CLOCK_MONOTONIC)
    /* Linux and POSIX with clock_gettime */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
    /* Fallback */
    return (uint64_t)clock() * 1000 / CLOCKS_PER_SEC;
#else
    /* Standard C fallback */
    return (uint64_t)clock() * 1000 / CLOCKS_PER_SEC;
#endif
}

void test_set_structured_output(bool enable)
{
    g_use_structured_output = enable;
    if (enable) {
        test_output_init();
    }
}

bool test_is_structured_output_enabled(void)
{
    return g_use_structured_output;
}

/*
 * 增加断言计数的简单接口：每个断言宏在被执行时应调用它一次
 * 这样 test.c 内部维护的 g_current_assertion_count 会被更新
 */
void test_assertion_inc(void)
{
    g_current_assertion_count++;
}

/* 中央化失败上报：
 * - 若启用结构化输出，调用 test_assert_failed_detail 上报
 * - 始终打印到控制台（目前使用 test_print，未来可以重定向）
 */
#include <stdarg.h>

void test_print(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void test_report_failure(const char* expression, const char* expected_str, const char* actual_str, const char* printf_fmt, ...)
{
    if (test_is_structured_output_enabled()) {
        test_assert_failed_detail(__FILE__, __LINE__, expression, expected_str, actual_str);
    }

    va_list ap;
    va_start(ap, printf_fmt);
    vprintf(printf_fmt, ap);
    va_end(ap);
}

void test_suite_begin(const char* suite_name)
{
    if (!g_use_structured_output) return;
    
    g_suite_start_time = get_timestamp_ms();
    g_current_assertion_count = 0;
    
    test_suite_info_t suite_info = {
        .name = suite_name,
        .total_tests = 0,
        .passed_tests = 0,
        .failed_tests = 0,
        .start_time_ms = g_suite_start_time,
        .end_time_ms = 0
    };
    
    test_event_data_t data = {.suite = suite_info};
    test_output_emit(TEST_EVENT_SUITE_START, &data);
}

void test_suite_end(void)
{
    if (!g_use_structured_output) return;
    
    uint64_t end_time = get_timestamp_ms();
    
    test_suite_info_t suite_info = {
        .name = NULL,  /* 使用当前套件名称 */
        .total_tests = total_tests,
        .passed_tests = passed_tests,
        .failed_tests = failed_tests,
        .start_time_ms = g_suite_start_time,
        .end_time_ms = end_time
    };
    
    test_event_data_t data = {.suite = suite_info};
    test_output_emit(TEST_EVENT_SUITE_END, &data);
    test_output_flush();
}

void test_case_begin(const char* test_name, const char* file, uint32_t line)
{
    if (!g_use_structured_output) return;
    
    g_test_start_time = get_timestamp_ms();
    g_current_assertion_count = 0;
    
    g_current_test_info.name = test_name;
    g_current_test_info.file = file;
    g_current_test_info.line = line;
    g_current_test_info.start_time_ms = g_test_start_time;
    g_current_test_info.end_time_ms = 0;
    g_current_test_info.assertion_count = 0;
    
    test_event_data_t data = {.test = g_current_test_info};
    test_output_emit(TEST_EVENT_TEST_START, &data);
}

void test_case_end(bool passed)
{
    if (!g_use_structured_output) return;
    
    uint64_t end_time = get_timestamp_ms();
    g_current_test_info.end_time_ms = end_time;
    g_current_test_info.assertion_count = g_current_assertion_count;
    g_current_test_info.passed = passed;
    
    test_event_data_t data = {.test = g_current_test_info};
    
    /* 使用统一的 TEST_EVENT_TEST_END 事件，formatter 根据 data->test.passed 输出 PASS/FAIL */
    test_output_emit(TEST_EVENT_TEST_END, &data);
}

void test_assert_failed_detail(const char* file, uint32_t line,
                                const char* expression,
                                const char* expected,
                                const char* actual)
{
    if (!g_use_structured_output) return;

    fprintf(stderr, "[test] emit ASSERT_FAIL for %s:%u expr=%s\n", file ? file : "unknown", line, expression ? expression : "(null)");
    
    test_assert_fail_info_t fail_info = {
        .file = file,
        .line = line,
        .expression = expression,
        .message = NULL,
        .expected = expected,
        .actual = actual,
        .format = TEST_FORMAT_PLAIN
    };
    
    test_event_data_t data = {.assert_fail = fail_info};
    test_output_emit(TEST_EVENT_ASSERT_FAIL, &data);
}

/* ==================== 原有 run_tests 实现（向后兼容） ==================== */

int run_tests(void)
{
    total_tests = 0;
    passed_tests = 0;
    failed_tests = 0;
    
#if defined(_MSC_VER)
    const test_t** begin = (const test_t**)&_test_start;
    const test_t** end   = (const test_t**)&_test_stop;

    // 计算个数
    for (const test_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL) {
            total_tests++;
        }
    }
    
    if (!g_use_structured_output) {
        printf("num_tests = %d\n", total_tests);
    }
    
    // 发送套件开始事件
    if (g_use_structured_output) {
        test_suite_begin("test_suite");
    }

    for (const test_t** it = begin; it <= end; it++) {
        if (*it == NULL || (*it)->name == NULL) {
            continue;
        }
        
        if (!g_use_structured_output) {
            printf("Running test: %s\n", (*it)->name);
        }
        
        // 发送测试开始事件
        if (g_use_structured_output) {
            test_case_begin((*it)->name, "", 0);
        }
        
        current_test_failed = 0;
        g_current_assertion_count = 0;
        (*it)->func();
        
        bool test_passed = !current_test_failed;
        if (test_passed) {
            passed_tests++;
        } else {
            failed_tests++;
        }
        
        // 发送测试结束事件
        if (g_use_structured_output) {
            test_case_end(test_passed);
        }
    }
    
    // 发送套件结束事件
    if (g_use_structured_output) {
        test_suite_end();
    }
#else
    const test_t** begin = (const test_t**)__start_test_array;
    const test_t** end   = (const test_t**)__stop_test_array;

    for (const test_t** t = begin; t < end; t++) {
        if (*t != NULL && (*t)->name != NULL)
            total_tests++;
    }
    
    if (!g_use_structured_output) {
        printf("num_tests = %d\n", total_tests);
    }
    
    // 发送套件开始事件
    if (g_use_structured_output) {
        test_suite_begin("test_suite");
    }

    for (const test_t** t = begin; t < end; t++) {
        if (*t == NULL || (*t)->name == NULL)
            continue;
        
        if (!g_use_structured_output) {
            printf("Running test: %s\n", (*t)->name);
        }
        
        // 发送测试开始事件
        if (g_use_structured_output) {
            test_case_begin((*t)->name, "", 0);
        }
        
        current_test_failed = 0;
        g_current_assertion_count = 0;
        (*t)->func();
        
        bool test_passed = !current_test_failed;
        if (test_passed) {
            passed_tests++;
        } else {
            failed_tests++;
        }
        
        // 发送测试结束事件
        if (g_use_structured_output) {
            test_case_end(test_passed);
        }
    }
    
    // 发送套件结束事件
    if (g_use_structured_output) {
        test_suite_end();
    }
#endif
    
    if (!g_use_structured_output) {
        printf("\nTests finished: %d total, %d passed, %d failed\n", 
               total_tests, passed_tests, failed_tests);
    }
    
    return failed_tests;
}

/* ==================== 测试用例 ==================== */

#if TEST_ENABLE

#if TEST_SELF_MAIN
TEST_CASE(test_module)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
}
#endif // TEST_SELF_MAIN

#endif // TEST_ENABLE
