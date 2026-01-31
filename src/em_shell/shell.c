#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../em_base/string_util.h"

// 全局当前 shell 对象（只允许一个）
static shell_t *g_shell_current = NULL;
// 命令表与计数
static shell_cmd_t g_shell_cmds[SHELL_MAX_CMD];
static i32 g_shell_cmd_count = 0;

// 前置声明：内置命令
static i32 shell_help(i32 argc, char *argv[]);
static i32 shell_clear(i32 argc, char *argv[]);

// 初始化 shell 并注册内置命令
void shell_init(shell_t *shell, char *buffer, u16 size)
{
    shell->buffer = buffer;
    shell->buffer_size = size;
    if (g_shell_current == NULL) g_shell_current = shell;

    /* 注册内置命令 */
    shell_register_command("help", shell_help, "list all commands");
    shell_register_command("clear", shell_clear, "clear the screen");
}

// 列出已注册的命令及说明
static i32 shell_help(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;
    for (i32 i = 0; i < g_shell_cmd_count; ++i) {
        if (g_shell_cmds[i].desc)
            shell_print(shell, "%-12s - %s\r\n", g_shell_cmds[i].name, g_shell_cmds[i].desc);
        else
            shell_print(shell, "%-12s\r\n", g_shell_cmds[i].name);
    }
    return 0;
}

// 清屏命令
static i32 shell_clear(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;
    // 使用 ANSI 转义序列：清屏并移动游标到首行首列
    shell_print(shell, "\033[2J\033[H");
    return 0;
}

shell_t* shell_get_current(void)
{
    return g_shell_current;
}

i32 shell_register_command(const char *name, i32 (*func)(i32, char **), const char *desc)
{
    if (g_shell_cmd_count >= SHELL_MAX_CMD) return -1;
    g_shell_cmds[g_shell_cmd_count].name = name;
    g_shell_cmds[g_shell_cmd_count].func = func;
    g_shell_cmds[g_shell_cmd_count].desc = desc;
    g_shell_cmd_count++;
    return 0;
}

i32 shell_print(shell_t *shell, const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    i32 n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0 && shell && shell->write) {
        shell->write(tmp, (u16)n);
    }
    return n;
}

static void split_args(char *line, char *argv[], i32 *argc)
{
    i32 a = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ') *p++ = 0;
        if (*p == 0) break;
        argv[a++] = p;
        while (*p && *p != ' ') p++;
        if (a >= 32) break;
    }
    *argc = a;
}

i32 shell_run_command_by_name(const char *line)
{
    static char localbuf[SHELL_LINE_BUFFER];
    strncpy(localbuf, line, sizeof(localbuf)-1);
    localbuf[sizeof(localbuf)-1] = 0;
    char *argv[32];
    i32 argc = 0;
    split_args(localbuf, argv, &argc);
    if (argc == 0) return -1;
    for (i32 i = 0; i < g_shell_cmd_count; ++i) {
        if (str_is_equal(g_shell_cmds[i].name, argv[0])) {
            return g_shell_cmds[i].func(argc, argv);
        }
    }
    if (g_shell_current && g_shell_current->write) {
        g_shell_current->write("Command not Found\r\n", 18);
    }
    return -1;
}

void shell_handler(shell_t *shell, char data)
{
    if (!shell || !shell->buffer) return;
    static u16 idx = 0;
    if (data == '\r' || data == '\n') {
        /* 执行当前行 */
        shell->buffer[idx] = 0;
        if (shell->write) shell->write("\r\n", 2);
        if (idx > 0) {
            shell_run_command_by_name(shell->buffer);
            idx = 0;
        }
        if (shell->write) shell->write("> ", 2);
        return;
    }
    if (data == '\b') {
        if (idx > 0) {
            idx--;
            if (shell->write) shell->write("\b \b", 3);
        }
        return;
    }
    if ((unsigned char)data < 0x20) return; /* 忽略控制字符（上面已处理退格/回车） */
    if (idx < shell->buffer_size - 1) {
        shell->buffer[idx++] = data;
        if (shell->write) shell->write(&data, 1);
    }
}

i32 shell_parse_hex(const char *s, u32 *out)
{
    if (!s || !out) return 0;
    if (!(s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))) return 0;
    char *end = NULL;
    u32 v = (u32)strtoul(s, &end, 0);
    if (end == s) return 0;
    *out = v;
    return 1;
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

i32 shell_dispatch(shell_t *shell, const shell_sub_cmd_t *cmds, i32 cmd_count, i32 argc, char *argv[])
{
    if (argc < 2) return -1;
    for (i32 i = 0; i < cmd_count; i++) {
        if (str_is_equal(argv[1], cmds[i].name)) {
            return cmds[i].handler(argc, argv);
        }
    }
    return -2; // Unknown subcommand
}
