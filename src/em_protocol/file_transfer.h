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

typedef void* cbs_t;

// 文件传输协议接口
typedef struct file_transfer_ops {
    /**
     * @brief 核心处理：输入字节流
     * 
     * 调用者将从底层（如串口）接收到的原始数据喂入此接口。
     * 协议内部会进行状态机解析、拆包、校验。
     * 
     * @param owner 协议对象实例指针 (如 xmodem_t*)
     * @param in_buf 接收到的数据缓冲区
     * @param in_len 数据长度
     * @return i32 >0 表示处理后产生了需要回复的数据长度，0 表示无回复，<0 表示错误
     */
    i32 (*process)(void *owner, const u8 *in_buf, usize in_len);

    /**
     * @brief 核心处理：驱动定时器
     * 
     * 用于处理协议内部的超时逻辑（如 XModem 等待起始信号超时、重传超时等）。
     * 调用者应以固定频率（如 10ms 或 100ms）调用此接口。
     * 
     * @param owner 协议对象实例指针
     * @param ms_delta 距离上次调用过去的时间（毫秒）
     * @return i32 >0 表示因为超时产生了需要回复的数据长度，0 表示无动作，<0 表示错误
     */
    i32 (*tick)(void *owner, u32 ms_delta);

    /**
     * @brief 核心处理：输出字节流
     * 
     * 当 process 或 tick 返回值 >0 时，调用者应调用此接口获取待发送的回复数据。
     * 
     * @param owner 协议对象实例指针
     * @param out_buf 用于存放待发送数据的缓冲区
     * @param out_len 缓冲区大小
     * @return i32 实际填入的数据长度
     */
    i32 (*poll)(void *owner, u8 *out_buf, usize out_len);
} file_transfer_ops_t;

typedef enum {
    TP_XMODEM,
    TP_YMODEM,
    TP_ZMODEM
} transfer_protocol_enum;

typedef struct file_transfer {
    // 传输协议类型
    transfer_protocol_enum proto;
    // 指向实际协议操作接口对象的指针
    file_transfer_ops_t* ops;
    // 指向实际协议对象实例的指针（如 xmodem_t*）
    void* proto_ins;
} file_transfer_t;

void file_transfer_init(file_transfer_t* owner, transfer_protocol_enum proto, void *proto_ins);


#endif // !LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
