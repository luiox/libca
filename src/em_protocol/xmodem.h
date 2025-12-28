/**
 * @file xmodem.h
 * @author canrad (1517807724@qq.com)
 * @brief 实现XMODEM文件传输协议相关的接口
 * @version 0.1
 * @date 2025-12-27
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef LIBCA_EM_PROTOCOL_XMODEM_H
#define LIBCA_EM_PROTOCOL_XMODEM_H

#include "file_transfer.h"
#include "../em_base/ringbuffer.h"

#define XMODEM_ERR_NONE           0
#define XMODEM_ERR_RB_TOO_SMALL   1
#define XMODEM_ERR_TIMEOUT        2
#define XMODEM_ERR_RETRY_EXCEED   3
#define XMODEM_ERR_CANCELLED      4

typedef struct {
    // 如果设置了该回调，则在每接收到一块数据时调用 (Receiver)
    void (*on_data)(const u8 *data, usize len, usize offset);
    // 如果设置了该回调，则在文件传输完成时调用 (Receiver/Transmitter)
    void (*on_complete)(const u8 *data, usize len);
    // 错误处理回调，返回0表示错误已处理
    i32 (*on_error)(i32 err_code, void* err_data);
    // 获取待发送数据回调 (Transmitter)
    // 返回实际读取到的数据长度，0表示没有更多数据
    usize (*on_tx_fetch)(u8 *buf, usize len, usize offset);
}xmodem_cbs_t;

typedef struct{
    // 接收缓冲区
    ringbuffer_t* rb;
    // 发送缓冲区
    ringbuffer_t* sb;
    // 回调集
    xmodem_cbs_t cbs;

    // 状态机
    u8 state;
    // 期望的包序号
    u8 packet_num;
    // 是否使用CRC校验
    u8 use_crc;
    // 偏移量
    usize offset;
    // 总长度 (Transmitter使用)
    usize total_size;
    // 定时器
    u32 timer;
    // 重试次数
    u8 retry_count;
    // 最大重试次数
    u8 max_retries;
    // 是否是发送者模式
    u8 is_transmitter;

    // 临时包缓冲区，避免动态分配
    u8 packet_buf[1029];

}xmodem_t;

/**
 * @brief 初始化 XModem 协议私有数据
 */
void xmodem_proto_init(xmodem_t* xm, ringbuffer_t* rb, ringbuffer_t* sb);
// 设置为发送者模式
void xmodem_set_as_transmitter(xmodem_t* xm, usize total_size);
// 如果设置该回调，那就是相当于流式接收数据
void xmodem_set_on_data_cb(xmodem_t* xm, void (*on_data)(const u8 *data, usize len, usize offset));
// 如果仅仅设置该回调，那就是等文件接收完成后一次性提供数据
void xmodem_set_on_complete_cb(xmodem_t* xm, void (*on_complete)(const u8 *data, usize len));
// 设置错误处理回调
void xmodem_set_on_error_cb(xmodem_t* xm, i32 (*on_error)(i32 err_code, void* err_data));
// 设置发送数据获取回调
void xmodem_set_on_tx_fetch_cb(xmodem_t* xm, usize (*on_tx_fetch)(u8 *buf, usize len, usize offset));



#endif // !LIBCA_EM_PROTOCOL_XMODEM_H
