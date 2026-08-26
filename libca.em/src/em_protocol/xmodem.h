/// @file xmodem.h
/// @author Canrad
/// @brief 实现XMODEM文件传输协议相关的接口
/// @version 0.1
/// @date 2025-12-27
///
/// @copyright Copyright (c) 2025
///
#ifndef LIBCA_EM_PROTOCOL_XMODEM_H
#define LIBCA_EM_PROTOCOL_XMODEM_H

#include "file_transfer.h"
#include <em_util/soft_timer.h>

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

    // 当前协商使用的模式（可能会因为降级而与config->mode不一致）
    xmodem_mode_t current_mode;

    // 重试定时器，咱们不需要存开始的绝对时间戳，只需要相对时间
    acumulate_timer_t retry_timer;
    // 空闲定时器，记录空闲时间
    acumulate_timer_t idle_timer;
    // 重试次数
    u8 retry_count;
}xmodem_t;

/// @brief 获取 XMODEM 的全局唯一文件传输协议接口
///
/// 返回 XMODEM 协议实现的文件传输操作接口，供上层应用统一调用。
///
/// @return 指向文件传输操作接口结构体的指针，不为 NULL
const file_transfer_ops_t* get_xmodem_file_transfer_ops(void);

/// @brief 初始化 XMODEM 协议实例
///
/// 初始化一个 XMODEM 协议实例，配置其工作模式（收/发）、传输底层接口和回调函数。
/// 本函数应在调用其他 xmodem_* 函数前调用。
///
/// @param self 指向 xmodem_t 结构体的指针
/// @param io 指向传输层接口（transport_t）的指针，用于硬件通信
/// @param cbs 指向文件传输回调集合（file_transfer_cbs_t）的指针
/// @param config 指向 XMODEM 配置参数（xmodem_config_t）的指针，包含工作模式和重试策略
///
/// @return 0 表示初始化成功，非 0 表示初始化失败
i32 xmodem_init(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* config);

/// @brief XMODEM 协议的时钟滴答处理
///
/// 定期调用此函数驱动协议状态机的超时和重试逻辑。建议每 100ms 调用一次。
/// 在接收模式下，本函数会定期发送握手信号（'C' 或 NAK）；在发送模式下，
/// 本函数会检测超时并触发重试。
///
/// @param self 指向 xmodem_t 实例的指针
/// @param ms_delta 距离上一次调用的时间间隔，单位为毫秒
///
/// @return 0 表示正常工作，非 0 表示发生错误（如超时、重试超限等）
i32 xmodem_tick(void* self, u32 ms_delta);

/// @brief 处理接收到的数据
///
/// 本函数采用 PUSH 模型，由上层应用将接收到的数据推送给协议层。
/// 协议层会解析数据包、验证完整性，并通过回调函数与应用层交互。
///
/// @param self 指向 xmodem_t 实例的指针
/// @param in_buf 接收到的数据缓冲区
/// @param in_len 接收数据的长度，单位为字节
///
/// @return 0 表示数据处理成功，非 0 表示发生错误
i32 xmodem_process(void* self, const u8* in_buf, usize in_len);

/// @brief 启动 XMODEM 接收模式
///
/// 将 XMODEM 实例切换至接收模式，准备接收文件。调用此函数后，协议将进入
/// 握手阶段，等待发送端发送数据。上层应持续调用 xmodem_tick() 和 xmodem_process()
/// 来驱动接收流程。
///
/// @param self 指向 xmodem_t 实例的指针
void xmodem_start_recv(void *self);

/// @brief 启动 XMODEM 发送模式
///
/// 将 XMODEM 实例切换至发送模式，准备发送文件。调用此函数后，协议进入
/// 等待接收端握手的阶段。上层应持续调用 xmodem_tick() 和 xmodem_process()
/// 来驱动发送流程。应用层通过 on_send 回调向协议层提供要发送的数据。
///
/// @param self 指向 xmodem_t 实例的指针
/// @param filename 要发送的文件名称（可选，某些协议变体可能使用）
/// @param file_size 要发送的文件总大小，单位为字节
void xmodem_start_send(void *self, const char* filename, u32 file_size);

/// @brief 获取已传输的数据大小
///
/// 返回当前已成功传输（接收或发送）的数据大小。在接收模式下，返回已接收
/// 的有效数据字节数；在发送模式下，返回已确认接收的数据字节数。
///
/// @param self 指向 xmodem_t 实例的指针
///
/// @return 已传输的数据大小，单位为字节
i32 xmodem_get_transferred_size(void *self);


#endif // !LIBCA_EM_PROTOCOL_XMODEM_H
