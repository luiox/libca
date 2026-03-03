/**
 * @file partition.h
 * @author canrad (1517807724@qq.com)
 * @brief Flash 分区管理器，支持分区查找、读写擦除和流式写入
 * @version 1.0
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_OTA_PARTITION_H
#define LIBCA_EM_OTA_PARTITION_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 错误码定义
 * ============================================================================ */

#define PARTITION_OK                (0)     /* 成功 */
#define PARTITION_ERR_NOT_FOUND     (-1)    /* 分区未找到 */
#define PARTITION_ERR_INVALID_PARAM (-2)    /* 无效参数 */
#define PARTITION_ERR_OUT_OF_RANGE  (-3)    /* 超出分区范围 */
#define PARTITION_ERR_READONLY      (-4)    /* 分区只读 */
#define PARTITION_ERR_READ_FAIL     (-5)    /* 读取失败 */
#define PARTITION_ERR_WRITE_FAIL    (-6)    /* 写入失败 */
#define PARTITION_ERR_ERASE_FAIL    (-7)    /* 擦除失败 */
#define PARTITION_ERR_PORT_NOT_SET  (-8)    /* 底层端口未注册 */
#define PARTITION_ERR_SIZE_MISMATCH (-9)    /* 流式写入大小不匹配 */
#define PARTITION_ERR_NOT_OPEN      (-10)   /* 流未打开 */
#define PARTITION_ERR_NOT_READABLE  (-11)   /* 分区不可读 */

/* ============================================================================
 * 分区属性标志
 * ============================================================================ */

#define PARTITION_FLAG_READABLE     (1 << 0)    /* 可读 */
#define PARTITION_FLAG_WRITABLE     (1 << 1)    /* 可写 */
#define PARTITION_FLAG_ERASEABLE    (1 << 2)    /* 可擦除 */
#define PARTITION_FLAG_READONLY     (PARTITION_FLAG_READABLE)  /* 只读分区 */

/* ============================================================================
 * Port 定义（底层驱动适配层）
 * ============================================================================ */

typedef struct partition_port {
    /**
     * @brief 从指定地址读取数据
     * @param addr  起始地址（绝对地址）
     * @param buf   接收缓冲区
     * @param len   读取长度（字节）
     * @return 0 成功，负值 失败
     */
    i32 (*read)(u32 addr, u8 *buf, u32 len);

    /**
     * @brief 向指定地址写入数据
     * @param addr  起始地址
     * @param data  待写入数据
     * @param len   写入长度（字节）
     * @return 0 成功，负值 失败
     */
    i32 (*write)(u32 addr, const u8 *data, u32 len);

    /**
     * @brief 擦除指定区域的Flash
     * @param addr  起始地址
     * @param len   擦除长度（字节）
     * @return 0 成功，负值 失败
     */
    i32 (*erase)(u32 addr, u32 len);
} partition_port_t;

/**
 * @brief 注册底层端口
 * @param port 端口操作表
 */
void partition_register_port(const partition_port_t *port);

/**
 * @brief 检查端口是否已注册
 * @return true 已注册，false 未注册
 */
bool partition_port_is_registered(void);

/* ============================================================================
 * 分区定义
 * ============================================================================ */

typedef struct partition {
    const char *name;       /**< 分区名称，用于查找 */
    u32 start;              /**< 起始地址（绝对地址） */
    u32 size;               /**< 大小（字节） */
    u8 flags;               /**< 属性标志 */
} partition_t;

/* ============================================================================
 * 分区查找与基础操作
 * ============================================================================ */

/**
 * @brief 根据名称查找分区
 * @param table 分区表数组
 * @param count 分区表元素个数
 * @param name 分区名称
 * @return 找到返回分区指针，未找到返回 NULL
 */
const partition_t* partition_find(const partition_t *table, usize count, const char *name);

/**
 * @brief 从分区读取数据
 * @param part 分区指针
 * @param offset 分区内偏移量
 * @param buf 接收缓冲区
 * @param len 读取长度
 * @return PARTITION_OK 成功，其他值失败
 */
i32 partition_read(const partition_t *part, u32 offset, u8 *buf, u32 len);

/**
 * @brief 向分区写入数据
 * @param part 分区指针
 * @param offset 分区内偏移量
 * @param data 待写入数据
 * @param len 写入长度
 * @return PARTITION_OK 成功，其他值失败
 */
i32 partition_write(const partition_t *part, u32 offset, const u8 *data, u32 len);

/**
 * @brief 擦除整个分区
 * @param part 分区指针
 * @return PARTITION_OK 成功，其他值失败
 */
i32 partition_erase(const partition_t *part);

/**
 * @brief 擦除分区内指定区域
 * @param part 分区指针
 * @param offset 分区内偏移量
 * @param len 擦除长度
 * @return PARTITION_OK 成功，其他值失败
 */
i32 partition_erase_range(const partition_t *part, u32 offset, u32 len);

/* ============================================================================
 * 流式写入（适合 OTA 升级场景）
 * ============================================================================ */

/**
 * @brief 流式写入回调函数类型
 * @param offset 当前写入偏移量（相对于分区起始）
 * @param data 已写入的数据
 * @param len 数据长度
 * @param userdata 用户数据
 */
typedef void (*partition_stream_callback_t)(u32 offset, const u8 *data, u32 len, void *userdata);

typedef struct partition_stream {
    const partition_t *part;            /**< 目标分区 */
    u32 current_offset;                 /**< 当前写入偏移 */
    u32 total_size;                     /**< 预期总大小 */
    u32 written;                        /**< 已写入字节数 */
    partition_stream_callback_t on_block_written;  /**< 块写入完成回调 */
    void *userdata;                     /**< 回调用户数据 */
} partition_stream_t;

/**
 * @brief 打开分区流
 * @param stream 流对象（由用户分配）
 * @param part 目标分区
 * @param total_size 预期写入总大小（用于最终校验）
 * @return PARTITION_OK 成功，其他值失败
 */
i32 partition_stream_open(partition_stream_t *stream, const partition_t *part, u32 total_size);

/**
 * @brief 向流中写入数据块
 * @param stream 流对象
 * @param data 数据指针
 * @param len 数据长度
 * @return PARTITION_OK 成功，其他值失败
 */
i32 partition_stream_write(partition_stream_t *stream, const u8 *data, u32 len);

/**
 * @brief 关闭分区流
 * @param stream 流对象
 * @return PARTITION_OK 成功，其他值失败（如大小不匹配）
 */
i32 partition_stream_close(partition_stream_t *stream);

/**
 * @brief 获取流当前写入位置
 * @param stream 流对象
 * @return 当前偏移量
 */
u32 partition_stream_offset(const partition_stream_t *stream);

/**
 * @brief 获取流剩余可写入空间
 * @param stream 流对象
 * @return 剩余字节数
 */
u32 partition_stream_remaining(const partition_stream_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* LIBCA_EM_OTA_PARTITION_H */
