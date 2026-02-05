# em_test 使用手册

## 目录

1. [快速开始](#快速开始)
2. [断言宏参考](#断言宏参考)
3. [测试插件框架](#测试插件框架)
4. [xmake集成](#xmake集成)
5. [完整示例](#完整示例)

---

## 快速开始

### 1.1 最简单的测试

```c
// my_test.c
#include "test.h"

TEST_CASE(test_example) {
    TEST_ASSERT(2 + 2 == 4);
}

int main() {
    return run_tests();
}
```

### 1.2 编译运行

```bash
# 使用xmake（推荐）
xmake build my_test
xmake run my_test

# 或直接编译
gcc my_test.c test.c -o my_test
./my_test
```

### 1.3 输出示例

```
num_tests = 1
Running test: test_example

Tests finished: 1 total, 1 passed, 0 failed (assertions: 1 passed, 0 failed)
```

---

## 断言宏参考

### 2.1 通用断言

| 宏 | 说明 | 示例 |
|---|------|------|
| `TEST_ASSERT(cond)` | 通用布尔断言 | `TEST_ASSERT(x > 0)` |

### 2.2 整数比较（类型安全）

| 宏 | 类型 | 示例 |
|---|------|------|
| `TEST_EXPECT_EQ_U8(e, a)` | uint8_t | `TEST_EXPECT_EQ_U8(255, value)` |
| `TEST_EXPECT_EQ_I8(e, a)` | int8_t | `TEST_EXPECT_EQ_I8(-1, value)` |
| `TEST_EXPECT_EQ_U16(e, a)` | uint16_t | `TEST_EXPECT_EQ_U16(1000, value)` |
| `TEST_EXPECT_EQ_I16(e, a)` | int16_t | `TEST_EXPECT_EQ_I16(-1000, value)` |
| `TEST_EXPECT_EQ_U32(e, a)` | uint32_t | `TEST_EXPECT_EQ_U32(100000, value)` |
| `TEST_EXPECT_EQ_I32(e, a)` | int32_t | `TEST_EXPECT_EQ_I32(-100000, value)` |

**为什么需要类型特定宏？**  
避免C语言整数提升导致的意外行为：
```c
int8_t a = -1;      // 二进制 0xFF
uint8_t b = 255;    // 二进制 0xFF
// a == b 为 false（提升为int后 -1 != 255）
// 但 TEST_EXPECT_EQ_I8 和 TEST_EXPECT_EQ_U8 可以正确处理
```

### 2.3 浮点数比较

| 宏 | 说明 | 示例 |
|---|------|------|
| `TEST_EXPECT_EQ_F32(e, a)` | float（ε=1e-6） | `TEST_EXPECT_EQ_F32(3.14f, result)` |
| `TEST_EXPECT_EQ_F64(e, a)` | double（ε=1e-6） | `TEST_EXPECT_EQ_F64(3.14, result)` |
| `TEST_EXPECT_EQ_F32_E(e, a, eps)` | float（自定义精度） | `TEST_EXPECT_EQ_F32_E(3.14f, result, 0.01f)` |
| `TEST_EXPECT_EQ_F64_E(e, a, eps)` | double（自定义精度） | `TEST_EXPECT_EQ_F64_E(3.14, result, 0.01)` |

### 2.4 布尔、指针、字符串、内存

| 宏 | 说明 | 示例 |
|---|------|------|
| `TEST_EXPECT_EQ_BOOL(e, a)` | bool比较 | `TEST_EXPECT_EQ_BOOL(true, flag)` |
| `TEST_EXPECT_EQ_TRUE(a)` | 检查true | `TEST_EXPECT_EQ_TRUE(is_valid())` |
| `TEST_EXPECT_EQ_FALSE(a)` | 检查false | `TEST_EXPECT_EQ_FALSE(is_empty())` |
| `TEST_EXPECT_NULL(ptr)` | 空指针 | `TEST_EXPECT_NULL(p)` |
| `TEST_EXPECT_NOT_NULL(ptr)` | 非空指针 | `TEST_EXPECT_NOT_NULL(p)` |
| `TEST_EXPECT_EQ_STR(e, a)` | 字符串 | `TEST_EXPECT_EQ_STR("hello", str)` |
| `TEST_EXPECT_EQ_MEM(p1, p2, n)` | 内存比较 | `TEST_EXPECT_EQ_MEM(&a, &b, sizeof(a))` |

### 2.5 使用示例

```c
TEST_CASE(test_all_types) {
    // 整数
    TEST_EXPECT_EQ_I32(42, answer);
    
    // 浮点（带默认精度1e-6）
    TEST_EXPECT_EQ_F32(3.14159f, pi);
    
    // 浮点（自定义精度）
    TEST_EXPECT_EQ_F32_E(3.14f, pi, 0.01f);
    
    // 布尔
    TEST_EXPECT_EQ_TRUE(is_ready);
    TEST_EXPECT_EQ_FALSE(has_error);
    
    // 指针
    TEST_EXPECT_NOT_NULL(buffer);
    TEST_EXPECT_NULL(free_ptr);
    
    // 字符串
    TEST_EXPECT_EQ_STR("success", status);
    
    // 内存（无视符号比较二进制）
    int8_t a = -1;
    uint8_t b = 255;
    TEST_EXPECT_EQ_MEM(&a, &b, 1);  // 通过！都是0xFF
}
```

---

## 测试插件框架

### 3.1 概述

em_test 支持**插件化输出**，允许你自定义测试结果的输出方式。

- **内置终端插件**：默认启用，输出到控制台
- **自定义插件**：可注册回调函数实现自定义输出

### 3.2 生命周期钩子

框架在测试执行过程中会触发以下事件：

```
test_run()
    ├─ [开始] 测试套件
    │
    ├─ 遍历每个测试:
    │   ├─ [开始] 单个测试
    │   ├─ 执行测试...
    │   │   └─ [断言失败] 如果断言失败
    │   └─ [结束] 单个测试
    │
    └─ [结束] 测试套件
```

### 3.3 默认终端输出

框架内置终端输出插件，提供清晰的测试结果展示：

```
num_tests = 3
Running test: test_pass
Running test: test_fail
  ✗ test.c:25: TEST_EXPECT_EQ_I32(expected, actual) failed: expected 10 (0x0000000A), got 5 (0x00000005)
Running test: test_another_pass

Tests finished: 3 total, 2 passed, 1 failed (assertions: 3 passed, 1 failed)
```

### 3.4 自定义输出回调

你可以设置自定义输出函数来捕获测试输出：

```c
#include "test.h"

static FILE* log_file = NULL;

void my_output(const char* msg) {
    // 输出到终端
    printf("%s", msg);
    
    // 同时写入日志文件
    if (log_file) {
        fprintf(log_file, "%s", msg);
    }
}

int main() {
    log_file = fopen("test.log", "w");
    
    // 设置自定义输出
    test_set_output(my_output);
    
    int result = run_tests();
    
    fclose(log_file);
    return result;
}
```

### 3.5 文件输出插件

框架提供简单的文件记录器插件：

```c
#include "test.h"
#include "simple_file_recorder.h"

int main() {
    // 启用文件记录（同时保留终端输出）
    test_file_recorder_init("report.txt", 0);  // 0=覆盖, 1=追加
    
    int result = run_tests();
    
    test_file_recorder_close();
    return result;
}
```

---

## xmake集成

### 4.1 rule("em_test")

em_test 提供 xmake rule 简化测试项目的构建：

```lua
-- xmake.lua
add_rules("em_test")

target("my_test")
    set_kind("binary")
    add_rules("em_test", { 
        test_enable = true,        -- 启用测试
        use_default_main = true    -- 使用框架提供的main
    })
    add_files("my_test.c")
```

**配置选项：**

| 选项 | 类型 | 说明 |
|-----|------|------|
| `test_enable` | bool | 启用测试（定义 TEST_ENABLE 宏） |
| `use_default_main` | bool | 使用 test_main.c 提供的 main 函数 |

### 4.2 使用默认main

设置 `use_default_main = true`，你只需编写测试用例：

```c
// my_test.c - 无需写main函数
#include "test.h"

TEST_CASE(test_feature) {
    TEST_EXPECT_EQ_I32(42, compute());
}
```

### 4.3 自定义main

设置 `use_default_main = false`，自己控制测试流程：

```c
// my_test.c
#include "test.h"
#include "simple_file_recorder.h"

int main() {
    test_file_recorder_init("report.txt", 0);
    int result = run_tests();
    test_file_recorder_close();
    return result;
}
```

对应的 xmake.lua：
```lua
target("my_test")
    set_kind("binary")
    add_rules("em_test", { 
        test_enable = true,
        use_default_main = false    -- 使用自定义main
    })
    add_files("my_test.c")
    add_files("simple_file_recorder.c")  -- 添加插件
```

### 4.4 运行测试

```bash
# 构建
xmake build my_test

# 运行
xmake run my_test

# 或作为测试运行（xmake test）
xmake test my_test
```

---

## 完整示例

### 5.1 基础测试文件

```c
// calculator.c
#include <stdint.h>

int32_t add(int32_t a, int32_t b) {
    return a + b;
}

int32_t divide(int32_t a, int32_t b) {
    return b != 0 ? a / b : 0;
}

// ========== 测试代码 ==========
#include "test.h"

TEST_CASE(test_add_positive) {
    TEST_EXPECT_EQ_I32(5, add(2, 3));
    TEST_EXPECT_EQ_I32(0, add(0, 0));
}

TEST_CASE(test_add_negative) {
    TEST_EXPECT_EQ_I32(-5, add(-2, -3));
    TEST_EXPECT_EQ_I32(0, add(-1, 1));
}

TEST_CASE(test_divide) {
    TEST_EXPECT_EQ_I32(2, divide(10, 5));
    TEST_EXPECT_EQ_I32(0, divide(10, 0));  // 边界情况
}

TEST_CASE(test_type_safety) {
    // 演示整数类型提升问题的解决方案
    int8_t signed_val = -1;     // 0xFF
    uint8_t unsigned_val = 255; // 0xFF
    
    // 各自用正确的类型比较
    TEST_EXPECT_EQ_I8(-1, signed_val);
    TEST_EXPECT_EQ_U8(255, unsigned_val);
    
    // 二进制比较（无视符号）
    TEST_EXPECT_EQ_MEM(&signed_val, &unsigned_val, 1);
}
```

### 5.2 xmake配置

```lua
-- xmake.lua
add_rules("em_test")

target("test-calculator")
    set_kind("binary")
    add_rules("em_test", { 
        test_enable = true, 
        use_default_main = true 
    })
    add_files("calculator.c")
```

### 5.3 运行结果

```bash
$ xmake run test-calculator
num_tests = 4
Running test: test_add_positive
Running test: test_add_negative
Running test: test_divide
Running test: test_type_safety

Tests finished: 4 total, 4 passed, 0 failed (assertions: 8 passed, 0 failed)
```

### 5.4 带文件输出的完整版本

```c
// calculator_test.c（仅测试代码）
#include "test.h"
#include "simple_file_recorder.h"

TEST_CASE(test_add) {
    TEST_EXPECT_EQ_I32(5, add(2, 3));
}

// main.c
int main() {
    test_file_recorder_init("test_report.txt", 0);
    int result = run_tests();
    test_file_recorder_close();
    return result;
}
```

```lua
-- xmake.lua
target("test-calculator")
    set_kind("binary")
    add_rules("em_test", { 
        test_enable = true, 
        use_default_main = false  -- 使用自定义main
    })
    add_files("calculator.c")      -- 被测代码
    add_files("calculator_test.c") -- 测试代码
    add_files("main.c")            -- 自定义main
    add_files("simple_file_recorder.c")
```

---

## 附录：宏速查表

### 断言宏（全部带 TEST_ 前缀）

| 类别 | 宏 | 类型 |
|-----|---|------|
| 通用 | `TEST_ASSERT(cond)` | bool |
| 8位 | `TEST_EXPECT_EQ_U8/I8` | uint8_t/int8_t |
| 16位 | `TEST_EXPECT_EQ_U16/I16` | uint16_t/int16_t |
| 32位 | `TEST_EXPECT_EQ_U32/I32` | uint32_t/int32_t |
| 浮点 | `TEST_EXPECT_EQ_F32/F64` | float/double |
| 浮点自定义 | `TEST_EXPECT_EQ_F32_E/F64_E` | float/double + epsilon |
| 布尔 | `TEST_EXPECT_EQ_BOOL/TRUE/FALSE` | bool |
| 指针 | `TEST_EXPECT_EQ_NULL/NOT_NULL` | pointer |
| 字符串 | `TEST_EXPECT_EQ_STR` | const char* |
| 内存 | `TEST_EXPECT_EQ_MEM` | void* + size |

### API函数

```c
// 运行所有测试
int run_tests(void);

// 设置自定义输出回调
typedef void (*test_output_fn)(const char* msg);
void test_set_output(test_output_fn fn);

// 文件记录器插件
int test_file_recorder_init(const char* filepath, int append);
void test_file_recorder_close(void);
```

---

**文档版本：** v1.0  
**最后更新：** 2026-02-05  
**状态：** 用户使用手册
