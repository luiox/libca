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
#include "../em_util/soft_timer.h"

#define XMODEM_OK                 0
#define XMODEM_ERR_RB_TOO_SMALL   1
#define XMODEM_ERR_TIMEOUT        2
#define XMODEM_ERR_RETRY_EXCEED   3
#define XMODEM_ERR_CANCELLED      4

typedef enum xmodem_mode_enum{
    XMODEM_MODE_STANDARD,
    XMODEM_MODE_CRC,
    XMODEM_MODE_1K,
}xmodem_mode_t;

typedef struct xmodem_config{
    // 用户自定义数据
    void* user_data;
    // 模式选择
    xmodem_mode_t mode;
    // 接收缓冲区
    u8* recv_buffer;
    usize recv_buffer_size;

    // 是否是发送者模式
    bool is_transmitter;
    // 最大重试次数，如果为负数则无限重试，特征值-1表示无限重试
    i8 max_retries;
}xmodem_config_t;

// NOTE: xmodem now adapted to generic file transfer callbacks
typedef struct xmodem {
    // 协议层持有的 transport
    transport_t* io;

    // 应用回调 (file_transfer_cbs_t)
    file_transfer_cbs_t* cbs;

    // 由用户通过init的config参数传递给我们的配置
    xmodem_config_t* config;

    // 状态机
    u8 state;
    // 期望的包序号
    u8 packet_num;
    // 偏移量
    usize offset;
    // 当前包接收到的长度
    usize received_len;
    // 总长度 (Transmitter使用)
    usize total_size;
    // 重试定时器，咱们不需要存开始的绝对时间戳，只需要相对时间
    acumulate_timer_t retry_timer;
    // 空闲定时器，记录空闲时间
    acumulate_timer_t idle_timer;
    // 重试次数
    u8 retry_count;
}xmodem_t;

// 获取xmodem的全局唯一文件传输协议接口
const file_transfer_ops_t* get_xmodem_file_transfer_ops(void);

i32 xmodem_init(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* config);
i32 xmodem_tick(void* self, u32 ms_delta);
i32 xmodem_process(void* self, const u8* in_buf, usize in_len);
void xmodem_start_recv(void *self);
void xmodem_start_send(void *self, const char* filename, u32 file_size);
i32 xmodem_get_transferred_size(void *self);


#endif // !LIBCA_EM_PROTOCOL_XMODEM_H
