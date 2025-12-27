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
#include "../em_base/ringbuffer.h"

typedef struct {
    // 如果设置了该回调，则在每接收到一块数据时调用
    void (*on_data)(const u8 *data, usize len, usize offset);
    // 如果设置了该回调，则在文件传输完成时调用
    void (*on_complete)(const u8 *data, usize len);
    // YModem 特有：接收到文件名和大小的回调
    void (*on_file_info)(const char *filename, usize file_size);
} ymodem_cbs_t;

typedef struct {
    ringbuffer_t* rb;
    ringbuffer_t* sb;
    ymodem_cbs_t cbs;

    u8 state;
    usize offset;
    u32 timer;
    u8 packet_num;
    char filename[128];
    usize file_size;
    
    // 内部状态标志
    bool is_batch;
    u8 error_count;
} ymodem_t;

/**
 * @brief 初始化 YModem 协议私有数据
 */
void ymodem_proto_init(ymodem_t* ym, ringbuffer_t* rb, ringbuffer_t* sb);

/**
 * @brief 设置数据接收回调
 */
void ymodem_set_on_data_cb(ymodem_t* ym, void (*on_data)(const u8 *data, usize len, usize offset));

/**
 * @brief 设置文件信息回调
 */
void ymodem_set_on_file_info_cb(ymodem_t* ym, void (*on_file_info)(const char *filename, usize file_size));

#endif // !LIBCA_EM_PROTOCOL_YMODEM_H
