/**
 * @file file_transfer.h
 * @author canrad (1517807724@qq.com)
 * @brief 定义文件传输协议相关的接口
 * @version 0.1
 * @date 2025-12-27
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
#define LIBCA_EM_PROTOCOL_FILE_TRANSFER_H

#include "../em_base/datatype.h"
#include "transport.h"

// 文件传输协议的回调集
typedef struct
{
    /**
     * @brief 数据接收回调
     * @param offset 当前文件偏移量
     * @param data   解包后的纯净数据 (不需要关心 SOH/SEQ/CRC)
     * @param len    数据长度
     * @return 0 成功, 非 0 失败(比如 Flash 写错了)
     */
    i32 (*on_data)(u32 offset, const u8* data, usize len);

    /**
     * @brief 传输开始回调
     * @param total_size 文件总大小 (如果是 0 表示未知)
     * @param filename   文件名
     */
    void (*on_start)(u32 total_size, const char* filename);

    /**
     * @brief 传输结束回调 (成功或失败)
     * @param status 状态码
     */
    void (*on_finish)(i32 status);
} file_transfer_cbs_t;

// 文件传输协议接口
typedef struct file_transfer_ops
{
    /**
     * @brief 初始化协议对象
     * @param self 协议实例 (如 xmodem_t)
     * @param io 底层传输通道
     * @param cbs 上层业务回调
     */
    i32 (*init)(void *self, transport_t *io, const file_transfer_cbs_t *cbs);

    /**
     * @brief 核心处理：驱动定时器
     *
     * 用于处理协议内部的超时逻辑（如 XModem 等待起始信号超时、重传超时等）。
     * 调用者应以固定频率（如 10ms 或 100ms）调用此接口。
     *
     * @param self 协议对象实例指针
     * @param ms_delta 距离上次调用过去的时间（毫秒）
     * @return i32 错误码
     */
    i32 (*tick)(void* self, u32 ms_delta);

    /**
     * @brief 核心处理：输入字节流
     *
     * 调用者将从底层（如串口）接收到的原始数据喂入此接口。
     * 协议内部会进行状态机解析、拆包、校验。
     *
     * @param self 协议对象实例指针 (如 xmodem_t*)
     * @param in_buf 接收到的数据缓冲区
     * @param in_len 数据长度
     * @return i32 错误码
     */
    i32 (*process)(void* self, const u8* in_buf, usize in_len);

    /**
     * @brief 请求开始接收 (发送 'C' 或者握手信号) 
     * 因为接受一般来说需要给发送端发送一个开始信号
     *
     * @param self 协议对象实例指针 (如 xmodem_t*)
     */
    void (*start_recv)(void *self);

    /**
     * @brief 获取当前进度百分比
     *
     * @param self 协议对象实例指针 (如 xmodem_t*)
     */
    i32 (*get_progress)(void *self);
} file_transfer_ops_t;

typedef enum
{
    TP_XMODEM,
    TP_YMODEM,
    TP_ZMODEM
} transfer_protocol_enum;

typedef struct file_transfer
{
    // 传输协议类型
    transfer_protocol_enum proto;
    // 指向实际协议操作接口对象的指针
    file_transfer_ops_t* ops;
    // 指向实际协议对象实例的指针（如 xmodem_t*）
    void* proto_ins;
} file_transfer_t;

void file_transfer_init(file_transfer_t* owner, transfer_protocol_enum proto, void* proto_ins);


#endif   // !LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
