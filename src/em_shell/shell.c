#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../em_base/string_util.h"
#include "../em_base/debug.h"

/* ========== 前置声明 ========== */
static i32 shell_help(i32 argc, char *argv[]);
static i32 shell_clear(i32 argc, char *argv[]);
static i32 shell_execute_tree(const shell_cmd_t *root, u16 count, i32 argc, char *argv[]);

/* ========== 内置命令数组 ========== */
static const shell_cmd_t g_builtins[] = {
    {
        .name = "help",
        .help = "list all commands",
        .u.handler = shell_help,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "clear",
        .help = "clear the screen",
        .u.handler = shell_clear,
        .sub_count = 0,
        .is_group = false,
    },
};

#define BUILTIN_COUNT (sizeof(g_builtins) / sizeof(g_builtins[0]))

/* ========== 全局状态 ========== */
static shell_t *g_shell_current = NULL;

/* 用于测试的全局状态重置 */
#if TEST_ENABLE
static void shell_test_reset_global_state(void)
{
    g_shell_current = NULL;
}
#endif

/* ========== 工具函数 ========== */

/**
 * @brief 向 shell 端口写入数据
 */
static i32 shell_write_data(shell_t *shell, const void *buf, usize len)
{
    if (!shell || !shell->port || !shell->port->write_bytes) {
        return -1;
    }
    return shell->port->write_bytes(shell->port->priv, (void *)buf, len);
}

/**
 * @brief 从参数列表中解析参数，支持格式："-key" "value"
 */
static i32 find_argument_index(i32 argc, char *argv[], const char *key)
{
    param_check(key);
    for (i32 i = 0; i < argc - 1; i++) {
        if (str_is_equal(argv[i], key)) {
            return i + 1;  /* 返回值所在索引 */
        }
    }
    return -1;  /* 未找到 */
}

/* ========== 内置命令实现 ========== */

/**
 * @brief help 命令实现
 */
static i32 shell_help(i32 argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    shell_print(shell, "\r\n=== Available Commands ===\r\n");

    /* 列出用户定义的命令树 */
    if (shell->cmd_root && shell->cmd_count > 0) {
        for (u16 i = 0; i < shell->cmd_count; i++) {
            const shell_cmd_t *cmd = &shell->cmd_root[i];
            if (cmd->help) {
                shell_print(shell, "%-16s - %s\r\n", cmd->name, cmd->help);
            } else {
                shell_print(shell, "%-16s\r\n", cmd->name);
            }
        }
    }

    /* 列出内置命令 */
    shell_print(shell, "\r\n=== Built-in Commands ===\r\n");
    for (usize i = 0; i < BUILTIN_COUNT; i++) {
        if (g_builtins[i].help) {
            shell_print(shell, "%-16s - %s\r\n", g_builtins[i].name, g_builtins[i].help);
        } else {
            shell_print(shell, "%-16s\r\n", g_builtins[i].name);
        }
    }

    shell_print(shell, "\r\n");
    return 0;
}

/**
 * @brief clear 命令实现
 */
static i32 shell_clear(i32 argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    /* ANSI 转义序列：清屏并移动游标到左上角 */
    shell_print(shell, "\033[2J\033[H");
    return 0;
}

/* ========== 核心 API 实现 ========== */

void shell_init(shell_t *shell, shell_port_t *port, char *buffer, u16 size,
                const shell_cmd_t *cmd_root, u16 cmd_count)
{
    param_check(shell);
    param_check(port);
    param_check(buffer);
    
    shell->port = port;
    shell->buffer = buffer;
    shell->buffer_size = size;
    shell->cmd_root = cmd_root;
    shell->cmd_count = cmd_count;

    /* 仅第一个 shell 成为全局对象 */
    if (g_shell_current == NULL) {
        g_shell_current = shell;
    }
}

shell_t* shell_get_current(void)
{
    return g_shell_current;
}

