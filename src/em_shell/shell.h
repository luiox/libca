#ifndef LIBCA_EM_SHELL_SHELL_H
#define LIBCA_EM_SHELL_SHELL_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置宏 ========== */
#define SHELL_MAX_CMD       32      /* 最大命令数（叶子节点） */
#define SHELL_LINE_BUFFER   256     /* 输入行缓冲大小 */
#define SHELL_MAX_ARGC      32      /* 最大参数个数 */
#define SHELL_PARSE_BUF_LEN 256     /* 参数解析临时缓冲 */

/* ========== 端口接口 ========== */

/**
 * @brief Shell 端口接口定义
 * @details 所有 I/O 操作均通过此接口进行，实现 shell 与具体通信端口的分离
 */
typedef struct shell_port_t {
    /**
     * @brief 从端口读取数据
     * @param[in] self 端口对象指针
     * @param[out] buf 接收缓冲区
     * @param[in] size 待读取字节数
     * @return 成功读取的字节数（>0），或错误码（<0）
     */
    i32 (*read_bytes)(void *self, void *buf, usize size);

    /**
     * @brief 向端口写入数据
     * @param[in] self 端口对象指针
     * @param[in] buf 待发送数据
     * @param[in] size 待发送字节数
     * @return 成功写入的字节数（>0），或错误码（<0）
     */
    i32 (*write_bytes)(void *self, const void *buf, usize size);

    void *priv;  /* 端口私有数据（如串口句柄） */
} shell_port_t;

/* ========== 命令树结构 ========== */

/**
 * @brief Shell 命令函数指针原型
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 命令执行返回值（通常 0 为成功，负数为错误码）
 */
typedef i32 (*shell_cmd_fn_t)(i32 argc, char *argv[]);

/**
 * @brief em_shell 命令树节点结构体
 * @details 支持分层命令结构（树形），节点可为叶子（具体命令）或分支（子命令组）
 */
typedef struct shell_cmd_t {
    const char *name;       /**< 命令名称（如 "mem"、"read"） */
    const char *help;       /**< 帮助信息（如 "Memory operations"） */

    /**
     * @brief 命令的执行体，采用 union 节省内存并实现多态
     */
    union {
        shell_cmd_fn_t handler;         /**< 叶子节点：指向具体执行的函数 */
        const struct shell_cmd_t *sub;  /**< 分支节点：指向子命令数组 */
    } u;

    /**
     * @brief 子命令计数（仅对分支节点有效）
     * 对于叶子节点，此值应为 0
     */
    u16 sub_count;

    /**
     * @brief 标记位：true 表示此节点是子命令集合（分支），false 表示是具体命令（叶子）
     */
    bool is_group;

} shell_cmd_t;

/* ========== Shell 对象 ========== */

/**
 * @brief Shell 对象类型
 */
typedef struct shell_t {
    shell_port_t *port;         /**< I/O 端口接口 */
    char *buffer;               /**< 用户提供的行缓冲 */
    u16 buffer_size;            /**< 缓冲区大小 */
    const shell_cmd_t *cmd_root;/**< 命令树根节点 */
    u16 cmd_count;              /**< 命令树中的总命令数 */
} shell_t;

/* ========== 初始化 API ========== */

/**
 * @brief 初始化 shell 对象
 * @param[in] shell shell 对象指针
 * @param[in] port I/O 端口接口
 * @param[in] buffer 行缓冲区地址
 * @param[in] size 缓冲区大小
 * @param[in] cmd_root 命令树根节点（通常是静态注册的命令数组）
 * @param[in] cmd_count 根节点命令总数
 */
void shell_init(shell_t *shell, shell_port_t *port, char *buffer, u16 size,
                const shell_cmd_t *cmd_root, u16 cmd_count);

/**
 * @brief 获取当前全局 shell 对象
 * @return 全局 shell 指针，若未初始化则返回 NULL
 */
shell_t* shell_get_current(void);

/* ========== 命令执行 API ========== */

/**
 * @brief 处理一个输入字符
 * @param[in] shell shell 对象指针
 * @param[in] data 输入字符
 * @details 自动处理回车/换行、退格等编辑字符，缓冲输入直至回车后执行
 */
void shell_handler(shell_t *shell, char data);

/**
 * @brief 通过命令行文本直接执行命令
 * @param[in] line 命令行字符串（不应包含 \r \n）
 * @return 命令执行的返回值
 * @note 此函数会修改 line 内容，调用前建议复制原值
 */
i32 shell_run_command_by_name(const char *line);

/* ========== 输出 API ========== */

/**
 * @brief 格式化输出到 shell
 * @param[in] shell shell 对象指针
 * @param[in] fmt 格式化字符串
 * @return 输出的字符数
 */
i32 shell_print(shell_t *shell, const char *fmt, ...);

/**
 * @brief 十六进制转储输出
 * @param[in] shell shell 对象指针
 * @param[in] addr 起始地址（用于标签显示）
 * @param[in] data 待转储数据指针
 * @param[in] len 转储长度（字节数）
 */
void shell_hexdump(shell_t *shell, u32 addr, const u8 *data, u32 len);

/* ========== 参数解析 API ========== */

/**
 * @brief 解析十六进制字符串（0x 前缀格式）
 * @param[in] s 输入字符串
 * @param[out] out 输出值
 * @return 1 解析成功，0 解析失败
 * @details 示例：shell_parse_hex("0x1F", &value) → value=31, return=1
 */
i32 shell_parse_hex(const char *s, u32 *out);

/**
 * @brief 从参数列表中查找指定的短参数并返回其值
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @param[in] key 要查找的参数键（如 "-r"、"-v"）
 * @param[out] value 参数值（若找到则指向 argv 中的字符串）
 * @return 1 找到，0 未找到或格式错误
 * @details 示例：在 argv=["-r", "0x400", "-v", "10"] 中
 *          shell_parse_short_param(4, argv, "-r", &val) → val="0x400", return=1
 */
i32 shell_parse_short_param(i32 argc, char *argv[], const char *key, char **value);

/**
 * @brief 从参数列表中查找并解析十六进制短参数
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @param[in] key 要查找的参数键
 * @param[out] value 解析出的十六进制值
 * @return 1 成功，0 失败
 * @details 示例：shell_parse_short_hex_param(4, argv, "-r", &val)
 *          若 argv 包含 ["-r", "0x400"]，则 val=1024, return=1
 */
i32 shell_parse_short_hex_param(i32 argc, char *argv[], const char *key, u32 *value);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_SHELL_SHELL_H
