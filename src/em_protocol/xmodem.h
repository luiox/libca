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
#include "../em_util/ringbuffer.h"

#define XMODEM_ERR_NONE           0
#define XMODEM_ERR_RB_TOO_SMALL   1
#define XMODEM_ERR_TIMEOUT        2
#define XMODEM_ERR_RETRY_EXCEED   3
#define XMODEM_ERR_CANCELLED      4

// NOTE: xmodem now adapted to generic file transfer callbacks
typedef struct xmodem {
    // 接收缓冲区
    ringbuffer_t* rb;

    // 协议层持有的 transport
    transport_t* io;

    // 应用回调 (file_transfer_cbs_t)
    file_transfer_cbs_t cbs;
    void* user_data;

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
 * @brief 初始化 XModem 协议私有数据 (只初始化内部缓冲)
 */
void xmodem_proto_init(xmodem_t* xm, ringbuffer_t* rb);

/**
 * @brief file_transfer 兼容 init 接口
 */
i32 xmodem_init(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* user_data);

/**
 * @brief 启动接收
 */
void xmodem_start_recv(void *self);

/**
 * @brief 启动发送
 */
void xmodem_start_send(void *self, const char* filename, u32 file_size);

/**
 * @brief 获取已传输字节数
 */
i32 xmodem_get_transferred_size(void *self);


#endif // !LIBCA_EM_PROTOCOL_XMODEM_H
