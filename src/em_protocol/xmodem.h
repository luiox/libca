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
    // 文件传输协议操作接口
    file_transfer_ops_t ops;
    // 接收缓冲区
    ringbuffer_t* rb;
    // 发送缓冲区
    ringbuffer_t* sb;
    // 回调集
    xmodem_cbs_t cbs;

    // 状态机
    u8 state;

}xmodem_t;

void xmodem_init(xmodem_t* xm, ringbuffer_t* rb, ringbuffer_t* sb);
void xmodem_set_on_data_cb(xmodem_t* xm, void (*on_data)(const u8 *data, usize len, usize offset));
void xmodem_set_on_complete_cb(xmodem_t* xm, void (*on_complete)(const u8 *data, usize len));



#endif // !LIBCA_EM_PROTOCOL_XMODEM_H
