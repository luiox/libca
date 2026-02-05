/**
 * @file test_output.c
 * @brief 结构化测试输出系统实现
 */

#include "test_output.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <io.h>
    #define isatty _isatty
    #define fileno _fileno
#else
    #include <unistd.h>
#endif

/* ==================== 内部数据结构 ==================== */

static test_output_manager_t g_output_manager = {0};

/* 缓冲区，避免频繁内存分配 */
static char g_output_buffer[TEST_OUTPUT_BUF_SIZE];

/* ==================== 工具函数 ==================== */

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
    #else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    #endif
}

/**
 * @brief 获取ISO 8601格式时间字符串
 */
static void get_iso_timestamp(char* buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

/**
 * @brief 检查是否支持彩色输出
 */
static bool is_color_supported(FILE* fp)
{
    #if defined(_WIN32)
        return isatty(fileno(fp)) != 0;
    #else
        return isatty(fileno(fp)) != 0;
    #endif
}

/* ==================== 格式化器实现 ==================== */

/* ----- 纯文本格式化器 ----- */

typedef struct {
    char timestamp[32];
} plain_formatter_data_t;

static int plain_formatter_init(test_formatter_t* self, void* config)
{
    (void)config;
    plain_formatter_data_t* data = calloc(1, sizeof(plain_formatter_data_t));
    if (!data) return -1;
    self->priv_data = data;
    get_iso_timestamp(data->timestamp, sizeof(data->timestamp));
    return 0;
}

static void plain_formatter_cleanup(test_formatter_t* self)
{
    if (self->priv_data) {
        free(self->priv_data);
        self->priv_data = NULL;
    }
}

static int plain_formatter_on_event(test_formatter_t* self, 
                                     test_output_event_t event,
                                     const test_event_data_t* data)
{
    (void)self;
    char* buf = g_output_buffer;
    size_t buf_size = sizeof(g_output_buffer);
    int len = 0;
    
    switch (event) {
        case TEST_EVENT_SUITE_START:
            len = snprintf(buf, buf_size,
                "===============================================================================\n"
                "Test Suite: %s\n"
                "Started: %s\n"
                "===============================================================================\n",
                data->suite.name,
                ((plain_formatter_data_t*)self->priv_data)->timestamp);
            break;
            
        case TEST_EVENT_SUITE_END:
            len = snprintf(buf, buf_size,
                "===============================================================================\n"
                "Tests finished: %u total, %u passed, %u failed\n"
                "Duration: %llu ms\n"
                "===============================================================================\n",
                data->suite.total_tests,
                data->suite.passed_tests,
                data->suite.failed_tests,
                (unsigned long long)(data->suite.end_time_ms - data->suite.start_time_ms));
            break;
            
        case TEST_EVENT_TEST_START:
            len = snprintf(buf, buf_size, "Running: %s ... ", data->test.name);
            break;
            
        case TEST_EVENT_TEST_END:
            if (data->test.passed) {
                len = snprintf(buf, buf_size, "PASS (%llu ms)\n",
                    (unsigned long long)(data->test.end_time_ms - data->test.start_time_ms));
            } else {
                len = snprintf(buf, buf_size, "FAIL (%llu ms)\n",
                    (unsigned long long)(data->test.end_time_ms - data->test.start_time_ms));
            }
            break;
            
        case TEST_EVENT_ASSERT_FAIL:
            len = snprintf(buf, buf_size,
                "  [ASSERT FAIL] %s:%u\n"
                "    Expression: %s\n"
                "    Expected:   %s\n"
                "    Actual:     %s\n",
                data->assert_fail.file ? data->assert_fail.file : "unknown",
                data->assert_fail.line,
                data->assert_fail.expression ? data->assert_fail.expression : "N/A",
                data->assert_fail.expected ? data->assert_fail.expected : "N/A",
                data->assert_fail.actual ? data->assert_fail.actual : "N/A");
            break;
            
        default:
            return 0;
    }
    
    /* 这里不直接输出，而是返回格式化后的字符串给调用者 */
    return len > 0 ? len : 0;
}

static int plain_formatter_flush(test_formatter_t* self)
{
    (void)self;
    return 0;
}

/* ----- 彩色文本格式化器（控制台） ----- */

#if defined(_WIN32)
    #define COLOR_RESET    ""
    #define COLOR_RED      ""
    #define COLOR_GREEN    ""
    #define COLOR_YELLOW   ""
    #define COLOR_BLUE     ""
    #define COLOR_CYAN     ""
    #define COLOR_GRAY     ""
#else
    #define COLOR_RESET    "\033[0m"
    #define COLOR_RED      "\033[31m"
    #define COLOR_GREEN    "\033[32m"
    #define COLOR_YELLOW   "\033[33m"
    #define COLOR_BLUE     "\033[34m"
    #define COLOR_CYAN     "\033[36m"
    #define COLOR_GRAY     "\033[90m"
#endif

typedef struct {
    bool use_color;
} color_formatter_data_t;

static int color_formatter_init(test_formatter_t* self, void* config)
{
    (void)config;
    color_formatter_data_t* data = calloc(1, sizeof(color_formatter_data_t));
    if (!data) return -1;
    
    /* 默认根据环境检测 */
    data->use_color = is_color_supported(stdout);
    
    /* 可以通过配置强制开关 */
    if (config) {
        data->use_color = *(bool*)config;
    }
    
    /* 检查环境变量 */
    const char* no_color = getenv("NO_COLOR");
    const char* force_color = getenv("FORCE_COLOR");
    if (no_color && *no_color) data->use_color = false;
    if (force_color && *force_color) data->use_color = true;
    
    self->priv_data = data;
    return 0;
}

static void color_formatter_cleanup(test_formatter_t* self)
{
    if (self->priv_data) {
        free(self->priv_data);
        self->priv_data = NULL;
    }
}

static int color_formatter_on_event(test_formatter_t* self,
                                     test_output_event_t event,
                                     const test_event_data_t* data)
{
    color_formatter_data_t* fmt_data = (color_formatter_data_t*)self->priv_data;
    bool color = fmt_data->use_color;
    char* buf = g_output_buffer;
    size_t buf_size = sizeof(g_output_buffer);
    int len = 0;
    
    #define C(c) (color ? (c) : "")
    
    switch (event) {
        case TEST_EVENT_SUITE_START:
            len = snprintf(buf, buf_size,
                "%s═══════════════════════════════════════════════════════════════════════════════%s\n"
                "%s  Test Suite:%s %s%s%s\n"
                "%s═══════════════════════════════════════════════════════════════════════════════%s\n",
                C(COLOR_CYAN), C(COLOR_RESET),
                C(COLOR_BLUE), C(COLOR_RESET), C(COLOR_YELLOW), data->suite.name, C(COLOR_RESET),
                C(COLOR_CYAN), C(COLOR_RESET));
            break;
            
        case TEST_EVENT_SUITE_END: {
            const char* result_color = (data->suite.failed_tests == 0) ? COLOR_GREEN : COLOR_RED;
            len = snprintf(buf, buf_size,
                "%s═══════════════════════════════════════════════════════════════════════════════%s\n"
                "  Results: %s%u total%s, %s%u passed%s, %s%u failed%s\n"
                "  Duration: %s%llu ms%s\n"
                "%s═══════════════════════════════════════════════════════════════════════════════%s\n",
                C(COLOR_CYAN), C(COLOR_RESET),
                C(COLOR_BLUE), data->suite.total_tests, C(COLOR_RESET),
                C(COLOR_GREEN), data->suite.passed_tests, C(COLOR_RESET),
                C(result_color), data->suite.failed_tests, C(COLOR_RESET),
                C(COLOR_GRAY), (unsigned long long)(data->suite.end_time_ms - data->suite.start_time_ms), C(COLOR_RESET),
                C(COLOR_CYAN), C(COLOR_RESET));
            break;
        }
            
        case TEST_EVENT_TEST_START:
            len = snprintf(buf, buf_size, "  %s●%s %s%s%s ... ",
                C(COLOR_GRAY), C(COLOR_RESET),
                C(COLOR_BLUE), data->test.name, C(COLOR_RESET));
            break;
            
        case TEST_EVENT_TEST_END:
            if (data->test.passed) {
                len = snprintf(buf, buf_size, "%s✓ PASS%s (%s%llu ms%s)\n",
                    C(COLOR_GREEN), C(COLOR_RESET),
                    C(COLOR_GRAY), (unsigned long long)(data->test.end_time_ms - data->test.start_time_ms), C(COLOR_RESET));
            } else {
                len = snprintf(buf, buf_size, "%s✗ FAIL%s (%s%llu ms%s)\n",
                    C(COLOR_RED), C(COLOR_RESET),
                    C(COLOR_GRAY), (unsigned long long)(data->test.end_time_ms - data->test.start_time_ms), C(COLOR_RESET));
            }
            break;
            
        case TEST_EVENT_ASSERT_FAIL:
            len = snprintf(buf, buf_size,
                "    %s├─%s %sAssert Fail%s: %s%s%s:%s%u%s\n"
                "    %s├─%s %sExpression:%s %s\n"
                "    %s├─%s %sExpected:%s   %s\n"
                "    %s└─%s %sActual:%s     %s\n",
                C(COLOR_RED), C(COLOR_RESET),
                C(COLOR_YELLOW), C(COLOR_RESET), C(COLOR_GRAY), 
                data->assert_fail.file ? data->assert_fail.file : "unknown", C(COLOR_RESET),
                C(COLOR_YELLOW), data->assert_fail.line, C(COLOR_RESET),
                C(COLOR_RED), C(COLOR_RESET), C(COLOR_BLUE), C(COLOR_RESET), 
                data->assert_fail.expression ? data->assert_fail.expression : "N/A",
                C(COLOR_RED), C(COLOR_RESET), C(COLOR_GREEN), C(COLOR_RESET),
                data->assert_fail.expected ? data->assert_fail.expected : "N/A",
                C(COLOR_RED), C(COLOR_RESET), C(COLOR_RED), C(COLOR_RESET),
                data->assert_fail.actual ? data->assert_fail.actual : "N/A");
            break;
            
        default:
            return 0;
    }
    
    #undef C
    return len > 0 ? len : 0;
}

static int color_formatter_flush(test_formatter_t* self)
{
    (void)self;
    return 0;
}

/* ----- JSON格式化器 ----- */

typedef struct {
    FILE* fp;
    bool first_test;
    bool first_assert;
    char suite_start_time[32];
} json_formatter_data_t;

static int json_formatter_init(test_formatter_t* self, void* config)
{
    FILE* fp = (FILE*)config;
    if (!fp) return -1;
    
    json_formatter_data_t* data = calloc(1, sizeof(json_formatter_data_t));
    if (!data) return -1;
    
    data->fp = fp;
    data->first_test = true;
    data->first_assert = true;
    get_iso_timestamp(data->suite_start_time, sizeof(data->suite_start_time));
    
    self->priv_data = data;
    
    /* 写入JSON头部 */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": \"1.0\",\n");
    fprintf(fp, "  \"generator\": \"em_test\",\n");
    
    return 0;
}