i32 shell_print(shell_t *shell, const char *fmt, ...)
{
    if (!shell || !fmt) return -1;

    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    i32 n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n > 0) {
        shell_write_data(shell, tmp, (usize)n);
    }
    return n;
}

/**
 * @brief 参数分割：将字符串按空格分割成 argv
 */
static void split_args(char *line, char *argv[], i32 *argc)
{
    i32 a = 0;
    char *p = line;

    while (*p) {
        /* 跳过空格 */
        while (*p == ' ') *p++ = 0;
        if (*p == 0) break;

        /* 记录参数起点 */
        argv[a++] = p;

        /* 找参数结尾 */
        while (*p && *p != ' ') p++;

        if (a >= SHELL_MAX_ARGC) break;
    }
    *argc = a;
}

/**
 * @brief 树形命令递归分发
 */
static i32 shell_execute_tree(const shell_cmd_t *root, u16 count, i32 argc, char *argv[])
{
    if (argc == 0 || !argv[0]) return -1;

    shell_t *shell = shell_get_current();

    /* 查找第一级命令 */
    for (u16 i = 0; i < count; i++) {
        if (str_is_equal(root[i].name, argv[0])) {
            const shell_cmd_t *cmd = &root[i];

            /* 如果是分支节点（子命令组） */
            if (cmd->is_group) {
                if (argc < 2) {
                    if (shell) {
                        shell_print(shell, "Usage: %s <subcommand> [args...]\r\n", cmd->name);
                    }
                    return -1;
                }
                /* 递归分发到子命令 */
                return shell_execute_tree(cmd->u.sub, cmd->sub_count, argc - 1, argv + 1);
            }

            /* 如果是叶子节点（具体命令） */
            if (cmd->u.handler) {
                return cmd->u.handler(argc, argv);
            }

            return -1;
        }
    }

    /* 命令未找到 */
    if (shell) {
        shell_print(shell, "Command '%s' not found. Type 'help' for available commands.\r\n", argv[0]);
    }
    return -1;
}

i32 shell_run_command_by_name(const char *line)
{
    if (!line) return -1;

    /* 复制到本地缓冲（split_args 会修改内容） */
    static char localbuf[SHELL_LINE_BUFFER];
    strncpy(localbuf, line, sizeof(localbuf) - 1);
    localbuf[sizeof(localbuf) - 1] = 0;

    char *argv[SHELL_MAX_ARGC];
    i32 argc = 0;

    split_args(localbuf, argv, &argc);
    if (argc == 0) return -1;

    shell_t *shell = shell_get_current();

    /* 优先从用户定义命令树中查找 */
    if (shell && shell->cmd_root && shell->cmd_count > 0) {
        i32 ret = shell_execute_tree(shell->cmd_root, shell->cmd_count, argc, argv);
        if (ret != -1) return ret;  /* 命令已执行 */
    }

    /* 其次从内置命令中查找 */
    i32 ret = shell_execute_tree(g_builtins, BUILTIN_COUNT, argc, argv);
    return ret;
}

/**
 * @brief 字符处理主函数
 */
void shell_handler(shell_t *shell, char data)
{
    if (!shell || !shell->buffer) return;

    static u16 idx = 0;

    /* 回车/换行：执行命令 */
    if (data == '\r' || data == '\n') {
        shell->buffer[idx] = 0;
        shell_write_data(shell, "\r\n", 2);
        if (idx > 0) {
            shell_run_command_by_name(shell->buffer);
            idx = 0;
        }
        shell_write_data(shell, "> ", 2);
        return;
    }

    /* 退格处理 */
    if (data == '\b') {
        if (idx > 0) {
            idx--;
            shell_write_data(shell, "\b \b", 3);
        }
        return;
    }

    /* 忽略其他控制字符 */
    if ((unsigned char)data < 0x20) {
        return;
    }

    /* 缓冲输入字符 */
    if (idx < shell->buffer_size - 1) {
        shell->buffer[idx++] = data;
        shell_write_data(shell, &data, 1);
    }
}

