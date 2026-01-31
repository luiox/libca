/**
 * @file example.c
 * @brief em_shell 使用示例
 * 
 * 演示：
 * 1. 定义 shell_port_t 接口实现
 * 2. 构建树形命令结构（包含分支和叶子节点）
 * 3. 短参数解析（-key value 格式）
 */

#include "shell.h"
#include <stdio.h>
#include <string.h>

/* ========== 示例 1：实现串口 I/O 端口 ========== */

/**
 * @brief 模拟的串口端口结构
 */
typedef struct {
    char rx_buffer[256];
    char tx_buffer[256];
    u16 rx_idx;
    u16 tx_idx;
} uart_port_t;

static uart_port_t g_uart_port = {0};

/**
 * @brief UART 读取函数
 */
static i32 uart_read(void *self, void *buf, usize size)
{
    uart_port_t *uart = (uart_port_t *)self;
    if (!uart || !buf || size == 0) return -1;

    /* 实际应用中应从硬件读取 */
    u16 available = uart->rx_idx;
    if (available == 0) return 0;  /* 无数据可读 */

    u16 to_read = (available > size) ? size : available;
    memcpy(buf, uart->rx_buffer, to_read);
    uart->rx_idx -= to_read;
    memmove(uart->rx_buffer, uart->rx_buffer + to_read, uart->rx_idx);

    return to_read;
}

/**
 * @brief UART 写入函数
 */
static i32 uart_write(void *self, const void *buf, usize size)
{
    uart_port_t *uart = (uart_port_t *)self;
    if (!uart || !buf || size == 0) return -1;

    u16 available = sizeof(uart->tx_buffer) - uart->tx_idx;
    if (available < size) return -1;  /* 缓冲区满 */

    memcpy(uart->tx_buffer + uart->tx_idx, buf, size);
    uart->tx_idx += size;

    /* 实际应用中应发送到硬件 */
    printf("%.*s", (int)size, (const char *)buf);
    fflush(stdout);

    return (i32)size;
}

/* ========== 示例 2：定义自定义命令处理函数 ========== */

/**
 * @brief mem read 子命令 - 读内存
 */
static i32 cmd_mem_read(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    u32 addr = 0;
    if (!shell_parse_short_hex_param(argc, argv, "-a", &addr)) {
        shell_print(shell, "Usage: mem read -a <hex_addr>\r\n");
        return -1;
    }

    shell_print(shell, "Reading from address 0x%X\r\n", addr);
    /* 实际代码：从addr读取数据 */
    u8 data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    shell_hexdump(shell, addr, data, sizeof(data));

    return 0;
}

/**
 * @brief mem write 子命令 - 写内存
 */
static i32 cmd_mem_write(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    u32 addr = 0;
    char *value_str = NULL;

    if (!shell_parse_short_hex_param(argc, argv, "-a", &addr) ||
        !shell_parse_short_param(argc, argv, "-v", &value_str)) {
        shell_print(shell, "Usage: mem write -a <hex_addr> -v <value>\r\n");
        return -1;
    }

    shell_print(shell, "Writing to address 0x%X, value: %s\r\n", addr, value_str);
    /* 实际代码：向addr写入数据 */
    return 0;
}

/**
 * @brief mem dump 子命令 - 转储内存
 */
static i32 cmd_mem_dump(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    u32 addr = 0;
    u32 len = 16;

    shell_parse_short_hex_param(argc, argv, "-a", &addr);
    shell_parse_short_hex_param(argc, argv, "-l", &len);

    shell_print(shell, "Dumping 0x%X bytes from 0x%X\r\n", len, addr);
    
    /* 模拟转储数据 */
    u8 sample_data[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    };
    u32 dump_len = (len > sizeof(sample_data)) ? sizeof(sample_data) : len;
    shell_hexdump(shell, addr, sample_data, dump_len);

    return 0;
}

/**
 * @brief ota start 子命令 - 启动 OTA
 */