static void json_formatter_cleanup(test_formatter_t* self)
{
    json_formatter_data_t* data = (json_formatter_data_t*)self->priv_data;
    if (data && data->fp) {
        fprintf(data->fp, "}\n");
        fflush(data->fp);
    }
    if (data) {
        free(data);
        self->priv_data = NULL;
    }
}

static int json_formatter_on_event(test_formatter_t* self,
                                    test_output_event_t event,
                                    const test_event_data_t* data)
{
    json_formatter_data_t* fmt_data = (json_formatter_data_t*)self->priv_data;
    FILE* fp = fmt_data->fp;
    
    switch (event) {
        case TEST_EVENT_SUITE_START:
            fprintf(fp, "  \"timestamp\": \"%s\",\n", fmt_data->suite_start_time);
            fprintf(fp, "  \"suite\": {\n");
            fprintf(fp, "    \"name\": \"%s\",\n", data->suite.name ? data->suite.name : "unnamed");
            fprintf(fp, "    \"tests\": [\n");
            break;
            
        case TEST_EVENT_SUITE_END:
            fprintf(fp, "\n    ],\n");
            fprintf(fp, "    \"summary\": {\n");
            fprintf(fp, "      \"total\": %u,\n", data->suite.total_tests);
            fprintf(fp, "      \"passed\": %u,\n", data->suite.passed_tests);
            fprintf(fp, "      \"failed\": %u,\n", data->suite.failed_tests);
            fprintf(fp, "      \"duration_ms\": %llu\n", 
                (unsigned long long)(data->suite.end_time_ms - data->suite.start_time_ms));
            fprintf(fp, "    }\n");
            fprintf(fp, "  }\n");
            break;
            
        case TEST_EVENT_TEST_START:
            if (!fmt_data->first_test) {
                fprintf(fp, ",\n");
            }
            fmt_data->first_test = false;
            /* 在每个新测试开始时，重置断言标记（用于判断是否有失败详情） */
            fmt_data->first_assert = true;
            fprintf(fp, "      {\n");
            fprintf(fp, "        \"name\": \"%s\",\n", data->test.name ? data->test.name : "unnamed");
            fprintf(fp, "        \"file\": \"%s\",\n", data->test.file ? data->test.file : "");
            fprintf(fp, "        \"line\": %u,\n", data->test.line);
            break;
            
        case TEST_EVENT_TEST_END:
            /* 如果之前有断言失败写入了 failures 数组，则需要关闭数组 */
            if (!fmt_data->first_assert) {
                fprintf(fp, "\n        ],\n");
            }

            if (data->test.passed) {
                fprintf(fp, "        \"status\": \"passed\",\n");
                fprintf(fp, "        \"duration_ms\": %llu,\n",
                    (unsigned long long)(data->test.end_time_ms - data->test.start_time_ms));
                fprintf(fp, "        \"assertions\": %u\n", data->test.assertion_count);
                fprintf(fp, "      }");
            } else {
                fprintf(fp, "        \"status\": \"failed\",\n");
                fprintf(fp, "        \"duration_ms\": %llu,\n",
                    (unsigned long long)(data->test.end_time_ms - data->test.start_time_ms));
                fprintf(fp, "        \"assertions\": %u\n", data->test.assertion_count);
                fprintf(fp, "      }");
            }
            break;
            
        case TEST_EVENT_ASSERT_FAIL:
            /* 调试：记录是否触发 */
            /* 如果是第一次失败，需要输出 failures 数组开头 */
            if (fmt_data->first_assert) {
                fmt_data->first_assert = false;
                fprintf(fp, "        \"failures\": [\n");
            } else {
                fprintf(fp, ",\n");
            }

            fprintf(fp, "          {\n");
            fprintf(fp, "            \"file\": \"%s\",\n", 
                data->assert_fail.file ? data->assert_fail.file : "");
            fprintf(fp, "            \"line\": %u,\n", data->assert_fail.line);
            fprintf(fp, "            \"expression\": \"%s\",\n",
                data->assert_fail.expression ? data->assert_fail.expression : "");
            fprintf(fp, "            \"message\": \"%s\",\n",
                data->assert_fail.message ? data->assert_fail.message : "");
            fprintf(fp, "            \"expected\": \"%s\",\n",
                data->assert_fail.expected ? data->assert_fail.expected : "");
            fprintf(fp, "            \"actual\": \"%s\"\n",
                data->assert_fail.actual ? data->assert_fail.actual : "");
            fprintf(fp, "          }");
            break;
            
        default:
            return 0;
    }
    
    fflush(fp);
    return 0;
}