/* ========== 参数解析 API ========== */

i32 shell_parse_hex(const char *s, u32 *out)
{
    if (!s || !out) return 0;

    /* 检查 0x 或 0X 前缀 */
    if (!(s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))) {
        return 0;
    }

    char *end = NULL;
    u32 v = (u32)strtoul(s, &end, 0);

    /* 检查是否成功解析了数字部分（end 必须指向字符串末尾或空字符） */
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

i32 shell_parse_short_param(i32 argc, char *argv[], const char *key, char **value)
{
    if (!key || !value || argc < 2) return 0;

    i32 idx = find_argument_index(argc, argv, key);
    if (idx < 0) return 0;

    *value = argv[idx];
    return 1;
}

i32 shell_parse_short_hex_param(i32 argc, char *argv[], const char *key, u32 *value)
{
    if (!key || !value) return 0;

    char *str_val = NULL;
    if (!shell_parse_short_param(argc, argv, key, &str_val)) {
        return 0;
    }

    return shell_parse_hex(str_val, value);
}

void shell_hexdump(shell_t *shell, u32 addr, const u8 *data, u32 len)
{
    if (!shell || !data) return;

    for (u32 i = 0; i < len; i += 16) {
        i32 this_line_len = (len - i) >= 16 ? 16 : (i32)(len - i);
        char line[128];
        u32 off = (u32)snprintf(line, sizeof(line), "%08X: ", (unsigned)(addr + i));

        for (i32 j = 0; j < this_line_len; ++j) {
            off += (u32)snprintf(line + off, sizeof(line) - off, "%02X ", data[i + j]);
        }

        snprintf(line + off, sizeof(line) - off, "\r\n");
        shell_print(shell, "%s", line);
    }
}

/* ========== 单元测试 ========== */
#if TEST_ENABLE

#include "../em_test/test.h"

/* 模拟端口结构 */
typedef struct {
    char output[512];
    u16 output_len;
    i32 write_count;
} mock_port_t;

static mock_port_t g_mock_port = {0};

/* 模拟读端口 */
static i32 mock_read(void *self, void *buf, usize size)
{
    (void)self;
    (void)buf;
    (void)size;
    return 0;
}

/* 模拟写端口 */
static i32 mock_write(void *self, const void *buf, usize size)
{
    mock_port_t *port = (mock_port_t *)self;
    if (!port || !buf) return -1;

    if (port->output_len + size > sizeof(port->output)) {
        return -1;
    }

    memcpy(port->output + port->output_len, buf, size);
    port->output_len += (u16)size;
    port->write_count++;

    return (i32)size;
}

static void mock_port_reset(void)
{
    memset(&g_mock_port, 0, sizeof(g_mock_port));
}

/* 获取最后一次输出内容 */
static const char* mock_get_output(void)
{
    return (const char *)g_mock_port.output;
}

/* ========== 测试辅助命令 ========== */

static i32 test_cmd_leaf(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;
    shell_print(shell, "leaf_cmd: argc=%d\r\n", argc);
    return 0;
}

static i32 test_cmd_sub1(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;
    shell_print(shell, "sub1_executed\r\n");
    return 0;
}

static i32 test_cmd_sub2(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    u32 value = 0;
    if (!shell_parse_short_hex_param(argc, argv, "-x", &value)) {
        shell_print(shell, "no_param\r\n");
        return -1;
    }

    shell_print(shell, "sub2_value=0x%X\r\n", value);
    return 0;
}

/* 测试用的子命令数组 */
static const shell_cmd_t g_test_subcmds[] = {
    {
        .name = "sub1",
        .help = "test subcommand 1",
        .u.handler = test_cmd_sub1,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "sub2",
        .help = "test subcommand 2",
        .u.handler = test_cmd_sub2,
        .sub_count = 0,
        .is_group = false,
    },
};