static i32 cmd_ota_start(i32 argc, char *argv[])
{
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    char *url = NULL;
    if (!shell_parse_short_param(argc, argv, "-u", &url)) {
        shell_print(shell, "Usage: ota start -u <url>\r\n");
        return -1;
    }

    shell_print(shell, "Starting OTA from URL: %s\r\n", url);
    /* 实际代码：启动 OTA 流程 */
    return 0;
}

/**
 * @brief ota status 子命令 - OTA 状态
 */
static i32 cmd_ota_status(i32 argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    shell_print(shell, "OTA Status: Idle\r\n");
    /* 实际代码：查询 OTA 状态 */
    return 0;
}

/**
 * @brief reboot 命令 - 重启系统
 */
static i32 cmd_reboot(i32 argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_t *shell = shell_get_current();
    if (!shell) return -1;

    shell_print(shell, "System rebooting...\r\n");
    /* 实际代码：系统重启 */
    return 0;
}

/* ========== 示例 3：构建分层命令树 ========== */

/**
 * @brief mem 命令的子命令数组
 */
static const shell_cmd_t g_mem_subcmds[] = {
    {
        .name = "read",
        .help = "read memory at address (-a <hex>)",
        .u.handler = cmd_mem_read,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "write",
        .help = "write memory at address (-a <hex> -v <value>)",
        .u.handler = cmd_mem_write,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "dump",
        .help = "dump memory region (-a <hex> -l <len>)",
        .u.handler = cmd_mem_dump,
        .sub_count = 0,
        .is_group = false,
    },
};

/**
 * @brief ota 命令的子命令数组
 */
static const shell_cmd_t g_ota_subcmds[] = {
    {
        .name = "start",
        .help = "start OTA upgrade (-u <url>)",
        .u.handler = cmd_ota_start,
        .sub_count = 0,
        .is_group = false,
    },
    {
        .name = "status",
        .help = "show OTA status",
        .u.handler = cmd_ota_status,
        .sub_count = 0,
        .is_group = false,
    },
};

/**
 * @brief 顶级命令数组
 */
static const shell_cmd_t g_commands[] = {
    {
        .name = "mem",
        .help = "memory operations (read/write/dump)",
        .u.sub = g_mem_subcmds,
        .sub_count = sizeof(g_mem_subcmds) / sizeof(g_mem_subcmds[0]),
        .is_group = true,
    },
    {
        .name = "ota",
        .help = "OTA upgrade operations (start/status)",
        .u.sub = g_ota_subcmds,
        .sub_count = sizeof(g_ota_subcmds) / sizeof(g_ota_subcmds[0]),
        .is_group = true,
    },
    {
        .name = "reboot",
        .help = "reboot system",
        .u.handler = cmd_reboot,
        .sub_count = 0,
        .is_group = false,
    },
};

/* ========== 使用示例 ========== */

#ifdef EXAMPLE_MAIN

int main(void)
{
    /* 初始化 UART 端口 */
    shell_port_t port = {
        .read_bytes = uart_read,
        .write_bytes = uart_write,
        .priv = &g_uart_port,
    };

    /* 创建 shell 对象 */
    shell_t shell;
    char shell_buffer[SHELL_LINE_BUFFER];

    shell_init(&shell, &port, shell_buffer, sizeof(shell_buffer),
               g_commands, sizeof(g_commands) / sizeof(g_commands[0]));

    printf("\n=== em_shell Example ===\n");
    printf("Type 'help' to show all commands\n\n");

    /* 模拟命令行交互 */
    const char *test_commands[] = {
        "help",
        "mem read -a 0x1000",
        "mem write -a 0x2000 -v 0xFF",
        "mem dump -a 0x3000 -l 0x20",
        "ota start -u http://example.com/firmware.bin",
        "reboot",
    };

    for (usize i = 0; i < sizeof(test_commands) / sizeof(test_commands[0]); i++) {
        printf("> %s\n", test_commands[i]);
        shell_run_command_by_name(test_commands[i]);
        printf("\n");
    }

    return 0;
}

#endif
