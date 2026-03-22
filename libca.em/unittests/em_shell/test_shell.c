/* Auto-migrated from src/em_shell/shell.c test blocks */
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <em_base/string_util.h>
#include <em_base/debug.h>

/* Keep internal shell state visible for migrated inline tests. */
#include "../../src/em_shell/shell.c"

static void shell_test_reset_global_state(void)
{
    g_shell_current = NULL;
}

#include <em_test/test.h>

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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("leaf", work_buf, sizeof(work_buf));
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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("branch sub1", work_buf, sizeof(work_buf));
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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("branch sub2 -x 0xABCD", work_buf, sizeof(work_buf));
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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("branch sub2", work_buf, sizeof(work_buf));
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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("unknown_cmd", work_buf, sizeof(work_buf));
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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("help", work_buf, sizeof(work_buf));
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
    char work_buf[256];

    shell_init(&shell, &port, buffer, sizeof(buffer),
               g_test_commands, sizeof(g_test_commands) / sizeof(g_test_commands[0]));

    i32 ret = shell_run_command_by_name("clear", work_buf, sizeof(work_buf));
    TEST_ASSERT_EQUAL_INT(0, ret);
    const char *output = mock_get_output();
    /* clear 命令发送 ANSI 转义序列 */
    TEST_ASSERT_NOT_NULL(strstr(output, "\033[2J"));
}

