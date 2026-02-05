# em_test 设计文档

本文档介绍 em_test 测试框架的内部实现机制，包括测试用例的自动收集、插件架构等核心原理。

---

## 1. 测试用例自动收集机制

### 1.1 核心思想

利用编译器的 **section 属性** 将测试用例信息放入特定的数据段，然后在运行时通过遍历该数据段自动收集所有测试用例。

**优势：**
- 无需手动注册测试
- 添加新测试只需写 `TEST_CASE` 宏
- 零运行时开销的注册

### 1.2 实现细节

#### 数据结构

```c
typedef struct {
    const char* name;       // 测试名称
    void (*func)(void);     // 测试函数指针
} test_t;
```

#### TEST_CASE 宏展开

```c
#define TEST_CASE(name)                                                                        \
    static void          CA_SAFE_NAME(test_func_)(void);                                       \
    static const test_t  CA_SAFE_NAME(test_data_) = {CA_MAKE_STRING(name), CA_SAFE_NAME(test_func_)}; \
    static TEST_CASE_ALLOC const test_t* CA_SAFE_NAME(test_ptr_) = &CA_SAFE_NAME(test_data_); \
    static void          CA_SAFE_NAME(test_func_)(void)
```

展开后示例：

```c
TEST_CASE(my_test) {
    TEST_ASSERT(1 == 1);
}
```

展开为：

```c
static void test_func_123(void);
static const test_t test_data_123 = {"my_test", test_func_123};
static __attribute__((used, section("test_array"))) const test_t* test_ptr_123 = &test_data_123;
static void test_func_123(void) {
    TEST_ASSERT(1 == 1);
}
```

**关键点：**
1. `test_data_` - 存储测试名称和函数指针
2. `test_ptr_` - 指向 test_data 的指针，被放入 section
3. `CA_SAFE_NAME` - 使用 `__LINE__` 生成唯一名称，避免冲突
4. `TEST_CASE_ALLOC` - section 属性，控制数据存放位置

### 1.3 Section 布局

数据在内存中的布局：

```
[.test$a]  _test_start (NULL，作为起始标记)
[.test$m]  test_ptr_1  -> test_data_1
           test_ptr_2  -> test_data_2
           test_ptr_3  -> test_data_3
           ...
[.test$z]  _test_stop (NULL，作为结束标记)
```

**MSVC 特殊处理：**

```c
#if defined(_MSC_VER)
    #pragma section(".test$a", read)
    #pragma section(".test$m", read)
    #pragma section(".test$z", read)
    #define TEST_CASE_ALLOC __declspec(allocate(".test$m"))
#else
    #define TEST_CASE_ALLOC __attribute__((used, section("test_array")))
#endif
```

MSVC 使用 `.test$a`、`.test$m`、`.test$z` 三段式布局，链接器会自动按字母顺序排列，确保 `$a` 在最前，`$z` 在最后。

### 1.4 遍历算法

**GCC/Clang：**

链接器自动生成 `__start_test_array` 和 `__stop_test_array` 符号：

```c
extern const test_t* __start_test_array[];
extern const test_t* __stop_test_array[];

for (const test_t** t = __start_test_array; t < __stop_test_array; t++) {
    if (*t != NULL && (*t)->name != NULL) {
        (*t)->func();  // 执行测试
    }
}
```

**MSVC：**

使用显式定义的起止标记：

```c
__declspec(allocate(".test$a")) const test_t* _test_start = NULL;
__declspec(allocate(".test$z")) const test_t* _test_stop  = NULL;

for (const test_t** it = &_test_start; it <= &_test_stop; it++) {
    if (*it != NULL && (*it)->name != NULL) {
        (*it)->func();  // 执行测试
    }
}
```

### 1.5 链接器脚本（可选）

对于嵌入式平台，可能需要自定义链接器脚本：

```ld
SECTIONS {
    .text : { *(.text*) }
    
    .test_array : {
        __start_test_array = .;
        *(.test_array)
        __stop_test_array = .;
    }
}
```

---

## 2. 插件架构设计

### 2.1 设计目标

- **可扩展**：用户可自定义输出方式
- **零侵入**：核心框架保持极简
- **易用**：提供默认实现，开箱即用

### 2.2 架构概览

