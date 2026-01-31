/**
 * @file ymodem.h
 * @author canrad (1517807724@qq.com)
 * @brief 实现YMODEM文件传输协议相关的接口
 * @version 0.1
 * @date 2025-12-27
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef LIBCA_EM_PROTOCOL_YMODEM_H
#define LIBCA_EM_PROTOCOL_YMODEM_H

#include "file_transfer.h"
#include "../em_util/ringbuffer.h"

typedef struct ymodem {
    ringbuffer_t* rb;

    // 协议层持有的 transport
    transport_t* io;

    // 应用回调 (file_transfer_cbs_t)
    file_transfer_cbs_t cbs;
    void* user_data;

    u8 state;
    usize offset;
    u32 timer;
    u8 packet_num;
    char filename[128];
    usize file_size;
    
    // 内部状态标志
    bool is_batch;
    u8 error_count;

    /* 兼容旧回调 */
    void (*legacy_on_data)(const u8 *data, usize len, usize offset);
    void (*legacy_on_file_info)(const char *filename, usize file_size);
} ymodem_t;

/**
 * @brief 初始化 YModem 协议私有数据
 */
void ymodem_proto_init(ymodem_t* ym, ringbuffer_t* rb);

/**
 * @brief file_transfer 兼容 init 接口
 */
i32 ymodem_init(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* user_data);

/**
 * @brief 启动接收
 */
void ymodem_start_recv(void *self);

/**
 * @brief 启动发送
 */
void ymodem_start_send(void *self, const char* filename, u32 file_size);

/**
 * @brief 设置数据接收回调 (兼容旧接口)
 */
void ymodem_set_on_data_cb(ymodem_t* ym, void (*on_data)(const u8 *data, usize len, usize offset));

/**
 * @brief 设置文件信息回调 (兼容旧接口)
 */
void ymodem_set_on_file_info_cb(ymodem_t* ym, void (*on_file_info)(const char *filename, usize file_size));

#endif // !LIBCA_EM_PROTOCOL_YMODEM_H
