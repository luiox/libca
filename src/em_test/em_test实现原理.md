# em_test 实现原理

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

### 2.5 生命周期事件

虽然当前实现简化了生命周期，但预留了扩展点：

```c
typedef struct {
    void (*on_suite_start)(int test_count);
    void (*on_test_start)(const char* name);
    void (*on_assert_fail)(const char* file, int line, ...);
    void (*on_test_end)(const char* name, int passed);
    void (*on_suite_end)(int passed, int failed);
} test_plugin_t;
```

**未来扩展方向：**
- JSON/XML 报告生成器
- IDE 集成插件
- CI/CD 集成插件
- 性能分析插件

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

**Linux/GCC：**

```bash
# 查看 section 大小
readelf -S my_test | grep test_array

# 查看 section 内容
readelf -p test_array my_test
```

**Windows/MSVC：**

```bash
# 查看 section
dumpbin /SECTION:.test$m my_test.exe
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

### 8.2 实现新插件

1. 创建插件文件
2. 实现输出回调
3. 提供初始化/清理函数

示例见 `simple_file_recorder.c`

---

**文档版本：** v1.0  
**最后更新：** 2026-02-05  
**状态：** 实现原理文档