```
┌─────────────────────────────────────────┐
│           测试框架核心                   │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐ │
│  │ 收集器  │  │ 执行器  │  │ 统计器  │ │
│  └────┬────┘  └────┬────┘  └────┬────┘ │
│       └─────────────┴─────────────┘     │
│                   │                      │
│              插件接口层                  │
│                   │                      │
└───────────────────┼─────────────────────┘
                    │
        ┌───────────┼───────────┐
        ▼           ▼           ▼
┌───────────┐ ┌───────────┐ ┌───────────┐
│ 终端插件   │ │ 文件插件   │ │ 用户插件   │
│ (默认)    │ │ (可选)    │ │ (自定义)  │
└───────────┘ └───────────┘ └───────────┘
```

### 2.3 输出回调机制

框架通过回调函数实现插件：

```c
typedef void (*test_output_fn)(const char* msg);

void test_set_output(test_output_fn fn);
```

**默认终端输出：**

```c
static void test_default_output(const char* msg) {
    printf("%s", msg);
}
```

**自定义输出示例：**

```c
void my_output(const char* msg) {
    // 可以同时输出到多个目标
    printf("%s", msg);              // 终端
    fprintf(log_file, "%s", msg);   // 文件
    send_to_network(msg);           // 网络
}
```

### 2.4 文件记录器插件实现

```c
static FILE* g_fp = NULL;
static test_output_fn g_original_output = NULL;

static void test_output_to_both(const char* msg) {
    // 调用原始输出（终端）
    if (g_original_output) {
        g_original_output(msg);
    } else {
        printf("%s", msg);
    }
    
    // 写入文件
    if (g_fp != NULL) {
        fprintf(g_fp, "%s", msg);
        fflush(g_fp);
    }
}

int test_file_recorder_init(const char* filepath, int append) {
    g_fp = fopen(filepath, append ? "a" : "w");
    if (g_fp == NULL) return -1;
    
    // 保存原始输出并设置新的输出
    test_set_output(test_output_to_both);
    return 0;
}
```

### 2.5 插件自动注册机制

#### 2.5.1 设计目标

实现类似 `TEST_CASE` 的插件自动收集机制：
- **零配置**：添加插件文件即可，无需修改main
- **自动初始化**：框架自动调用插件初始化函数
- **批量注册**：支持多个插件并存

#### 2.5.2 实现原理

插件自动注册机制与测试用例收集机制完全类似：

**1. 插件结构体**

```c
typedef struct {
    const char* name;                           // 插件名称
    test_plugin_init_fn init;                   // 初始化函数
    test_plugin_cleanup_fn cleanup;             // 清理函数
    test_plugin_suite_start_fn suite_start;     // 套件开始回调
    test_plugin_suite_end_fn suite_end;         // 套件结束回调
    test_plugin_test_start_fn test_start;       // 测试开始回调
    test_plugin_test_end_fn test_end;           // 测试结束回调
    test_plugin_assert_fail_fn assert_fail;     // 断言失败回调
    void* user_data;                            // 用户数据
} test_plugin_t;
```

**2. 独立的 Section**

插件使用单独的 section `test_plugin`（MSVC: `.tplugin$m`）：

```c
#if defined(_MSC_VER)
#    pragma section(".tplugin$a", read)
#    pragma section(".tplugin$m", read)
#    pragma section(".tplugin$z", read)
#    define TEST_PLUGIN_ALLOC __declspec(allocate(".tplugin$m"))
#else
#    define TEST_PLUGIN_ALLOC __attribute__((used, section("test_plugin")))
#endif
```

**3. 自动注册宏 TEST_PLUGIN_REGISTER**

```c
#define TEST_PLUGIN_REGISTER(plugin_name, init_func)                                           \
    static void init_func(void);                                                               \
    static const test_plugin_t test_plugin_data_##plugin_name = {                              \
        CA_MAKE_STRING(plugin_name),                                                           \
        init_func,                                                                             \
        NULL, NULL, NULL, NULL, NULL, NULL,                                                    \
        NULL                                                                                   \
    };                                                                                         \
    static TEST_PLUGIN_ALLOC const test_plugin_t* test_plugin_ptr_##plugin_name = &test_plugin_data_##plugin_name
```

**展开后示例：**

```c
TEST_PLUGIN_REGISTER(json_reporter, json_init);
```

展开为：

```c
static void json_init(void);
static const test_plugin_t test_plugin_data_json_reporter = {
    "json_reporter",
    json_init,
    NULL, NULL, NULL, NULL, NULL, NULL,
    NULL
};
static __attribute__((used, section("test_plugin"))) 
    const test_plugin_t* test_plugin_ptr_json_reporter = &test_plugin_data_json_reporter;
```