static int json_formatter_flush(test_formatter_t* self)
{
    json_formatter_data_t* data = (json_formatter_data_t*)self->priv_data;
    if (data && data->fp) {
        fflush(data->fp);
    }
    return 0;
}

/* ==================== 格式化器表 ==================== */

static test_formatter_t g_builtin_formatters[] = {
    {
        .name = "plain",
        .format = TEST_FORMAT_PLAIN,
        .init = plain_formatter_init,
        .cleanup = plain_formatter_cleanup,
        .on_event = plain_formatter_on_event,
        .flush = plain_formatter_flush,
        .priv_data = NULL
    },
    {
        .name = "color",
        .format = TEST_FORMAT_COLOR,
        .init = color_formatter_init,
        .cleanup = color_formatter_cleanup,
        .on_event = color_formatter_on_event,
        .flush = color_formatter_flush,
        .priv_data = NULL
    },
    {
        .name = "json",
        .format = TEST_FORMAT_JSON,
        .init = json_formatter_init,
        .cleanup = json_formatter_cleanup,
        .on_event = json_formatter_on_event,
        .flush = json_formatter_flush,
        .priv_data = NULL
    }
};

#define NUM_BUILTIN_FORMATTERS (sizeof(g_builtin_formatters) / sizeof(g_builtin_formatters[0]))

