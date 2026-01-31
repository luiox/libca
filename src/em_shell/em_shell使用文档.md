# em_shell 使用文档

## 目录

- [快速开始](#快速开始)
- [API 参考](#api-参考)
- [常见用例](#常见用例)
- [调试技巧](#调试技巧)
- [常见问题](#常见问题)
- [内置命令](#内置命令)
- [编译运行](#编译运行)

---

## 快速开始

### 5 分钟上手

#### 第 1 步：定义 I/O 端口

```c
/* 实现串口写入 */
static i32 uart_write(void *self, const void *buf, usize size)
{
    HAL_UART_Transmit(UART1, (uint8_t *)buf, size, 100);
    return (i32)size;
}

/* 实现串口读取（可选） */
static i32 uart_read(void *self, void *buf, usize size)
{
    return HAL_UART_Receive(UART1, (uint8_t *)buf, size, 0);
}

/* 创建端口对象 */
shell_port_t my_port = {
    .write_bytes = uart_write,
    .read_bytes = uart_read,
    .priv = NULL,
};
```

#### 第 2 步：定义自定义命令

```c
/* 简单命令 */
static i32 cmd_hello(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    shell_print(shell, "Hello, World!\r\n");
    return 0;
}

/* 带参数的命令 */
static i32 cmd_set_param(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    
    char *name = NULL;
    u32 value = 0;
    
    if (!shell_parse_short_param(argc, argv, "-n", &name) ||
        !shell_parse_short_hex_param(argc, argv, "-v", &value)) {
        shell_print(shell, "Usage: set -n <name> -v <hex_value>\r\n");
        return -1;
    }
    
    shell_print(shell, "Set %s = 0x%X\r\n", name, value);
    return 0;
}
```

#### 第 3 步：构建命令数组

```c
static const shell_cmd_t g_commands[] = {
    {
        .name = "hello",
        .help = "print hello world",
        .u.handler = cmd_hello,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "set",
        .help = "set parameter (-n <name> -v <hex>)",
        .u.handler = cmd_set_param,
        .sub_count = 0,
        .is_group = false,
    },
};
```

#### 第 4 步：初始化 shell

```c
shell_t my_shell;
char shell_buffer[SHELL_LINE_BUFFER];

shell_init(&my_shell, &my_port, shell_buffer, sizeof(shell_buffer),
           g_commands, sizeof(g_commands) / sizeof(g_commands[0]));

shell_print(&my_shell, "Welcome to my shell!\r\n> ");
```

#### 第 5 步：在中断中处理输入

```c
void USART1_IRQHandler(void)
{
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
        char ch = USART_ReceiveData(USART1);
        shell_handler(&my_shell, ch);  /* 传递给 shell */
    }
}
```

完成！现在在终端中输入 `hello` 或 `set -n param1 -v 0x100` 试试。

---

## API 参考

### 初始化 API

```c
/**
 * 初始化 shell 对象
 * @param shell shell 对象指针
 * @param port I/O 端口接口
 * @param buffer 行缓冲区地址
 * @param size 缓冲区大小
 * @param cmd_root 命令树根节点
 * @param cmd_count 根节点命令总数
 */
void shell_init(shell_t *shell, shell_port_t *port, char *buffer, u16 size,
                const shell_cmd_t *cmd_root, u16 cmd_count);

/**
 * 获取当前全局 shell 对象
 */
shell_t* shell_get_current(void);
```

### 命令执行 API

```c
/**
 * 处理一个输入字符
 * @param shell shell 对象指针
 * @param data 输入字符
 * @details 自动处理回车/换行、退格等编辑字符
 */
void shell_handler(shell_t *shell, char data);

/**
 * 通过命令行文本直接执行命令
 * @param line 命令行字符串（不应包含 \r \n）
 * @return 命令执行的返回值
 */
i32 shell_run_command_by_name(const char *line);
```

### 输出 API

```c
/**
 * 格式化输出到 shell
 * @param shell shell 对象指针
 * @param fmt 格式化字符串
 * @return 输出的字符数
 */
i32 shell_print(shell_t *shell, const char *fmt, ...);

/**
 * 十六进制转储输出
 * @param shell shell 对象指针
 * @param addr 起始地址
 * @param data 待转储数据指针
 * @param len 转储长度（字节数）
 */
void shell_hexdump(shell_t *shell, u32 addr, const u8 *data, u32 len);
```

### 参数解析 API

```c
/**
 * 解析十六进制字符串
 * @param s 输入字符串
 * @param out 输出值
 * @return 1 成功，0 失败
 * @example shell_parse_hex("0x1F", &value) → value=31
 */
i32 shell_parse_hex(const char *s, u32 *out);

/**
 * 查找并返回短参数值
 * @param argc 参数个数
 * @param argv 参数列表
 * @param key 要查找的参数键（如 "-r"）
 * @param value 参数值
 * @return 1 找到，0 未找到
 * @example shell_parse_short_param(4, argv, "-r", &val)
 *          若 argv 包含 ["-r", "0x400"]，则 val="0x400"
 */
i32 shell_parse_short_param(i32 argc, char *argv[], const char *key, char **value);

/**
 * 查找并解析十六进制短参数
 * @param argc 参数个数
 * @param argv 参数列表
 * @param key 要查找的参数键
 * @param value 解析出的十六进制值
 * @return 1 成功，0 失败
 */
i32 shell_parse_short_hex_param(i32 argc, char *argv[], const char *key, u32 *value);
```

---

## 常见用例

#### 用例 1：分层命令（内存操作）

```c
static i32 cmd_mem_read(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    u32 addr = 0;
    shell_parse_short_hex_param(argc, argv, "-a", &addr);
    shell_print(shell, "[0x%X] = 0x%02X\r\n", addr, *(u8*)addr);
    return 0;
}

static i32 cmd_mem_write(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    u32 addr = 0;
    u32 value = 0;
    shell_parse_short_hex_param(argc, argv, "-a", &addr);
    shell_parse_short_hex_param(argc, argv, "-v", &value);
    *(u8*)addr = value;
    shell_print(shell, "[0x%X] <- 0x%02X\r\n", addr, value);
    return 0;
}

/* 子命令数组 */
static const shell_cmd_t g_mem_subcmds[] = {
    {.name="read", .help="read byte", .u.handler=cmd_mem_read, .is_group=false},
    {.name="write", .help="write byte", .u.handler=cmd_mem_write, .is_group=false},
};

/* 顶级命令 - 分支节点 */
static const shell_cmd_t g_commands[] = {
    {
        .name = "mem",
        .help = "memory operations",
        .u.sub = g_mem_subcmds,
        .sub_count = 2,
        .is_group = true,  /* 重要：标记为分支 */
    },
};

/* 使用：
   > mem read -a 0x20000000
   [0x20000000] = 0xFF
   
   > mem write -a 0x20000000 -v 0xAA
   [0x20000000] <- 0xAA
*/
```

#### 用例 2：多级参数解析

```c
static i32 cmd_config(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    
    char *mode = NULL;
    u32 timeout = 0;
    char *name = NULL;
    
    /* 解析多个参数 */
    shell_parse_short_param(argc, argv, "-m", &mode);
    shell_parse_short_hex_param(argc, argv, "-t", &timeout);
    shell_parse_short_param(argc, argv, "-n", &name);
    
    shell_print(shell, "Config: mode=%s, timeout=%d, name=%s\r\n", 
                mode ? mode : "N/A", timeout, name ? name : "N/A");
    return 0;
}

/* 使用：
   > config -m test -t 0x64 -n device1
   Config: mode=test, timeout=100, name=device1
*/
```

#### 用例 3：显示内存转储

```c
static i32 cmd_hexdump(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    u32 addr = 0;
    u32 len = 16;
    
    shell_parse_short_hex_param(argc, argv, "-a", &addr);
    shell_parse_short_hex_param(argc, argv, "-l", &len);
    
    shell_hexdump(shell, addr, (u8*)addr, len);
    return 0;
}

/* 使用：
   > hexdump -a 0x20000000 -l 0x20
   20000000: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF
   20000010: AA AA AA AA AA AA AA AA AA AA AA AA AA AA AA AA
*/
```

---

## 调试技巧

#### 实时调试 - 添加调试命令

```c
static i32 cmd_debug_info(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    shell_print(shell, "RAM: %d / %d bytes\r\n", get_used_ram(), MAX_RAM);
    shell_print(shell, "Uptime: %d seconds\r\n", get_uptime());
    return 0;
}
```

#### 验证参数解析

```c
/* 用 shell_parse_short_param 调试 */
char *value = NULL;
if (!shell_parse_short_param(argc, argv, "-x", &value)) {
    shell_print(shell, "ERROR: -x parameter not found\r\n");
    return -1;
}
shell_print(shell, "DEBUG: -x = %s\r\n", value);
```

#### 监控命令执行

```c
/* 在命令处理函数开头/结尾添加日志 */
static i32 my_command(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    shell_print(shell, "[EXEC] my_command: argc=%d\r\n", argc);
    
    /* 实际业务 */
    
    shell_print(shell, "[DONE] my_command\r\n");
    return 0;
}
```

---

## 常见问题

**Q: 命令无法执行？**

A: 检查以下几点：
1. 命令树是否正确初始化？`shell_init()` 的 `cmd_root` 和 `cmd_count` 参数
2. 分支节点的 `is_group` 是否为 `true`？
3. 子命令数组的 `sub_count` 是否正确？
4. 是否在中断中正确调用 `shell_handler()`？

**Q: 参数解析失败？**

A: 确保：
1. 参数格式：`-key value` 中间有空格
2. 十六进制参数：确实以 `0x` 开头（不区分大小写）
3. 参数索引合法：`shell_parse_short_param` 返回 `0` 时，参数不存在

**Q: 如何添加更多命令？**

A: 直接在 `g_commands` 数组中添加新元素，重新编译即可。无需修改 `shell.c`。

**Q: 是否支持命令别名？**

A: 可以！创建多个命令节点指向同一个处理函数：
```c
static const shell_cmd_t g_commands[] = {
    {.name="mem", .u.handler=cmd_mem, ...},
    {.name="m", .u.handler=cmd_mem, ...},  /* 别名 */
};
```

**Q: 是否可以用于生产环境？**

A: 完全可以。轻量级设计、完整测试、无动态分配。

---

## 内置命令

| 命令 | 说明 |
|------|------|
| `help` | 列出所有可用命令 |
| `clear` | 清屏（ANSI 转义序列） |

---

## 编译运行

```bash
# 编译项目
xmake build

# 在实际硬件或模拟环境中使用 shell

# 运行单元测试
xmake run test-shell
```

---

## 性能指标

| 指标 | 值 |
|------|-----|
| 最大命令树深度 | 无限 |
| 最大参数个数 | 32（可配置） |
| 输入行缓冲 | 256 字节（可配置） |
| 输出缓冲 | 256 字节（内部） |
| 内存占用 | 仅 shell_t 结构体 + 缓冲区（无动态分配） |
| 字符处理延迟 | < 1ms（取决于硬件） |

---

## 完整工程示例

详见工程中的 `example.c` 文件，包含：
- 完整的 UART 端口实现
- 多级命令定义
- 参数解析示例
- 十六进制转储