**4. 批量初始化**

在 `run_tests()` 中自动遍历并初始化所有插件：

```c
static void test_plugins_init(void) {
#if defined(_MSC_VER)
    const test_plugin_t** begin = (const test_plugin_t**)&_plugin_start;
    const test_plugin_t** end   = (const test_plugin_t**)&_plugin_stop;
    
    for (const test_plugin_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL && (*it)->init != NULL) {
            (*it)->init();  // 调用插件初始化函数
        }
    }
#else
    // GCC/Clang 版本...
#endif
}
```

#### 2.5.3 生命周期调用

```
run_tests()
    ├─ test_plugins_init()          // 1. 批量初始化所有插件
    │   └─ 遍历所有 TEST_PLUGIN_REGISTER 注册的插件
    │       └─ 调用 plugin->init()
    │           └─ 插件在init中设置回调函数
    │
    ├─ g_plugin_suite_start()       // 2. 套件开始
    │
    ├─ 遍历测试:
    │   ├─ g_plugin_test_start()    // 3. 测试开始
    │   ├─ 执行测试...
    │   └─ g_plugin_test_end()      // 4. 测试结束
    │
    ├─ g_plugin_suite_end()         // 5. 套件结束
    │
    └─ test_plugins_cleanup()       // 6. 批量清理所有插件
```

#### 2.5.4 与测试用例机制的对比

| 特性 | TEST_CASE | TEST_PLUGIN_REGISTER |
|-----|-----------|---------------------|
| Section | `test_array` / `.test$m` | `test_plugin` / `.tplugin$m` |
| 存储内容 | `test_t*` 指针 | `test_plugin_t*` 指针 |
| 自动收集 | ✓ | ✓ |
| 批量处理 | `run_tests()` 遍历 | `test_plugins_init()` 遍历 |
| 用户代码 | 在宏内定义测试函数 | 在宏外定义init函数 |
| 配置方式 | 无需配置 | 在init函数中设置回调 |

#### 2.5.5 使用示例

**插件实现：**

```c
// json_reporter.c
#include "test.h"
#include <stdio.h>

static FILE* g_report = NULL;

static void json_suite_start(int test_count) {
    g_report = fopen("report.json", "w");
    fprintf(g_report, "{\"tests\": [");
}

static void json_test_end(const char* name, int passed) {
    fprintf(g_report, "{\"name\":\"%s\",\"status\":\"%s\"},",
            name, passed ? "passed" : "failed");
}

static void json_suite_end(int passed, int failed) {
    fprintf(g_report, "],\"passed\":%d,\"failed\":%d}", passed, failed);
    fclose(g_report);
}

// 初始化函数 - 设置回调
static void json_init(void) {
    test_plugin_set_suite_start(json_suite_start);
    test_plugin_set_test_end(json_test_end);
    test_plugin_set_suite_end(json_suite_end);
}

// 自动注册 - 只需一行！
TEST_PLUGIN_REGISTER(json_reporter, json_init);
```

**用户使用：**

```c
// main.c
#include "test.h"

int main() {
    return run_tests();  // 插件自动生效！
}
```

**构建：**

```lua
-- xmake.lua
target("my_test")
    add_files("main.c")
    add_files("json_reporter.c")  -- 添加即可，自动注册
```

---

## 3. 类型安全机制

### 3.1 整数类型提升问题

**问题：**

```c
int8_t a = -1;      // 0xFF
uint8_t b = 255;    // 0xFF

// C语言会提升为int比较
if (a == b)  // false! -1 != 255
```

**解决方案：**

使用显式类型转换，确保比较的是目标类型：

```c
#define TEST_EXPECT_EQ_I8(e, a) \
    test_assert_eq_i8(__FILE__, __LINE__, ..., (int8_t)(e), (int8_t)(a))

void test_assert_eq_i8(...) {
    // 直接比较int8_t，不会发生提升
    if (expected != actual) { ... }
}
```

### 3.2 浮点数比较

**问题：** 浮点数不能直接相等比较

**解决方案：** 使用 epsilon 容差

```c
void test_assert_eq_f32(..., float epsilon) {
    float diff = expected > actual ? expected - actual : actual - expected;
    if (diff > epsilon) {
        // 失败
    }
}
```

---

## 4. xmake Rule 集成