/* ==================== 公共API实现 ==================== */

int test_output_init(void)
{
    if (g_output_manager.initialized) {
        return 0;  /* 已经初始化 */
    }
    
    memset(&g_output_manager, 0, sizeof(g_output_manager));
    g_output_manager.log_level = TEST_LOG_INFO;
    
    /* 注册内置格式化器 */
    test_output_register_builtin_formatters();
    
    g_output_manager.initialized = true;
    return 0;
}

void test_output_cleanup(void)
{
    if (!g_output_manager.initialized) {
        return;
    }
    
    /* 清理所有目标 */
    for (uint32_t i = 0; i < g_output_manager.target_count; i++) {
        test_output_target_entry_t* target = &g_output_manager.targets[i];
        
        if (target->formatter && target->formatter->cleanup) {
            target->formatter->cleanup(target->formatter);
        }
        
        if (target->type == TEST_TARGET_FILE && target->target.file.fp) {
            fclose(target->target.file.fp);
            target->target.file.fp = NULL;
        }
        
        if (target->type == TEST_TARGET_FILE && target->target.file.path) {
            free(target->target.file.path);
            target->target.file.path = NULL;
        }
    }
    
    memset(&g_output_manager, 0, sizeof(g_output_manager));
}

int test_output_register_builtin_formatters(void)
{
    for (size_t i = 0; i < NUM_BUILTIN_FORMATTERS; i++) {
        if (g_output_manager.formatter_count >= TEST_OUTPUT_MAX_FORMATTERS) {
            return -1;
        }
        memcpy(&g_output_manager.formatters[g_output_manager.formatter_count++],
               &g_builtin_formatters[i], sizeof(test_formatter_t));
    }
    return 0;
}

