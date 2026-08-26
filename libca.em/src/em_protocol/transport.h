/// @file transport.h
/// @author Canrad
/// @brief 定义抽象IO接口
/// @version 0.1
/// @date 2026-01-24
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_PROTOCOL_TRANSPORT_H
#define LIBCA_EM_PROTOCOL_TRANSPORT_H

#include <em_base/datatype.h>

// 状态码
#define TRANSPORT_OK 0
#define TRANSPORT_ERR (-1)
#define TRANSPORT_TIMEOUT (-2)
#define TRANSPORT_BUSY (-3)

typedef struct transport transport_t;

struct transport {
    /// 写数据 (发送)
    /// @param buf 数据缓冲区
    /// @param len 期望发送的长度
    /// @param ctx 私有上下文 (例如 UART 句柄)
    /// @return 实际发送的字节数，或负数错误码
    i32 (*write)(transport_t* self, const u8 *buf, usize len);

    /// 读数据 (接收) 在push模式下不能用，只有在pull模式实现的协议可以使用
    /// @param buf 存储数据的缓冲区
    /// @param len 期望读取的长度
    /// @param timeout_ms 超时时间 (毫秒)
    /// @param ctx 私有上下文
    /// @return 实际读取的字节数，0表示超时，负数表示错误
    i32 (*read)(transport_t* self, u8 *buf, usize len, u32 timeout_ms);

    /// (可选) 刷新/等待传输完成
    /// 比如串口发送可能只是进了 FIFO，需要确保真正发出去
    void (*flush)(transport_t *self);

    // 私有上下文，可能是指向具体设备的句柄 (比如 &huart1)
    void *ctx;
} ;

#endif // !LIBCA_EM_PROTOCOL_TRANSPORT_H
