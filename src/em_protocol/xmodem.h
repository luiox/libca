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

typedef struct {
    // 如果设置了该回调，则在每接收到一块数据时调用
    void (*on_data)(const u8 *data, usize len, usize offset);
    // 如果设置了该回调，则在文件传输完成时调用
    void (*on_complete)(const u8 *data, usize len);
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
    // 定时器
    u32 timer;

}xmodem_t;

/**
 * @brief 初始化 XModem 协议私有数据
 */
void xmodem_proto_init(xmodem_t* xm, ringbuffer_t* rb, ringbuffer_t* sb);
// 如果设置该回调，那就是相当于流式接收数据
void xmodem_set_on_data_cb(xmodem_t* xm, void (*on_data)(const u8 *data, usize len, usize offset));
// 如果仅仅设置该回调，那就是等文件接收完成后一次性提供数据
void xmodem_set_on_complete_cb(xmodem_t* xm, void (*on_complete)(const u8 *data, usize len));



#endif // !LIBCA_EM_PROTOCOL_XMODEM_H
