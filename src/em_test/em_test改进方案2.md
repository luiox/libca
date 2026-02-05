# em_test 改进方案 v2.2

## 1. 现状分析

### 1.1 当前代码结构

```
libca/src/em_test/
├── test.h                          # 366行 - 核心头文件
├── test.c                          # 373行 - 框架实现
├── test_output.h                   # 275行 - 结构化输出头文件
├── test_output.c                   # 808行 - 结构化输出实现
├── test_main.c                     # 16行 - 默认main函数
├── test_enhanced_assertions.c      # 148行 - 增强断言示例
├── test_structured_output_demo.c   # 90行 - 结构化输出演示
├── xmake.lua                       # 66行 - 构建配置
├── README_STRUCTURED_OUTPUT.md     # 313行 - 结构化输出文档
└── README_ENHANCED_ASSERTIONS.md   # 117行 - 增强断言文档
```

**总代码量：约 2,600+ 行**

### 1.2 核心问题

| 问题类别 | 具体问题 | 影响 |
|---------|---------|------|
| **过度设计** | 结构化输出系统包含格式化器、目标管理器、多格式支持 | 代码量过大，学习成本高 |
| **宏定义冗余** | U8/I8/U16 等类型特定断言与普通断言重复逻辑 | 代码膨胀，可读性差 |
| **依赖复杂** | 分散在多个文件中，依赖关系混乱 | 难以维护和理解 |
| **API复杂** | 需要显式配置输出目标、格式化器、初始化/清理 | 使用门槛高 |
| **无自测试** | 框架本身没有内建测试验证其正确性 | 无法确保框架自身可靠性 |

### 1.3 整数类型提升问题（关键）

**C语言整数提升规则：**
当比较 `int8_t` 和 `uint8_t` 时，两者都会被提升为 `int`：
- `int8_t` 的 -1 提升为 int 仍是 -1
- `uint8_t` 的 255 提升为 int 是 255
- 结果：`-1 == 255` 为 `false`

**虽然它们的二进制表示完全相同（都是 0xFF）！**

```c
int8_t a = -1;      // 二进制: 0xFF
uint8_t b = 255;    // 二进制: 0xFF

// 错误的比较方式
TEST_ASSERT_EQUAL_INT(a, b);  // 失败！比较的是 -1 != 255

// 但在嵌入式/协议解析中，它们应该被认为是相等的
```

---

## 2. 设计目标

### 2.1 核心原则

1. **简洁实用**
   - test.h + test.c 双文件结构，避免单头文件的棘手问题
   - 总代码量控制在 **500行以内**

2. **类型安全**
   - 解决整数类型提升问题
   - 显式类型比较，无歧义

3. **可控依赖**
   - 标准库 + em_base/ 下的头文件（datatype.h, macro_util.h, compiler_compat.h）
   - 不依赖源文件，仅依赖头文件

4. **保留 TEST_CASE 风格**
   - 测试写在文件底部
   - 利用链接器 section 自动收集

5. **插件化输出**
   - 内置终端输出插件（默认）
   - 可选的简单文件输出插件

### 2.2 设计约束