int test_output_register_formatter(test_formatter_t* formatter)
{
    if (!formatter || g_output_manager.formatter_count >= TEST_OUTPUT_MAX_FORMATTERS) {
        return -1;
    }
    memcpy(&g_output_manager.formatters[g_output_manager.formatter_count++],
           formatter, sizeof(test_formatter_t));
    return 0;
}

static test_formatter_t* find_formatter(test_output_format_t format)
{
    for (uint32_t i = 0; i < g_output_manager.formatter_count; i++) {
        if (g_output_manager.formatters[i].format == format) {
            return &g_output_manager.formatters[i];
        }
    }
    return NULL;
}

int test_output_add_console(test_output_format_t format)
{
    if (g_output_manager.target_count >= TEST_OUTPUT_MAX_TARGETS) {
        return -1;
    }
    
    if (format != TEST_FORMAT_PLAIN && format != TEST_FORMAT_COLOR) {
        format = TEST_FORMAT_COLOR;  /* 默认彩色 */
    }
    
    test_formatter_t* formatter = find_formatter(format);
    if (!formatter) return -1;
    
    test_output_target_entry_t* target = &g_output_manager.targets[g_output_manager.target_count++];
    target->type = TEST_TARGET_CONSOLE;
    target->format = format;
    target->formatter = formatter;
    
    /* 初始化formatter */
    bool use_color = (format == TEST_FORMAT_COLOR);
    if (formatter->init) {
        return formatter->init(formatter, &use_color);
    }
    
    return 0;
}

int test_output_add_file(const char* filepath, test_output_format_t format, bool append)
{
    if (!filepath || g_output_manager.target_count >= TEST_OUTPUT_MAX_TARGETS) {
        return -1;
    }
    
    test_formatter_t* formatter = find_formatter(format);
    if (!formatter) return -1;
    
    const char* mode = append ? "a" : "w";
    FILE* fp = fopen(filepath, mode);
    if (!fp) return -1;
    
    test_output_target_entry_t* target = &g_output_manager.targets[g_output_manager.target_count++];
    target->type = TEST_TARGET_FILE;
    target->format = format;
    target->formatter = formatter;
    target->target.file.fp = fp;
    target->target.file.path = strdup(filepath);
    target->target.file.append = append;
    
    /* 初始化formatter，传入文件指针 */
    if (formatter->init) {
        int ret = formatter->init(formatter, fp);
        if (ret != 0) {
            fclose(fp);
            g_output_manager.target_count--;
            return ret;
        }
    }
    
    return 0;
}

