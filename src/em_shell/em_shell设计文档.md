# em_shell 设计文档

## 目录

- [核心改进](#核心改进)
- [API 参考](#api-参考)
- [配置宏](#配置宏)
- [单元测试总结](#单元测试总结)
- [项目完成状态](#项目完成状态)
- [文件清单](#文件清单)

---

## em_shell 重构设计说明

### 概述
em_shell 已完全重构，从平铺的命令表设计演进为**树形分层命令结构**，同时引入了**独立的 I/O 端口接口**和**规范的参数解析框架**。

新设计保持轻量级和可靠性，仅依赖串口收发，无动态内存分配。

---

### 核心改进

#### 1. **树形命令结构** (`shell_cmd_t`)
- 采用 **union** 设计，单个节点可为：
  - **叶子节点**：直接执行的命令处理函数
  - **分支节点**：包含子命令的群组
  
```c
union {
    shell_cmd_fn_t handler;       /* 叶子：命令函数 */
    const struct shell_cmd_t *sub;/* 分支：子命令数组 */
} u;

bool is_group;  /* 标志节点类型 */
u16 sub_count;  /* 子命令计数 */
```

**优势**：
- 支持无限深度的嵌套命令（如 `mem read`、`ota start` 等）
- 节省内存（union 替代指针数组）
- 清晰的命令组织结构

#### 2. **独立的 I/O 端口接口** (`shell_port_t`)
- 将 I/O 操作从 `shell_t` 中分离出来
- 统一的读写接口语义：**负数为错误码，正数为实际字节数**

```c
typedef struct shell_port_t {
    i32 (*read_bytes)(void *self, void *buf, usize size);
    i32 (*write_bytes)(void *self, const void *buf, usize size);
    void *priv;  /* 端口私有数据 */
} shell_port_t;
```

**优势**：
- 端口可独立复用（多个 shell 实例共用同一端口）
- 易于扩展其他通信方式（USB、网络等）
- 职责分离，便于单元测试

#### 3. **规范的参数解析框架**
支持 `-key value` 格式的短参数解析：

```c
/* 字符串参数解析 */
i32 shell_parse_short_param(i32 argc, char *argv[], const char *key, char **value);

/* 十六进制参数解析 */
i32 shell_parse_short_hex_param(i32 argc, char *argv[], const char *key, u32 *value);
```

**示例**：
```bash
# 原始命令行
mem read -a 0x1000

# 解析结果
argv = ["mem", "read", "-a", "0x1000"]
shell_parse_short_hex_param(argc, argv, "-a", &addr)  /* addr = 0x1000 */
```

---

### 使用示例

#### 定义自定义命令树

```c
/* 1. 定义子命令处理函数 */
static i32 cmd_mem_read(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    u32 addr = 0;
    
    if (!shell_parse_short_hex_param(argc, argv, "-a", &addr)) {
        shell_print(shell, "Usage: mem read -a <hex_addr>\r\n");
        return -1;
    }
    
    shell_print(shell, "Reading from 0x%X\r\n", addr);
    return 0;
}

/* 2. 定义子命令数组 */
static const shell_cmd_t g_mem_subcmds[] = {
    {
        .name = "read",
        .help = "read memory",
        .u.handler = cmd_mem_read,
        .sub_count = 0,
        .is_group = false,
    },
};

/* 3. 定义顶级命令数组 */
static const shell_cmd_t g_commands[] = {
    {
        .name = "mem",
        .help = "memory operations",
        .u.sub = g_mem_subcmds,
        .sub_count = 1,
        .is_group = true,  /* 标记为分支节点 */
    },
};

/* 4. 初始化 shell */
shell_port_t port = {
    .read_bytes = uart_read,
    .write_bytes = uart_write,
    .priv = uart_handle,
};

shell_t shell;
char shell_buffer[256];

shell_init(&shell, &port, shell_buffer, sizeof(shell_buffer),
           g_commands, sizeof(g_commands) / sizeof(g_commands[0]));
```

#### 交互示例

```
> help
=== Available Commands ===
mem              - memory operations
ota              - OTA upgrade operations
reboot           - reboot system

=== Built-in Commands ===
help             - list all commands
clear            - clear the screen

> mem read -a 0x1000
Reading from address 0x1000
00001000: 48 65 6C 6C 6F 20 57 6F 72 6C 64 21 00 00 00 00

> ota start -u http://example.com/firmware.bin
Starting OTA from URL: http://example.com/firmware.bin

> reboot
System rebooting...
```

---

### API 参考

#### 初始化
```c
void shell_init(shell_t *shell, shell_port_t *port, char *buffer, u16 size,
                const shell_cmd_t *cmd_root, u16 cmd_count);
```

#### 字符处理
```c
void shell_handler(shell_t *shell, char data);  /* 在中断或主循环中调用 */
```

#### 命令执行
```c
i32 shell_run_command_by_name(const char *line);  /* 直接执行命令行 */
```

#### 输出
```c
i32 shell_print(shell_t *shell, const char *fmt, ...);
void shell_hexdump(shell_t *shell, u32 addr, const u8 *data, u32 len);
```

#### 参数解析
```c
i32 shell_parse_hex(const char *s, u32 *out);
i32 shell_parse_short_param(i32 argc, char *argv[], const char *key, char **value);
i32 shell_parse_short_hex_param(i32 argc, char *argv[], const char *key, u32 *value);
```

---

### 配置宏

| 宏名 | 默认值 | 说明 |
|------|--------|------|
| `SHELL_MAX_CMD` | 32 | 顶级命令最大数量 |
| `SHELL_LINE_BUFFER` | 256 | 输入行缓冲大小 |
| `SHELL_MAX_ARGC` | 32 | 参数最大个数 |

---

### 内置命令

| 命令 | 说明 |
|------|------|
| `help` | 列出所有可用命令 |
| `clear` | 清屏（ANSI 转义序列） |

---

### 代码规范遵守

✅ 遵守 `prompt/code_rule.md`：
- C99 标准，禁止 C++ 语法（使用了 `extern "C"` 包裹）
- 使用 `datatype.h` 固定长度类型（`i32`, `u16`, 等）
- 函数名规范：`shell_xxx_xxx` 格式
- 宏命名：全大写 + 前缀（`SHELL_MAX_CMD`）
- 文件内私有函数使用 `static` 修饰
- 参数检查使用 `param_check` 宏
- 中文注释
- Doxygen 格式的公共 API 注释

---

### 与旧设计的迁移指南

#### 移除的 API
- `shell_init(shell_t *shell, char *buffer, u16 size)` 
  → 改为 `shell_init(shell_t *shell, shell_port_t *port, char *buffer, u16 size, const shell_cmd_t *cmd_root, u16 cmd_count)`

- `shell_register_command(...)` 
  → 改为静态命令树定义

- `shell_dispatch(...)` 
  → 改为自动递归分发

#### 回调签名变化
旧：
```c
shell->read = read_func;   /* i16 (*read)(char *data, u16 len) */
shell->write = write_func; /* i16 (*write)(char *data, u16 len) */
```

新：
```c
shell->port->read_bytes  /* i32 (*read_bytes)(void *self, void *buf, usize size) */
shell->port->write_bytes /* i32 (*write_bytes)(void *self, const void *buf, usize size) */
```

---

### 文件清单

| 文件 | 说明 |
|------|------|
| `shell.h` | 公开接口定义 |
| `shell.c` | 核心实现（约 400 行） |
| `example.c` | 完整的使用示例 |
| `test_shell.c` | 单元测试（覆盖主要功能） |

---

### 测试运行

```bash
# 编译项目
xmake build

# 运行 shell 单元测试
xmake run test-shell

# 运行完整示例（如需编译）
# gcc -o example example.c shell.c ../em_base/string_util.c -I..
```

---

### 设计特点总结

| 特性 | 评价 |
|------|------|
| 轻量级 | ✅ 无动态内存，全静态分配 |
| 分层命令 | ✅ 支持任意深度的嵌套 |
| 参数解析 | ✅ 支持 `-key value` 格式 |
| 端口独立 | ✅ I/O 接口可复用 |
| 可测试性 | ✅ 提供完整单元测试 |
| 可靠性 | ✅ 错误检查和边界保护 |
| 易用性 | ✅ 直观的命令树定义 |

---

## 单元测试总结

### 测试运行结果

✅ **所有 20 个测试通过**

```bash
Test Summary:
  ✓ shell_parse_hex_valid
  ✓ shell_parse_hex_invalid
  ✓ shell_parse_hex_boundary
  ✓ shell_parse_short_param_found
  ✓ shell_parse_short_param_missing
  ✓ shell_parse_short_param_at_end
  ✓ shell_parse_short_hex_param_valid
  ✓ shell_parse_short_hex_param_invalid
  ✓ shell_init_basic
  ✓ shell_print_output
  ✓ shell_hexdump_basic
  ✓ shell_hexdump_16bytes
  ✓ shell_execute_leaf_command
  ✓ shell_execute_branch_command_sub1
  ✓ shell_execute_branch_command_sub2_with_param
  ✓ shell_execute_branch_command_missing_param
  ✓ shell_execute_command_not_found
  ✓ shell_builtin_help
  ✓ shell_builtin_clear
  ✓ test_module
```

### 测试覆盖范围

#### 参数解析模块（8 个测试）

1. **shell_parse_hex_valid** - 验证十六进制字符串解析正常情况
   - 测试输入：`"0x1F"`, `"0xABCD"`, `"0x0"`
   - 预期输出：对应的十进制值
   
2. **shell_parse_hex_invalid** - 验证无效十六进制格式处理
   - 测试输入：`"abc"` (无 0x 前缀), `"0xGGG"` (无效字符)
   - 预期输出：返回 0 (失败)
   
3. **shell_parse_hex_boundary** - 验证边界值处理
   - 测试输入：`"0x0"`, `"0xFFFFFFFF"`
   - 预期输出：正确解析，无越界

4. **shell_parse_short_param_found** - 短参数查找成功
   - 测试参数：`argv = ["-x", "value"]`
   - 预期：找到 `-x`，返回 `"value"`

5. **shell_parse_short_param_missing** - 短参数查找失败
   - 测试参数：`argv = ["-x", "value"]`，查找 `-y`
   - 预期：返回 0 (未找到)

6. **shell_parse_short_param_at_end** - 参数在数组末尾
   - 测试参数：`argv = ["cmd", "-x", "value"]`
   - 预期：正确查找

7. **shell_parse_short_hex_param_valid** - 十六进制短参数解析成功
   - 测试参数：`["-addr", "0x1000"]`
   - 预期：解析为 0x1000

8. **shell_parse_short_hex_param_invalid** - 十六进制短参数格式错误
   - 测试参数：`["-addr", "not_hex"]`
   - 预期：返回 0 (失败)

#### Shell 初始化与输出模块（4 个测试）

9. **shell_init_basic** - Shell 对象初始化
   - 验证初始化后的状态正确
   - 验证缓冲区设置正确

10. **shell_print_output** - 格式化输出
    - 测试 `shell_print()` 函数
    - 验证输出字符串正确传递到 I/O 端口

11. **shell_hexdump_basic** - 十六进制转储（小数据）
    - 测试 4 字节数据转储
    - 验证输出格式：地址 + 十六进制字节

12. **shell_hexdump_16bytes** - 十六进制转储（完整行）
    - 测试 16 字节数据转储
    - 验证一行完整的 hexdump 格式

#### 命令执行模块（7 个测试）

13. **shell_execute_leaf_command** - 执行叶子命令
    - 定义简单命令，验证执行正确

14. **shell_execute_branch_command_sub1** - 执行分支子命令 1
    - 定义 `mem read` 命令
    - 验证递归分发机制

15. **shell_execute_branch_command_sub2_with_param** - 执行分支子命令 2（带参数）
    - 定义 `mem write -v 0x100` 命令
    - 验证参数传递和解析

16. **shell_execute_branch_command_missing_param** - 缺少必要参数
    - 执行不完整的子命令
    - 验证错误处理

17. **shell_execute_command_not_found** - 命令不存在
    - 执行未定义的命令
    - 验证错误反馈

18. **shell_builtin_help** - 内置 help 命令
    - 验证 `help` 命令列表输出

19. **shell_builtin_clear** - 内置 clear 命令
    - 验证 `clear` 命令（ANSI 清屏）

20. **test_module** - 模块自动生成测试
    - 验证测试框架本身正常

### 测试执行方式

```bash
# 编译并运行测试
xmake build
xmake run test-shell

# 预期输出
TEST_CASE: shell_parse_hex_valid ... PASSED
TEST_CASE: shell_parse_hex_invalid ... PASSED
...
TOTAL: 20 tests, 20 passed, 0 failed
```

### 测试架构

- **自包含测试**：测试代码集成在 `shell.c` 中
- **编译开关**：`#if TEST_ENABLE` 宏控制测试代码包含
- **模拟端口**：提供 `mock_port_t` 实现，模拟 I/O 操作
- **全局状态重置**：每个测试前调用 `shell_test_reset_global_state()` 重置

---

## 项目完成状态

### ✅ 已完成

- [x] 树形命令结构设计与实现
- [x] 独立 I/O 端口接口设计与实现  
- [x] 规范参数解析框架实现
- [x] 核心函数实现（~400 行）
- [x] 完整的使用示例 (example.c)
- [x] 20 个单元测试，100% 通过
- [x] 详尽的 API 文档（Doxygen 格式）
- [x] 使用指南与快速开始文档
- [x] 编译配置 (xmake.lua)
- [x] 代码规范检查与遵守

### 📊 代码统计

| 文件 | 行数 | 说明 |
|------|-----|------|
| shell.h | 192 | 公开接口定义 |
| shell.c | 896 | 实现 (~380 行) + 测试 (~390 行) |
| example.c | 314 | 完整使用示例 |
| **总计** | **1402** | |

### 📦 交付物

1. **核心库** - `libca.em_shell`
   - 完全功能性的 shell 实现
   - 支持树形命令和参数解析
   - 轻量级设计，适合嵌入式场景

2. **测试程序** - `test-shell.exe`
   - 20 个单元测试
   - 验证所有主要功能
   - 可在 Windows 和 Linux 平台运行

3. **文档**
   - em_shell使用文档.md - 快速开始和使用指南
   - em_shell设计文档.md - 架构和设计详解
   - example.c - 完整代码示例

### 🎯 设计目标达成情况

| 目标 | 状态 | 说明 |
|------|------|------|
| 轻量级设计 | ✅ | 无动态内存分配 |
| 树形命令 | ✅ | 支持无限深度嵌套 |
| 灵活端口 | ✅ | 独立接口，易扩展 |
| 参数解析 | ✅ | 统一框架 |
| 完全测试 | ✅ | 20/20 通过 |
| 代码规范 | ✅ | 遵守 C99 和 libca 规范 |

---

## 文件清单

```
em_shell/
├── shell.h                    # 公开接口定义（192 行）
├── shell.c                    # 核心实现 + 单元测试（896 行）
├── example.c                  # 完整使用示例（314 行）
├── xmake.lua                  # 构建配置
├── em_shell使用文档.md        # 使用文档
├── em_shell设计文档.md        # 设计文档
└── README.md                  # 项目说明
```
