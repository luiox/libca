/// @file ymodem.h
/// @author Canrad
/// @brief 实现YMODEM文件传输协议相关的接口
/// @version 0.1
/// @date 2025-12-27
///
/// @copyright Copyright (c) 2025
///
#ifndef LIBCA_EM_PROTOCOL_YMODEM_H
#define LIBCA_EM_PROTOCOL_YMODEM_H

#include "file_transfer.h"
#include <em_util/soft_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YMODEM_OK 0
#define YMODEM_ERR_RB_TOO_SMALL 1
#define YMODEM_ERR_TIMEOUT 2
#define YMODEM_ERR_RETRY_EXCEED 3
#define YMODEM_ERR_CANCELLED 4
#define YMODEM_ERR_UNSUPPORTED 5
#define YMODEM_ERR_BAD_PACKET 6

typedef struct ymodem_config
{
    // 用户自定义数据
    void* user_data;
    // 接收缓冲区
    u8*   recv_buffer;
    usize recv_buffer_size;

    // 是否是发送者模式（当前仅实现接收者）
    bool is_transmitter;
    // 最大重试次数，如果为负数则无限重试，特征值-1表示无限重试
    i8 max_retries;
} ymodem_config_t;

typedef struct ymodem
{
    // 协议层持有的 transport
    transport_t* io;

    // 应用回调 (file_transfer_cbs_t)
    file_transfer_cbs_t* cbs;

    // 配置
    ymodem_config_t* config;

    // 状态机
    u8    state;
    u8    packet_num;
    usize offset;
    usize received_len;

    // 文件信息
    char filename[128];
    u32  file_size;
    bool file_info_ready;

    // 计时器
    acumulate_timer_t retry_timer;
    acumulate_timer_t idle_timer;

    // 重试次数
    u8 retry_count;
} ymodem_t;

/// @brief 获取 YMODEM 的全局唯一文件传输协议接口
///
/// 返回 YMODEM-1K 协议实现的文件传输操作接口，供上层应用统一调用。
///
/// @return 指向文件传输操作接口结构体的指针，不为 NULL
const file_transfer_ops_t* get_ymodem_file_transfer_ops(void);

/// @brief 初始化 YMODEM 协议实例
///
/// @param self 指向 ymodem_t 结构体的指针
/// @param io 指向传输层接口（transport_t）的指针
/// @param cbs 指向文件传输回调集合（file_transfer_cbs_t）的指针
/// @param config 指向 YMODEM 配置参数（ymodem_config_t）的指针
///
/// @return 0 表示初始化成功，非 0 表示初始化失败
i32 ymodem_init(void* self, transport_t* io, const file_transfer_cbs_t* cbs, void* config);

/// @brief YMODEM 协议的时钟滴答处理
///
/// @param self 指向 ymodem_t 实例的指针
/// @param ms_delta 距离上一次调用的时间间隔，单位为毫秒
///
/// @return 0 表示正常工作，非 0 表示发生错误
i32 ymodem_tick(void* self, u32 ms_delta);

/// @brief 处理接收到的数据
///
/// @param self 指向 ymodem_t 实例的指针
/// @param in_buf 接收到的数据缓冲区
/// @param in_len 接收数据的长度，单位为字节
///
/// @return 0 表示数据处理成功，非 0 表示发生错误
i32 ymodem_process(void* self, const u8* in_buf, usize in_len);

/// @brief 启动 YMODEM 接收模式
///
/// @param self 指向 ymodem_t 实例的指针
void ymodem_start_recv(void* self);

/// @brief 启动 YMODEM 发送模式（当前未实现）
///
/// @param self 指向 ymodem_t 实例的指针
/// @param filename 要发送的文件名称
/// @param file_size 要发送的文件总大小，单位为字节
void ymodem_start_send(void* self, const char* filename, u32 file_size);

/// @brief 获取已传输的数据大小
///
/// @param self 指向 ymodem_t 实例的指针
///
/// @return 已传输的数据大小，单位为字节
i32 ymodem_get_transferred_size(void* self);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_PROTOCOL_YMODEM_H
