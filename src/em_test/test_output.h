/**
 * @file test_output.h
 * @brief 结构化测试输出系统
 * 
 * 提供多目标、多格式的测试输出能力
 * 支持：控制台（彩色/纯文本）、文件（JSON/纯文本）
 */

#ifndef LIBCA_EM_TEST_OUTPUT_H
#define LIBCA_EM_TEST_OUTPUT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置常量 ==================== */

#define TEST_OUTPUT_MAX_TARGETS 4       /* 最大输出目标数 */
#define TEST_OUTPUT_MAX_FORMATTERS 4    /* 最大格式化器数 */
#define TEST_OUTPUT_BUF_SIZE 1024       /* 输出缓冲区大小 */

/* ==================== 枚举定义 ==================== */

/**
 * @brief 输出目标类型
 */
typedef enum {
    TEST_TARGET_NONE = 0,
    TEST_TARGET_CONSOLE,        /* 标准输出 */
    TEST_TARGET_FILE,           /* 文件 */
    TEST_TARGET_CUSTOM          /* 自定义回调 */
} test_output_target_t;

/**
 * @brief 输出格式类型
 */
typedef enum {
    TEST_FORMAT_NONE = 0,
    TEST_FORMAT_PLAIN,          /* 纯文本 */
    TEST_FORMAT_COLOR,          /* 彩色文本（控制台） */
    TEST_FORMAT_JSON,           /* JSON格式 */
    TEST_FORMAT_JUNIT,          /* JUnit XML格式 */
    TEST_FORMAT_CUSTOM          /* 自定义格式 */
} test_output_format_t;

/**
 * @brief 输出事件类型
 */
typedef enum {
    TEST_EVENT_SUITE_START = 0, /* 测试套件开始 */
    TEST_EVENT_SUITE_END,       /* 测试套件结束 */
    TEST_EVENT_TEST_START,      /* 单个测试开始 */
    TEST_EVENT_TEST_END,        /* 单个测试结束（包含成功/失败状态） */
    TEST_EVENT_ASSERT_FAIL,     /* 断言失败（详细错误） */
    TEST_EVENT_SUMMARY          /* 测试总结 */
} test_output_event_t;

/**
 * @brief 日志级别
 */
typedef enum {
    TEST_LOG_ERROR = 0,
    TEST_LOG_WARN,
    TEST_LOG_INFO,
    TEST_LOG_DEBUG
} test_log_level_t;

/* ==================== 数据结构 ==================== */

/**
 * @brief 测试套件信息
 */
typedef struct {
    const char* name;           /* 套件名称 */
    uint32_t total_tests;       /* 总测试数 */
    uint32_t passed_tests;      /* 通过数 */
    uint32_t failed_tests;      /* 失败数 */
    uint64_t start_time_ms;     /* 开始时间 */
    uint64_t end_time_ms;       /* 结束时间 */
} test_suite_info_t;

/**
 * @brief 单个测试信息
 */
typedef struct {
    const char* name;           /* 测试名称 */
    const char* file;           /* 源文件路径 */
    uint32_t line;              /* 行号 */
    uint64_t start_time_ms;     /* 开始时间 */
    uint64_t end_time_ms;       /* 结束时间 */
    uint32_t assertion_count;   /* 断言计数 */
    bool passed;                /* 是否通过 */
    uint32_t fail_count;        /* 失败次数 */
} test_case_info_t;

/**
 * @brief 断言失败详情
 */
typedef struct {
    const char* file;           /* 失败发生文件 */
    uint32_t line;              /* 失败发生行号 */
    const char* expression;     /* 失败的表达式 */
    const char* message;        /* 错误消息（可空） */
    const char* expected;       /* 期望值字符串 */
    const char* actual;         /* 实际值字符串 */
    test_output_format_t format;/* 值的数据格式 */
} test_assert_fail_info_t;

/**
 * @brief 事件数据联合体
 */
