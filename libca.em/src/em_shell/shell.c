#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <em_base/string_util.h>
#include <em_base/debug.h>

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

/* ========== 工具函数 ========== */

/// @brief 向 shell 端口写入数据
static i32 shell_write_data(shell_t *shell, const void *buf, usize len)
{
    if (!shell || !shell->port || !shell->port->write_bytes) {
        return -1;
    }
    return shell->port->write_bytes(shell->port->priv, (void *)buf, len);
}

/// @brief 从参数列表中解析参数，支持格式："-key" "value"
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

/// @brief help 命令实现
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

/// @brief clear 命令实现
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
    shell->input_idx = 0;  /* 初始化输入索引 */

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

/// @brief 参数分割：将字符串按空格分割成 argv
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

/// @brief 树形命令递归分发
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

i32 shell_run_command_by_name(const char *line, char *buf, u16 buf_size)
{
    if (!line || !buf || buf_size == 0) return -1;

    /* 复制到工作缓冲区（split_args 会修改内容） */
    strncpy(buf, line, buf_size - 1);
    buf[buf_size - 1] = 0;

    char *argv[SHELL_MAX_ARGC];
    i32 argc = 0;

    split_args(buf, argv, &argc);
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

/// @brief 字符处理主函数
void shell_handler(shell_t *shell, char data)
{
    if (!shell || !shell->buffer) return;

    /* 回车/换行：执行命令 */
    if (data == '\r' || data == '\n') {
        shell->buffer[shell->input_idx] = 0;
        shell_write_data(shell, "\r\n", 2);
        if (shell->input_idx > 0) {
            /* 使用 shell 对象自己的缓冲作为工作缓冲 */
            shell_run_command_by_name(shell->buffer, shell->buffer, shell->buffer_size);
            shell->input_idx = 0;
        }
        shell_write_data(shell, "> ", 2);
        return;
    }

    /* 退格处理 */
    if (data == '\b') {
        if (shell->input_idx > 0) {
            shell->input_idx--;
            shell_write_data(shell, "\b \b", 3);
        }
        return;
    }

    /* 忽略其他控制字符 */
    if ((unsigned char)data < 0x20) {
        return;
    }

    /* 缓冲输入字符 */
    if (shell->input_idx < shell->buffer_size - 1) {
        shell->buffer[shell->input_idx++] = data;
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