/* 测试用的顶级命令数组 */
static const shell_cmd_t g_test_commands[] = {
    {
        .name = "leaf",
        .help = "leaf command",
        .u.handler = test_cmd_leaf,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "branch",
        .help = "branch with subcommands",
        .u.sub = g_test_subcmds,
        .sub_count = sizeof(g_test_subcmds) / sizeof(g_test_subcmds[0]),
        .is_group = true,
    },
};

/* ========== 测试用例 ========== */

/* 十六进制解析 - 有效输入 */
TEST_CASE(shell_parse_hex_valid)
{
    u32 value = 0;
    i32 ret = shell_parse_hex("0x1F", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(31, value);

    ret = shell_parse_hex("0xFF00", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0xFF00, value);

    ret = shell_parse_hex("0xDEADBEEF", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0xDEADBEEF, value);
}

/* 十六进制解析 - 无效输入 */
TEST_CASE(shell_parse_hex_invalid)
{
    u32 value = 0;

    /* 缺少 0x 前缀 */
    i32 ret = shell_parse_hex("1F", &value);
    TEST_ASSERT_EQUAL_INT(0, ret);

    /* 无效十六进制字符 */
    ret = shell_parse_hex("0xZZ", &value);
    TEST_ASSERT_EQUAL_INT(0, ret);

    /* NULL 指针 */
    ret = shell_parse_hex(NULL, &value);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ret = shell_parse_hex("0x100", NULL);
    TEST_ASSERT_EQUAL_INT(0, ret);
}

/* 十六进制解析 - 边界值 */
TEST_CASE(shell_parse_hex_boundary)
{
    u32 value = 0;

    /* 零值 */
    i32 ret = shell_parse_hex("0x0", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0, value);

    /* 最大值 */
    ret = shell_parse_hex("0xFFFFFFFF", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, value);

    /* 小写 x */
    ret = shell_parse_hex("0xabcd", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0xABCD, value);

    /* 大写 x */
    ret = shell_parse_hex("0XABCD", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0xABCD, value);
}

/* 短参数解析 - 有效参数 */
TEST_CASE(shell_parse_short_param_found)
{
    char *argv[] = {"cmd", "-r", "0x400", "-v", "100", "-n", "test"};
    i32 argc = 7;

    char *value = NULL;

    i32 ret = shell_parse_short_param(argc, argv, "-r", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_STRING("0x400", value);

    ret = shell_parse_short_param(argc, argv, "-v", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_STRING("100", value);

    ret = shell_parse_short_param(argc, argv, "-n", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_STRING("test", value);
}

/* 短参数解析 - 参数不存在 */
TEST_CASE(shell_parse_short_param_missing)
{
    char *argv[] = {"cmd", "-a", "val1"};
    i32 argc = 3;

    char *value = NULL;

    i32 ret = shell_parse_short_param(argc, argv, "-b", &value);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ret = shell_parse_short_param(argc, argv, "-notfound", &value);
    TEST_ASSERT_EQUAL_INT(0, ret);
}

/* 短参数解析 - 参数值在末尾 */
TEST_CASE(shell_parse_short_param_at_end)
{
    char *argv[] = {"cmd", "-x", "last"};
    i32 argc = 3;

    char *value = NULL;
    i32 ret = shell_parse_short_param(argc, argv, "-x", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_STRING("last", value);
}

/* 十六进制短参数解析 - 有效值 */
TEST_CASE(shell_parse_short_hex_param_valid)
{
    char *argv[] = {"cmd", "-addr", "0xDEAD", "-len", "0x100"};
    i32 argc = 5;

    u32 value = 0;

    i32 ret = shell_parse_short_hex_param(argc, argv, "-addr", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0xDEAD, value);

    ret = shell_parse_short_hex_param(argc, argv, "-len", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_HEX(0x100, value);
}

/* 十六进制短参数解析 - 参数不存在或格式错误 */
TEST_CASE(shell_parse_short_hex_param_invalid)
{
    char *argv[] = {"cmd", "-x", "0x100"};
    i32 argc = 3;

    u32 value = 0;

    i32 ret = shell_parse_short_hex_param(argc, argv, "-missing", &value);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ret = shell_parse_short_hex_param(argc, argv, "-x", &value);
    TEST_ASSERT_EQUAL_INT(1, ret);

    /* 格式错误 */
    char *argv2[] = {"cmd", "-x", "invalid"};
    ret = shell_parse_short_hex_param(3, argv2, "-x", &value);
    TEST_ASSERT_EQUAL_INT(0, ret);
}

/* Shell 初始化 */
TEST_CASE(shell_init_basic)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    shell_t *current = shell_get_current();
    TEST_ASSERT_NOT_NULL(current);
}

/* 格式化输出 */
TEST_CASE(shell_print_output)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_print(&shell, "Test %d %s\r\n", 42, "hello");
    TEST_ASSERT_TRUE(ret > 0);
    TEST_ASSERT_TRUE(g_mock_port.output_len > 0);
}

/* 十六进制转储 */
TEST_CASE(shell_hexdump_basic)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    u8 data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    shell_hexdump(&shell, 0x1000, data, sizeof(data));

    TEST_ASSERT_TRUE(g_mock_port.output_len > 0);
    const char *output = mock_get_output();
    TEST_ASSERT_NOT_NULL(strstr(output, "1000"));
    TEST_ASSERT_NOT_NULL(strstr(output, "DE"));
}

/* 十六进制转储 - 16 字节对齐 */
TEST_CASE(shell_hexdump_16bytes)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    u8 data[32];
    for (u8 i = 0; i < 32; i++) {
        data[i] = i;
    }

    shell_hexdump(&shell, 0x0, data, 32);
    TEST_ASSERT_TRUE(g_mock_port.output_len > 0);
}

/* 叶子命令执行 */
TEST_CASE(shell_execute_leaf_command)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("leaf");
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_TRUE(g_mock_port.output_len > 0);
}

/* 分层命令执行 - 第一级 */
TEST_CASE(shell_execute_branch_command_sub1)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("branch sub1");
    TEST_ASSERT_EQUAL_INT(0, ret);
    const char *output = mock_get_output();
    TEST_ASSERT_NOT_NULL(strstr(output, "sub1_executed"));
}

