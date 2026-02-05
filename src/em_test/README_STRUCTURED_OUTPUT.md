# em_test 结构化输出系统

## 概述

结构化输出系统为 em_test 测试框架提供了多目标、多格式的测试报告能力。支持同时输出到控制台（彩色/纯文本）和文件（JSON/纯文本），便于在 CI/CD 环境中集成和生成测试报告。

## 特性

- ✅ **多目标输出**：同时输出到多个目标（控制台 + 多个文件）
- ✅ **多格式支持**：彩色控制台、纯文本、JSON
- ✅ **向后兼容**：不启用时保持原有行为
- ✅ **嵌入式友好**：可配置，支持无文件系统环境
- ✅ **跨平台**：支持 Windows、Linux、macOS 和嵌入式系统

## 快速开始

### 1. 基本使用（默认配置）

```c
#include "test.h"

int main(int argc, char** argv)
{
    // 初始化并设置默认配置：控制台彩色 + JSON报告
    test_output_setup_default("test_report.json");
    
    // 启用结构化输出
    test_set_structured_output(true);
    
    // 运行测试
    return run_tests();
}
```

### 2. 自定义配置

```c
#include "test.h"

int main(int argc, char** argv)
{
    // 初始化输出系统
    test_output_init();
    
    // 添加控制台输出（彩色）
    test_output_add_console(TEST_FORMAT_COLOR);
    
    // 添加 JSON 文件输出
    test_output_add_file("report.json", TEST_FORMAT_JSON, false);
    
    // 添加纯文本文件输出（追加模式）
    test_output_add_file("log.txt", TEST_FORMAT_PLAIN, true);
    
    // 启用结构化输出
    test_set_structured_output(true);
    
    // 运行测试
    int result = run_tests();
    
    // 清理资源
    test_output_cleanup();
    
    return result;
}
```

### 3. 预定义配置

```c
// 仅控制台（开发环境）
test_output_setup_console_only(true);  // true = 彩色, false = 纯文本

// CI 环境（无颜色，JSON 输出）
test_output_setup_ci("test-results.json");

// 默认配置（控制台 + JSON）
test_output_setup_default("report.json");
```

## API 参考

### 初始化与配置

```c
// 初始化输出系统
int test_output_init(void);

// 清理输出系统
void test_output_cleanup(void);

// 添加控制台输出
int test_output_add_console(test_output_format_t format);

// 添加文件输出
int test_output_add_file(const char* filepath, test_output_format_t format, bool append);

// 添加自定义输出（通过回调函数）
int test_output_add_custom(
    int (*write_callback)(const char* data, size_t len, void* user_data),
    void* user_data,
    test_output_format_t format
);
```

### 输出格式枚举

```c
typedef enum {
    TEST_FORMAT_NONE = 0,
    TEST_FORMAT_PLAIN,    // 纯文本
    TEST_FORMAT_COLOR,    // 彩色文本（控制台）
    TEST_FORMAT_JSON,     // JSON格式
    TEST_FORMAT_JUNIT,    // JUnit XML格式（预留）
    TEST_FORMAT_CUSTOM    // 自定义格式
} test_output_format_t;
```

### 测试框架集成

```c
// 启用/禁用结构化输出（默认禁用，向后兼容）
void test_set_structured_output(bool enable);
bool test_is_structured_output_enabled(void);
```

### 便捷配置函数

```c
// 默认配置：控制台彩色 + JSON文件
int test_output_setup_default(const char* json_report_path);

// 仅控制台
int test_output_setup_console_only(bool use_color);

// CI环境配置：无颜色控制台 + JSON文件
int test_output_setup_ci(const char* json_report_path);
```

## 输出示例

### 彩色控制台输出

```
═══════════════════════════════════════════════════════════════════════════════
  Test Suite: my_tests
═══════════════════════════════════════════════════════════════════════════════

  ● test_pass_example ... ✓ PASS (0 ms)
  ● test_fail_example ... ✗ FAIL (0 ms)
    ├─ Assert Fail: test.c:42
    ├─ Expression: TEST_ASSERT_EQUAL_INT(5, result)
    ├─ Expected:   5
    └─ Actual:     4
  ● test_another_pass ... ✓ PASS (0 ms)

═══════════════════════════════════════════════════════════════════════════════
  Results: 3 total, 2 passed, 1 failed
  Duration: 1 ms
═══════════════════════════════════════════════════════════════════════════════
```

### JSON 输出示例

```json
{
  "version": "1.0",
  "generator": "em_test",
  "timestamp": "2026-02-05T12:00:00Z",
  "suite": {
    "name": "my_tests",
    "tests": [
      {
        "name": "test_pass_example",
        "file": "test.c",
        "line": 10,
        "status": "passed",
        "duration_ms": 0,
        "assertions": 2
      },
      {
        "name": "test_fail_example",
        "file": "test.c",
        "line": 16,
        "status": "failed",
        "duration_ms": 0,
        "assertions": 1,
        "failures": [
          {
            "file": "test.c",
            "line": 18,
            "expression": "TEST_ASSERT_EQUAL_INT(5, result)",
            "message": "",
            "expected": "5",
            "actual": "4"
          }
        ]
      }
    ],
    "summary": {
      "total": 3,
      "passed": 2,
      "failed": 1,
      "duration_ms": 1
    }
  }
}
```

## 高级用法

### 自定义输出目标

```c
// 自定义写入函数（例如：发送到网络、数据库等）
int my_custom_writer(const char* data, size_t len, void* user_data)
{
    // user_data 可以是任意上下文
    MyContext* ctx = (MyContext*)user_data;
    
    // 将数据发送到远程服务器
    send_to_server(ctx->socket, data, len);
    
    return 0;  // 成功
}

int main()
{
    test_output_init();
    
    MyContext ctx = {...};
    test_output_add_custom(my_custom_writer, &ctx, TEST_FORMAT_JSON);
    
    test_set_structured_output(true);
    run_tests();
    
    test_output_cleanup();
    return 0;
}
```

### 环境变量控制

系统支持以下环境变量：

- `NO_COLOR`: 如果设置，禁用彩色输出
- `FORCE_COLOR`: 如果设置，强制启用彩色输出（即使在非 TTY）

```bash
# 禁用颜色
NO_COLOR=1 ./test

# 强制启用颜色（例如在 CI 中）
FORCE_COLOR=1 ./test
```

## 向后兼容

如果不调用任何结构化输出相关的 API，框架保持原有的行为：

```c
int main()
{
    // 不启用结构化输出，保持原有行为
    return run_tests();  // 使用 printf 输出
}
```

## 嵌入式环境注意事项

在资源受限的嵌入式环境中：

1. **无文件系统**：只使用 `test_output_add_console()`
2. **减少内存占用**：不启用结构化输出，保持原有 printf 方式
3. **自定义输出**：使用 `test_output_add_custom()` 将输出发送到串口

```c
// 嵌入式示例：输出到串口
int serial_write(const char* data, size_t len, void* user_data)
{
    for (size_t i = 0; i < len; i++) {
        uart_putc(data[i]);
    }
    return 0;
}

void run_tests_embedded(void)
{
    test_output_init();
    test_output_add_custom(serial_write, NULL, TEST_FORMAT_PLAIN);
    test_set_structured_output(true);
    run_tests();
    test_output_cleanup();
}
```

## 构建

```bash
# 构建结构化输出演示
xmake build test-em_structured_output

# 运行演示
xmake run test-em_structured_output

# 查看生成的报告
cat test_report.json
cat test_report.txt
```

## 许可证

与 libca 项目使用相同的许可证。