### 4.1 Rule 实现

```lua
rule("em_test")
    on_load(function (target)
        local configs = target:extraconf("rules", "em_test")
        
        -- 自动添加框架源码
        target:add("files", path.join(os.scriptdir(), "test.c"))
        
        -- 自动添加头文件路径
        target:add("includedirs", os.scriptdir(), {public = true})
        
        -- 设置组别
        target:set("group", "em/test")
        
        -- 配置宏
        if configs then
            if configs.test_enable then
                target:add("defines", "TEST_ENABLE=1")
            end
            if configs.use_default_main then
                target:add("defines", "TEST_SELF_MAIN=1")
                target:add("files", path.join(os.scriptdir(), "test_main.c"))
            end
        end
    end)
```

### 4.2 使用方式

```lua
target("my_test")
    set_kind("binary")
    add_rules("em_test", { 
        test_enable = true, 
        use_default_main = true 
    })
    add_files("my_test.c")
```

---

## 5. 性能考虑

### 5.1 零开销抽象

- **宏展开**：所有断言都是宏，编译时展开，无函数调用开销
- **静态数组**：测试用例存储在静态数组，无动态内存分配
- **直接遍历**：运行时直接遍历指针数组，O(n)复杂度

### 5.2 内存占用

每个测试用例的内存开销：
- `test_t` 结构体：约 16 字节（64位平台）
- 指针存储：8 字节
- 总计：约 24 字节/测试

1000个测试约占用 24KB 内存。

### 5.3 编译时间

- section 属性不影响编译时间
- 宏展开在预处理阶段完成
- 链接时间略微增加（需要处理 section）

---

## 6. 移植性说明

### 6.1 支持的编译器

| 编译器 | 支持 | 备注 |
|-------|------|------|
| GCC | ✓ | 完全支持 |
| Clang | ✓ | 完全支持 |
| MSVC | ✓ | 需要特殊处理 |
| ARMCC | ✓ | 嵌入式支持 |
| IAR | ✓ | 嵌入式支持 |

### 6.2 平台特殊处理

**MSVC：**
- 使用三段式 section（$a, $m, $z）
- 显式定义起止标记
- 不使用链接器自动生成符号

**嵌入式平台：**
- 可能需要自定义链接器脚本
- 注意 section 对齐要求
- 考虑 RAM/ROM 限制

---

## 7. 调试技巧

### 7.1 查看 Section 内容

**测试用例 Section：**

```bash
# Linux/GCC
readelf -S my_test | grep test_array
readelf -p test_array my_test

# Windows/MSVC
dumpbin /SECTION:.test$m my_test.exe
```

**插件 Section：**

```bash
# Linux/GCC
readelf -S my_test | grep test_plugin
readelf -p test_plugin my_test

# Windows/MSVC
dumpbin /SECTION:.tplugin$m my_test.exe
```

**查看所有 section：**

```bash
# 查看 section 数量和大小
readelf -S my_test | grep -E "(test_array|test_plugin)"

# 典型输出：
# [25] test_array        PROGBITS  0000000000000000 001000 000180 00   0   0  8
# [26] test_plugin       PROGBITS  0000000000000000 001180 000040 00   0   0  8
```

### 7.2 验证测试收集

在 `run_tests()` 中添加调试输出：

```c
printf("Test count: %d\n", total_tests);
for (const test_t** t = begin; t < end; t++) {
    if (*t && (*t)->name) {
        printf("  Found: %s\n", (*t)->name);
    }
}
```

### 7.3 常见问题

**问题1：测试未被收集**
- 检查 section 属性是否正确
- 确认链接器是否保留 section
- 检查 `__start/__stop` 符号是否存在

**问题2：MSVC 链接错误**
- 确保 `#pragma section` 在使用前定义
- 检查 `__declspec(allocate)` 语法

**问题3：嵌入式平台不工作**
- 检查链接器脚本
- 确认 section 名称无冲突
- 验证内存对齐

**问题4：插件未被收集**
- 检查是否使用了 `TEST_PLUGIN_REGISTER` 宏
- 确认插件文件被添加到编译
- 查看 `test_plugin` section 是否存在

```bash
# 检查插件section
readelf -S my_test | grep test_plugin
```

**问题5：插件回调未生效**
- 确认在 `init` 函数中设置了回调
- 检查回调函数是否正确实现
- 在 `init` 函数中添加调试输出确认被调用