/* 分层命令执行 - 第二级（带参数） */
TEST_CASE(shell_execute_branch_command_sub2_with_param)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("branch sub2 -x 0xABCD");
    TEST_ASSERT_EQUAL_INT(0, ret);
    const char *output = mock_get_output();
    TEST_ASSERT_NOT_NULL(strstr(output, "sub2_value"));
}

/* 分层命令执行 - 参数缺失 */
TEST_CASE(shell_execute_branch_command_missing_param)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("branch sub2");
    TEST_ASSERT_EQUAL_INT(-1, ret);
}

/* 命令未找到 */
TEST_CASE(shell_execute_command_not_found)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("unknown_cmd");
    TEST_ASSERT_EQUAL_INT(-1, ret);
    const char *output = mock_get_output();
    TEST_ASSERT_NOT_NULL(strstr(output, "not found"));
}

/* 内置 help 命令 */
TEST_CASE(shell_builtin_help)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("help");
    TEST_ASSERT_EQUAL_INT(0, ret);
    const char *output = mock_get_output();
    TEST_ASSERT_NOT_NULL(strstr(output, "Available Commands"));
}

/* 内置 clear 命令 */
TEST_CASE(shell_builtin_clear)
{
    shell_test_reset_global_state();
    mock_port_reset();
    shell_port_t port = {
        .read_bytes = mock_read,
        .write_bytes = mock_write,
        .priv = &g_mock_port,
    };

    shell_t shell;
    char buffer[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("clear");
    TEST_ASSERT_EQUAL_INT(0, ret);
    const char *output = mock_get_output();
    /* clear 命令发送 ANSI 转义序列 */
    TEST_ASSERT_NOT_NULL(strstr(output, "\033[2J"));
}

#endif