| 约束项 | 要求 |
|-------|------|
| 文件结构 | test.h + test.c（双文件） |
| 代码行数 | < 500行（不含注释） |
| 依赖 | 标准库 + em_base/*.h（仅头文件） |
| C标准 | C99兼容 |
| 平台 | 跨平台（Windows/Linux/嵌入式） |

---

## 3. 新文件结构

```
libca/src/em_test/
├── test.h                          # 测试框架头文件（约150行）
├── test.c                          # 测试框架实现（约150行）
├── simple_file_recorder.h          # 文件输出插件头文件（约30行）
├── simple_file_recorder.c          # 文件输出插件实现（约50行）
├── README.md                       # 使用文档
│
└── examples/                       # 使用示例
    ├── test_basic.c
    ├── test_type_safety.c
    └── test_with_file_output.c
```

**代码量减少：2,600行 → 约400行（85%缩减）**

---

## 4. 新 API 设计

### 4.1 核心宏定义（13个）

| 类别 | 宏名称 | 参数 | 说明 |
|-----|--------|------|------|
| **测试定义** | `TEST_CASE(name)` | name: 测试名 | 定义测试用例块 |
| **通用断言** | `ASSERT(cond)` | cond: 布尔表达式 | 通用条件断言 |
| **8位整数** | `EXPECT_EQ_U8(e, a)` | e: 期望值, a: 实际值 | uint8_t 相等比较 |
| | `EXPECT_EQ_I8(e, a)` | e: 期望值, a: 实际值 | int8_t 相等比较 |
| **16位整数** | `EXPECT_EQ_U16(e, a)` | e: 期望值, a: 实际值 | uint16_t 相等比较 |
| | `EXPECT_EQ_I16(e, a)` | e: 期望值, a: 实际值 | int16_t 相等比较 |
| **32位整数** | `EXPECT_EQ_U32(e, a)` | e: 期望值, a: 实际值 | uint32_t 相等比较 |
| | `EXPECT_EQ_I32(e, a)` | e: 期望值, a: 实际值 | int32_t 相等比较 |
| **浮点数** | `EXPECT_EQ_F32(e, a)` | e: 期望值, a: 实际值 | float 相等（ε=1e-6） |
| | `EXPECT_EQ_F64(e, a)` | e: 期望值, a: 实际值 | double 相等（ε=1e-6） |
| | `EXPECT_EQ_F32_E(e, a, eps)` | e: 期望值, a: 实际值, eps: 精度 | float 相等（自定义精度） |
| | `EXPECT_EQ_F64_E(e, a, eps)` | e: 期望值, a: 实际值, eps: 精度 | double 相等（自定义精度） |
| **布尔** | `EXPECT_EQ_BOOL(e, a)` | e: 期望值, a: 实际值 | bool 相等比较 |
| **指针** | `EXPECT_NULL(ptr)` | ptr: 指针 | 检查空指针 |
| | `EXPECT_NOT_NULL(ptr)` | ptr: 指针 | 检查非空指针 |
| **字符串** | `EXPECT_EQ_STR(s1, s2)` | s1: 字符串1, s2: 字符串2 | 字符串相等比较 |
| **内存** | `EXPECT_EQ_MEM(p1, p2, n)` | p1: 指针1, p2: 指针2, n: 字节数 | 二进制内存比较 |

### 4.2 测试运行 API

```c
// test.h

// 初始化测试框架（可选，首次调用时会自动初始化）
void em_test_init(void);

// 运行所有注册的测试
int em_test_run(void);

// 注册输出回调（可选，用于自定义输出）
typedef void (*em_test_output_fn)(const char* msg);
void em_test_set_output(em_test_output_fn fn);
```

### 4.3 文件输出插件 API

```c
// simple_file_recorder.h

// 初始化文件输出插件
// filepath: 输出文件路径
// append: 是否追加模式（0=覆盖, 1=追加）
int em_test_file_recorder_init(const char* filepath, int append);

// 关闭文件输出
void em_test_file_recorder_close(void);
```

---

## 5. 使用示例

### 5.1 基础用法

```c
// calculator.c
#include <stdint.h>

int32_t add(int32_t a, int32_t b) {
    return a + b;
}

// 测试代码写在文件底部
#include "test.h"

TEST_CASE(test_add_basic) {
    EXPECT_EQ_I32(5, add(2, 3));
    EXPECT_EQ_I32(0, add(-1, 1));
}

TEST_CASE(test_add_overflow) {
    EXPECT_EQ_I32(INT32_MAX, add(INT32_MAX, 0));
}

int main() {
    return em_test_run();
}
```

### 5.2 类型安全示例

```c
TEST_CASE(test_type_safety) {
    int8_t signed_val = -1;     // 0xFF
    uint8_t unsigned_val = 255; // 0xFF
    
    // 显式类型比较，避免整数提升问题
    EXPECT_EQ_I8(-1, signed_val);
    EXPECT_EQ_U8(255, unsigned_val);
    
    // 二进制比较（无视符号）
    EXPECT_EQ_MEM(&signed_val, &unsigned_val, 1);
}
```

### 5.3 使用文件输出插件

```c
#include "test.h"
#include "simple_file_recorder.h"

TEST_CASE(test_example) {
    EXPECT_EQ_I32(42, 21 + 21);
}

int main() {
    // 启用文件输出（同时保留终端输出）
    em_test_file_recorder_init("test_report.txt", 0);
    
    int result = em_test_run();
    
    em_test_file_recorder_close();
    return result;
}
```

---

## 6. 迁移方案

### 6.1 旧宏到新宏的映射

**迁移前（旧版本）：**
```c
#include "test.h"

TEST_CASE(test_old) {
    TEST_ASSERT_EQUAL_INT(5, add(2, 3));
    TEST_ASSERT_EQUAL_U8(255, value);
    TEST_ASSERT_EQUAL_STRING("hello", str);
    TEST_ASSERT_NULL(ptr);
}
```

**迁移后（新版本）：**
```c
#include "test.h"

TEST_CASE(test_new) {
    EXPECT_EQ_I32(5, add(2, 3));        // int32_t 比较
    EXPECT_EQ_U8(255, value);           // uint8_t 比较
    EXPECT_EQ_STR("hello", str);        // 字符串比较
    EXPECT_NULL(ptr);                   // 空指针检查
}
```

### 6.2 完整迁移对照表

| 旧宏（test.h 旧版） | 新宏（test.h v2.2） | 说明 |
|-------------------|-------------------|------|
| `TEST_ASSERT(cond)` | `ASSERT(cond)` | 通用断言 |
| `TEST_ASSERT_EQUAL_INT(e, a)` | `EXPECT_EQ_I32(e, a)` | int32_t 比较 |
| `TEST_ASSERT_EQUAL_UINT(e, a)` | `EXPECT_EQ_U32(e, a)` | uint32_t 比较 |
| `TEST_ASSERT_EQUAL_U8(e, a)` | `EXPECT_EQ_U8(e, a)` | uint8_t 比较 |
| `TEST_ASSERT_EQUAL_I8(e, a)` | `EXPECT_EQ_I8(e, a)` | int8_t 比较 |
| `TEST_ASSERT_EQUAL_U16(e, a)` | `EXPECT_EQ_U16(e, a)` | uint16_t 比较 |
| `TEST_ASSERT_EQUAL_I16(e, a)` | `EXPECT_EQ_I16(e, a)` | int16_t 比较 |
| `TEST_ASSERT_EQUAL_U32(e, a)` | `EXPECT_EQ_U32(e, a)` | uint32_t 比较 |
| `TEST_ASSERT_EQUAL_I32(e, a)` | `EXPECT_EQ_I32(e, a)` | int32_t 比较 |
| `TEST_ASSERT_EQUAL_FLOAT(e, a)` | `EXPECT_EQ_F32(e, a)` | float 比较 |
| `TEST_ASSERT_EQUAL_DOUBLE(e, a)` | `EXPECT_EQ_F64(e, a)` | double 比较 |
| `TEST_ASSERT_EQUAL_STRING(e, a)` | `EXPECT_EQ_STR(e, a)` | 字符串比较 |
| `TEST_ASSERT_NULL(p)` | `EXPECT_NULL(p)` | 空指针检查 |
| `TEST_ASSERT_NOT_NULL(p)` | `EXPECT_NOT_NULL(p)` | 非空指针检查 |
| `TEST_ASSERT_TRUE(c)` | `ASSERT(c)` | 真值检查 |
| `TEST_ASSERT_FALSE(c)` | `ASSERT(!(c))` | 假值检查 |
| `TEST_ASSERT_EQUAL_MEMORY(e, a, s)` | `EXPECT_EQ_MEM(e, a, s)` | 内存比较 |

### 6.3 自动化迁移脚本（可选）

```bash
#!/bin/bash
# migrate_test.sh - 自动替换旧宏到新宏

sed -i 's/TEST_ASSERT_EQUAL_INT(/EXPECT_EQ_I32(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_UINT(/EXPECT_EQ_U32(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_U8(/EXPECT_EQ_U8(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_I8(/EXPECT_EQ_I8(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_U16(/EXPECT_EQ_U16(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_I16(/EXPECT_EQ_I16(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_U32(/EXPECT_EQ_U32(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_I32(/EXPECT_EQ_I32(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_FLOAT(/EXPECT_EQ_F32(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_DOUBLE(/EXPECT_EQ_F64(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_STRING(/EXPECT_EQ_STR(/g' "$1"
sed -i 's/TEST_ASSERT_NULL(/EXPECT_NULL(/g' "$1"
sed -i 's/TEST_ASSERT_NOT_NULL(/EXPECT_NOT_NULL(/g' "$1"
sed -i 's/TEST_ASSERT_TRUE(/ASSERT(/g' "$1"
sed -i 's/TEST_ASSERT_FALSE(/ASSERT(!(/g' "$1"
sed -i 's/TEST_ASSERT_EQUAL_MEMORY(/EXPECT_EQ_MEM(/g' "$1"
sed -i 's/TEST_ASSERT(/ASSERT(/g' "$1"
```

### 6.4 手动迁移检查清单

迁移每个测试文件时，请检查：

- [ ] 头文件从旧路径改为 `#include "em_test/test.h"`（或新路径）
- [ ] `TEST_ASSERT_EQUAL_INT` → `EXPECT_EQ_I32`（注意：int 可能是 32位或64位，根据需要选择）
- [ ] `TEST_ASSERT_EQUAL_UINT` → `EXPECT_EQ_U32`
- [ ] `TEST_ASSERT_EQUAL_FLOAT` → `EXPECT_EQ_F32`
- [ ] `TEST_ASSERT_EQUAL_DOUBLE` → `EXPECT_EQ_F64`
- [ ] `TEST_ASSERT_EQUAL_STRING` → `EXPECT_EQ_STR`
- [ ] `TEST_ASSERT_NULL` → `EXPECT_NULL`
- [ ] `TEST_ASSERT_NOT_NULL` → `EXPECT_NOT_NULL`
- [ ] `TEST_ASSERT_TRUE`/`TEST_ASSERT_FALSE` → `ASSERT`
- [ ] 删除旧的 `test_output.h` 相关代码（如果有）
- [ ] 检查浮点数比较是否需要自定义精度（使用 `EXPECT_EQ_F32_E` 或 `EXPECT_EQ_F64_E`）

---

## 7. 实现要点

### 7.1 test.h 关键设计

```c
#ifndef EM_TEST_H
#define EM_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// 依赖 em_base 头文件
#include "../em_base/datatype.h"
#include "../em_base/macro_util.h"
#include "../em_base/compiler_compat.h"

// 测试用例结构
typedef struct {
    const char* name;
    void (*func)(void);
    const char* file;
    int line;
} em_test_case_t;

// TEST_CASE 宏（使用 em_base 的宏工具）
#define TEST_CASE(name) \
    static void em_test_func_##name(void); \
    static const em_test_case_t em_test_data_##name CA_SECTION("em_test") = { \
        #name, em_test_func_##name, __FILE__, __LINE__ \
    }; \
    static void em_test_func_##name(void)

// 断言宏示例
#define EXPECT_EQ_I32(expected, actual) \
    em_test_assert_eq_i32(__FILE__, __LINE__, #expected, #actual, \
                          (int32_t)(expected), (int32_t)(actual))

// 外部接口
void em_test_init(void);
int em_test_run(void);

#endif
```

### 7.2 test.c 关键设计

```c
#include "test.h"

static int em_test_passed = 0;
static int em_test_failed = 0;

// 默认输出到终端
static void em_default_output(const char* msg) {
    printf("%s", msg);
}

static em_test_output_fn g_output_fn = em_default_output;

void em_test_set_output(em_test_output_fn fn) {
    g_output_fn = fn ? fn : em_default_output;
}

// 断言实现示例
void em_test_assert_eq_i32(const char* file, int line, 
                           const char* expr_e, const char* expr_a,
                           int32_t expected, int32_t actual) {
    if (expected != actual) {
        char buf[256];
        snprintf(buf, sizeof(buf), 
                 "  ✗ %s:%d: EXPECT_EQ_I32(%s, %s) failed: expected %d, got %d\n",
                 file, line, expr_e, expr_a, expected, actual);
        g_output_fn(buf);
        em_test_failed++;
    } else {
        em_test_passed++;
    }
}

// 自动收集并运行测试
int em_test_run(void) {
    // 使用 CA_SECTION 遍历所有测试用例
    // ...
}
```

---

## 8. 下一步行动

**等待指示后，可执行以下任务：**

1. **创建 test.h** - 测试框架头文件
2. **创建 test.c** - 测试框架实现
3. **创建 simple_file_recorder.h/c** - 文件输出插件
4. **创建迁移脚本** - 辅助旧代码迁移
5. **创建示例代码** - 演示各种用法

---

**文档版本：** v2.2  
**最后更新：** 2026-02-05  
**主要变更：** 
- 改为 test.h + test.c 双文件结构
- 依赖范围：标准库 + em_base/*.h
- 插件精简为：内置终端输出 + 简单文件输出插件
- 新增完整的迁移方案章节
**状态：** 待审批