typedef union {
    test_suite_info_t suite;            /* 套件事件 */
    test_case_info_t test;              /* 测试事件 */
    test_assert_fail_info_t assert_fail;/* 断言失败事件 */
} test_event_data_t;

/**
 * @brief 格式化器接口
 */
typedef struct test_formatter {
    const char* name;                                   /* 格式化器名称 */
    test_output_format_t format;                        /* 格式类型 */
    
    /* 初始化/清理 */
    int (*init)(struct test_formatter* self, void* config);
    void (*cleanup)(struct test_formatter* self);
    
    /* 事件处理 */
    int (*on_event)(struct test_formatter* self, 
                    test_output_event_t event,
                    const test_event_data_t* data);
    
    /* 刷新输出 */
    int (*flush)(struct test_formatter* self);
    
    /* 私有数据 */
    void* priv_data;
} test_formatter_t;

/**
 * @brief 输出目标
 */
typedef struct {
    test_output_target_t type;          /* 目标类型 */
    test_output_format_t format;        /* 格式类型 */
    test_formatter_t* formatter;        /* 关联的格式化器 */
    
    union {
        struct {
            FILE* fp;                   /* 文件指针 */
            char* path;                 /* 文件路径 */
            bool append;                /* 是否追加模式 */
        } file;
        
        struct {
            int (*write)(const char* data, size_t len, void* user_data);
            void* user_data;
        } custom;
    } target;
} test_output_target_entry_t;

/**
 * @brief 输出管理器
 */
typedef struct {
    test_output_target_entry_t targets[TEST_OUTPUT_MAX_TARGETS];
    uint32_t target_count;
    
    test_formatter_t formatters[TEST_OUTPUT_MAX_FORMATTERS];
    uint32_t formatter_count;
    
    test_suite_info_t current_suite;
    test_case_info_t current_test;
    
    bool initialized;
    test_log_level_t log_level;
} test_output_manager_t;

/* ==================== API接口 ==================== */

/**
 * @brief 初始化输出系统
 * @return 0成功，非0失败
 */
int test_output_init(void);

/**
 * @brief 清理输出系统
 */
void test_output_cleanup(void);

/**
 * @brief 添加控制台输出目标
 * @param format 输出格式（COLOR或PLAIN）
 * @return 0成功，非0失败
 */
int test_output_add_console(test_output_format_t format);

/**
 * @brief 添加文件输出目标
 * @param filepath 文件路径
 * @param format 输出格式（JSON/PLAIN/JUNIT）
 * @param append 是否追加模式
 * @return 0成功，非0失败
 */
int test_output_add_file(const char* filepath, test_output_format_t format, bool append);

/**
 * @brief 添加自定义输出目标
 * @param write_callback 写入回调函数
 * @param user_data 用户数据
 * @param format 格式类型
 * @return 0成功，非0失败
 */
int test_output_add_custom(
    int (*write_callback)(const char* data, size_t len, void* user_data),
    void* user_data,
    test_output_format_t format
);

/**
 * @brief 设置日志级别
 */
void test_output_set_level(test_log_level_t level);

/**
 * @brief 发送事件到所有输出目标
 */
int test_output_emit(test_output_event_t event, const test_event_data_t* data);

/**
 * @brief 刷新所有输出
 */
int test_output_flush(void);

/* ==================== 便捷函数 ==================== */

/**
 * @brief 快速配置：控制台彩色 + JSON文件
 */
int test_output_setup_default(const char* json_report_path);

/**
 * @brief 快速配置：仅控制台
 */
int test_output_setup_console_only(bool use_color);

/**
 * @brief 快速配置：CI环境（无颜色，JSON输出）
 */
int test_output_setup_ci(const char* json_report_path);

/* ==================== 格式化器注册 ==================== */

/**
 * @brief 注册内置格式化器
 */
int test_output_register_builtin_formatters(void);

/**
 * @brief 注册自定义格式化器
 */
int test_output_register_formatter(test_formatter_t* formatter);

#ifdef __cplusplus
}
#endif

#endif /* LIBCA_EM_TEST_OUTPUT_H */