```c
static void my_plugin_init(void) {
    printf("Plugin init called\n");  // 调试用
    test_plugin_set_suite_start(my_callback);
}
```

---

## 8. 扩展指南

### 8.1 添加新断言类型

1. 在 test.h 中添加宏：

```c
#define TEST_EXPECT_EQ_CUSTOM(expected, actual) \
    test_assert_eq_custom(__FILE__, __LINE__, #expected, #actual, expected, actual)
```

2. 在 test.c 中实现函数：

```c
void test_assert_eq_custom(...) {
    // 自定义比较逻辑
}
```

### 8.2 实现新插件（使用 TEST_PLUGIN_REGISTER）

**步骤1：创建插件文件**

```c
// my_plugin.c
#include "test.h"
#include <stdio.h>
```

**步骤2：定义回调函数**

```c
// 私有数据
static FILE* g_log = NULL;
static int g_test_count = 0;

// 套件开始回调
static void my_suite_start(int test_count) {
    g_log = fopen("my_plugin.log", "w");
    fprintf(g_log, "Starting %d tests\n", test_count);
    g_test_count = 0;
}

// 测试开始回调
static void my_test_start(const char* test_name) {
    fprintf(g_log, "[%d] Running: %s\n", ++g_test_count, test_name);
}

// 测试结束回调
static void my_test_end(const char* test_name, int passed) {
    fprintf(g_log, "    Result: %s\n", passed ? "PASS" : "FAIL");
}

// 套件结束回调
static void my_suite_end(int passed, int failed) {
    fprintf(g_log, "Finished: %d passed, %d failed\n", passed, failed);
    fclose(g_log);
    g_log = NULL;
}
```

**步骤3：实现初始化函数**

```c
// 插件初始化函数 - 在此注册所有回调
static void my_plugin_init(void) {
    // 设置需要的回调（可选）
    test_plugin_set_suite_start(my_suite_start);
    test_plugin_set_test_start(my_test_start);
    test_plugin_set_test_end(my_test_end);
    test_plugin_set_suite_end(my_suite_end);
    
    // 可以在此处打开文件、连接网络等
    printf("My plugin loaded\n");
}

// 可选：实现清理函数
static void my_plugin_cleanup(void) {
    // 关闭资源
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}
```

**步骤4：自动注册插件**

```c
// 关键：使用 TEST_PLUGIN_REGISTER 自动注册
TEST_PLUGIN_REGISTER(my_plugin, my_plugin_init);
```

**完整示例：**

```c
// jenkins_reporter.c
#include "test.h"
#include <stdio.h>
#include <time.h>

static FILE* g_xml = NULL;

static void xml_suite_start(int test_count) {
    g_xml = fopen("test_result.xml", "w");
    fprintf(g_xml, "<?xml version=\"1.0\"?>\n");
    fprintf(g_xml, "<testsuites>\n");
    fprintf(g_xml, "  <testsuite name=\"em_test\" tests=\"%d\">\n", test_count);
}

static void xml_test_end(const char* name, int passed) {
    fprintf(g_xml, "    <testcase name=\"%s\">\n", name);
    if (!passed) {
        fprintf(g_xml, "      <failure/>\n");
    }
    fprintf(g_xml, "    </testcase>\n");
}

static void xml_suite_end(int passed, int failed) {
    fprintf(g_xml, "  </testsuite>\n");
    fprintf(g_xml, "</testsuites>\n");
    fclose(g_xml);
}

static void xml_init(void) {
    test_plugin_set_suite_start(xml_suite_start);
    test_plugin_set_test_end(xml_test_end);
    test_plugin_set_suite_end(xml_suite_end);
}

TEST_PLUGIN_REGISTER(jenkins_reporter, xml_init);
```

**使用插件：**

```lua
-- xmake.lua
target("my_test")
    add_files("main.c")
    add_files("jenkins_reporter.c")  -- 自动注册！
```

```c
// main.c
#include "test.h"

int main() {
    return run_tests();  // 插件自动生效
}
```

**回调选择指南：**

| 如果你需要 | 设置回调 |
|-----------|---------|
| 打开输出文件 | `suite_start` |
| 写入测试开始信息 | `test_start` |
| 写入测试结果 | `test_end` |
| 关闭文件并生成报告 | `suite_end` |
| 实时捕获断言失败 | `assert_fail` |

---

**文档版本：** v1.0  
**最后更新：** 2026-02-05  
**状态：** 实现原理文档
