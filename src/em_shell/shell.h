#ifndef LIBCA_EM_SHELL_SHELL_H
#define LIBCA_EM_SHELL_SHELL_H

#include "../em_base/datatype.h"

#define SIMPLE_SHELL_MAX_CMD      32
#define SIMPLE_SHELL_LINE_BUFFER  256

// 命令结构体，外部可注册命令
typedef struct simple_shell_cmd_t {
    const char *name;
    i32 (*func)(i32 argc, char *argv[]);
    const char *desc;
} simple_shell_cmd_t;

// shell 对象类型
typedef struct simple_shell_t {
    i16 (*read)(char *data, u16 len);
    i16 (*write)(char *data, u16 len);

    char *buffer; /* 用户提供的行缓冲 */
    u16 buffer_size;
} simple_shell_t;

// 初始化并注册内置命令
void simple_shell_init(simple_shell_t *shell, char *buffer, u16 size);
// 获取当前全局 shell
simple_shell_t* simple_shell_get_current(void);

// 格式化输出到 shell
i32 simple_shell_print(simple_shell_t *shell, const char *fmt, ...);
// 处理一个输入字符
void simple_shell_handler(simple_shell_t *shell, char data);

// 注册命令
i32 simple_shell_register_command(const char *name, i32 (*func)(i32, char **), const char *desc);
// 通过行文本执行命令
i32 simple_shell_run_command_by_name(const char *line);

/* --- 新增通用工具函数 --- */

// 解析 0x 开头的十六进制字符串
i32 simple_shell_parse_hex(const char *s, u32 *out);

// 十六进制格式化转储输出到 shell
void simple_shell_hexdump(simple_shell_t *shell, u32 addr, const u8 *data, u32 len);

// 子命令分发结构体
typedef struct simple_shell_sub_cmd_t {
    const char *name;
    i32 (*handler)(i32 argc, char *argv[]);
} simple_shell_sub_cmd_t;

// 子命令分发（通常用于一级命令处理函数中）
i32 simple_shell_dispatch(simple_shell_t *shell, const simple_shell_sub_cmd_t *cmds, i32 cmd_count, i32 argc, char *argv[]);

#endif // LIBCA_EM_SHELL_SHELL_H