int test_output_add_custom(
    int (*write_callback)(const char* data, size_t len, void* user_data),
    void* user_data,
    test_output_format_t format)
{
    if (!write_callback || g_output_manager.target_count >= TEST_OUTPUT_MAX_TARGETS) {
        return -1;
    }
    
    test_formatter_t* formatter = find_formatter(format);
    if (!formatter) return -1;
    
    test_output_target_entry_t* target = &g_output_manager.targets[g_output_manager.target_count++];
    target->type = TEST_TARGET_CUSTOM;
    target->format = format;
    target->formatter = formatter;
    target->target.custom.write = write_callback;
    target->target.custom.user_data = user_data;
    
    if (formatter->init) {
        return formatter->init(formatter, NULL);
    }
    
    return 0;
}

void test_output_set_level(test_log_level_t level)
{
    g_output_manager.log_level = level;
}

int test_output_emit(test_output_event_t event, const test_event_data_t* data)
{
    if (!g_output_manager.initialized) {
        return -1;
    }
    
    int result = 0;
    
    for (uint32_t i = 0; i < g_output_manager.target_count; i++) {
        test_output_target_entry_t* target = &g_output_manager.targets[i];
        
        if (!target->formatter || !target->formatter->on_event) {
            continue;
        }
        
        /* 调用formatter处理事件，获取格式化后的字符串 */
        int len = target->formatter->on_event(target->formatter, event, data);
        if (len <= 0) continue;
        
        /* 根据目标类型输出 */
        switch (target->type) {
            case TEST_TARGET_CONSOLE:
                fwrite(g_output_buffer, 1, len, stdout);
                break;
                
            case TEST_TARGET_FILE: {
                FILE* fp = target->target.file.fp;
                if (fp) {
                    fwrite(g_output_buffer, 1, len, fp);
                }
                break;
            }
            
            case TEST_TARGET_CUSTOM:
                if (target->target.custom.write) {
                    target->target.custom.write(g_output_buffer, len, target->target.custom.user_data);
                }
                break;
                
            default:
                break;
        }
    }
    
    return result;
}

int test_output_flush(void)
{
    if (!g_output_manager.initialized) {
        return -1;
    }
    
    int result = 0;
    
    for (uint32_t i = 0; i < g_output_manager.target_count; i++) {
        test_output_target_entry_t* target = &g_output_manager.targets[i];
        
        if (target->formatter && target->formatter->flush) {
            int ret = target->formatter->flush(target->formatter);
            if (ret != 0) result = ret;
        }
        
        if (target->type == TEST_TARGET_FILE && target->target.file.fp) {
            fflush(target->target.file.fp);
        }
    }
    
    fflush(stdout);
    return result;
}

/* ==================== 便捷函数 ==================== */

int test_output_setup_default(const char* json_report_path)
{
    int ret = test_output_init();
    if (ret != 0) return ret;
    
    /* 添加控制台彩色输出 */
    ret = test_output_add_console(TEST_FORMAT_COLOR);
    if (ret != 0) return ret;
    
    /* 添加JSON文件输出 */
    if (json_report_path) {
        ret = test_output_add_file(json_report_path, TEST_FORMAT_JSON, false);
        if (ret != 0) return ret;
    }
    
    return 0;
}

int test_output_setup_console_only(bool use_color)
{
    int ret = test_output_init();
    if (ret != 0) return ret;
    
    return test_output_add_console(use_color ? TEST_FORMAT_COLOR : TEST_FORMAT_PLAIN);
}

int test_output_setup_ci(const char* json_report_path)
{
    int ret = test_output_init();
    if (ret != 0) return ret;
    
    /* CI环境：无颜色控制台 + JSON文件 */
    ret = test_output_add_console(TEST_FORMAT_PLAIN);
    if (ret != 0) return ret;
    
    if (json_report_path) {
        ret = test_output_add_file(json_report_path, TEST_FORMAT_JSON, false);
        if (ret != 0) return ret;
    }
    
    return 0;
}

/* ==================== 当前套件/测试信息管理 ==================== */

void test_output_set_suite_info(const test_suite_info_t* info)
{
    if (info) {
        memcpy(&g_output_manager.current_suite, info, sizeof(test_suite_info_t));
    }
}

void test_output_set_test_info(const test_case_info_t* info)
{
    if (info) {
        memcpy(&g_output_manager.current_test, info, sizeof(test_case_info_t));
    }
}

const test_suite_info_t* test_output_get_suite_info(void)
{
    return &g_output_manager.current_suite;
}

const test_case_info_t* test_output_get_test_info(void)
{
    return &g_output_manager.current_test;
}
