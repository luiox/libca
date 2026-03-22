/**
 * @file file_transfer.h
 * @author canrad (1517807724@qq.com)
 * @brief 定义文件传输协议相关的接口
 * @version 0.1
 * @date 2025-12-27
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
#define LIBCA_EM_PROTOCOL_FILE_TRANSFER_H

#include <em_base/datatype.h>
#include "transport.h"

/*
    文件传输协议的实现思路
    采用push模型，即由user调用process把数据喂进来，而不是通过transport_t的read主动读取（pull模式）
    原因很简单，因为pull模式对缓冲区的依赖很大，但是我们没有办法在MCU上维护一个非常大的缓冲区，
    从而强制把push模式变成pull模式。
 
    对于接收模式（作为接收方的视角，例如 Bootloader 等待 PC 端发送固件）下的分析：
    1. 状态机驱动：被动与主动的结合
    由于采用了 Push 模型，协议层的运行不再是线性的，而是由外部输入数据触发的。

    被动触发：当外部数据到来时，调用 process(in_buf)，协议层根据当前的接收状态解析数据包。
    主动响应：当协议层解析完一个完整的数据包（通过 CRC 校验）后，
    协议层主动调用 transport->write() 发送应答（ACK/NAK/C）。
    2. 状态流转逻辑
    接收模式下的状态机设计如下：

    IDLE (空闲)
        触发：用户调用 start_recv()。
        动作：发送握手信号（例如 YMODEM 发送字符 ‘C’），等待发送端发送数据。
        状态转移： -> WAIT_HEADER。
    WAIT_HEADER (等待包头)
        触发：process 收到字节。
        逻辑：判断是否为起始字符（SOH/STX）。
        状态转移：
        若收到 SOH/STX -> WAIT_SEQ。
        若收到 EOT -> SEND_ACK。
        若超时（由 tick 驱动） -> 重发 ‘C’ 或 IDLE。
    WAIT_DATA (等待数据体)
        触发：process 持续接收字节。
        逻辑：将数据暂存到协议层的临时解析缓冲区。
        关键点：在 Push 模型下，数据是分片进来的（可能一次中断只来 1 个字节），
        协议层必须维护一个指针或计数器，记录“当前包已经收了多少字节”。
        CHECK_CRC (校验)
        触发：收够完整包长度（128/1024 字节 + CRC）。
        动作：
        计算并校验 CRC。
        校验包序号（防止丢包或乱序）。
        状态转移：
        成功 -> CALLBACK_WRITE。
        失败 -> SEND_NAK。
    CALLBACK_WRITE (执行写入)
        动作：调用 cbs->on_recv(offset, tmp_buf, len)。
        逻辑：这是最关键的一步。 协议层将临时缓冲区里的数据“推”给应用层。
        风险控制：
        如果 on_recv 返回失败（例如 Flash 擦除未完成或写入错误），协议层不能发 ACK，
        必须视为校验失败，进入 SEND_NAK 流程，请求发送端重传当前包。
        状态转移：
        写入成功 -> SEND_ACK。
    SEND_ACK / SEND_NAK (发送应答)
        动作：
        ACK：调用 io->write(ACK)，重置计数器，等待下一包 -> WAIT_HEADER。
        NAK：调用 io->write(NAK)，保持当前序号状态 -> WAIT_HEADER。
    3. 为什么 Push 模型在这里是完美的？
        零拷贝的潜力（或低拷贝）：
        虽然 MCU 内存小，但我们只需要一个小的临时包缓冲区（比如 1KB）。
        当 process 慢慢把这 1KB 填满后，我们一次性调用 on_write。
        如果是 Pull 模式：我们需要先等串口收满 1KB 到大缓冲区，再通知协议层去读。
        这需要双倍缓冲，或者处理非常复杂的“半包”逻辑。
        Push 模型：数据“边收边填”，填满即处理，逻辑流与数据流完美同向。
        流控保护：
        如果 Flash 写入速度慢（假设写一个包需要 50ms），在 CALLBACK_WRITE 阶段，协议层会“卡”住。
        此时，串口还在继续接收中断吗？
        Push 的优势：你可以利用硬件流控（RTS/CTS）或者简单地依靠发送端的超时重传机制。
        因为协议层还没发 ACK，PC 端（发送端）就会傻傻地等待，自动帮我们暂停了数据流。
        这比我们在代码里做复杂的缓冲区溢出检测要简单得多。
        
 */

// 文件传输协议的回调集
typedef struct
{
    /**
     * @brief 数据接收回调 数据从Protocol -> App
     * 数据流：Serial -> Protocol -> [on_recv] -> App
     * @param offset 当前文件偏移量
     * @param data   解包后的纯净数据 (不需要关心 SOH/SEQ/CRC)
     * @param len    数据长度
     * @return 0 成功, 非 0 失败(比如 Flash 写错了)
     */
    i32 (*on_recv)(void *user_data, u32 offset, const u8* data, usize len);

    /**
     * @brief 数据发送回调（供发送模式使用）：协议层向应用层读数据 App -> Protocol
     * 数据流：Serial <- Protocol <- [on_send] <- App
     * 
     * @param offset 当前文件偏移量
     * @param buf 缓冲区
     * @note 关于填充(Padding):
     * 应用层只需拷贝实际存在的有效数据到 buf 中，并返回实际拷贝的字节数。
     * 如果返回的字节数 < len，协议层(Ops)负责根据协议规范（如XMODEM）填充剩余字节(如0x1A)。
     * 应用层不需关心协议的包大小对齐问题。
     */
    i32 (*on_send)(void *user_data, u32 offset, u8* buf, usize len);
 
    /**
     * @brief 传输开始回调
     * @param total_size 文件总大小 (如果是 0 表示未知)
     * @param filename   文件名
     */
    void (*on_start)(void *user_data, u32 total_size, const char* filename);

    /**
     * @brief 传输结束回调 (成功或失败)
     * @param status 状态码
     */
    void (*on_finish)(void *user_data, i32 status);
} file_transfer_cbs_t;

// 文件传输协议接口
typedef struct file_transfer_ops
{
    /**
     * @brief 初始化协议对象
     * @param self 协议实例 (如 xmodem_t)
     * @param io 底层传输通道
     * @param cbs 上层业务回调
     * @param config 协议配置数据，其中第一个一定是void* user_data，config由协议实例拥有，在回调时候会将user_data传递给上层
     */
    i32 (*init)(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* config);

    /**
     * @brief 核心处理：驱动定时器
     *
     * 用于处理协议内部的超时逻辑（如 XModem 等待起始信号超时、重传超时等）。
     * 调用者应以固定频率（如 10ms 或 100ms）调用此接口。
     *
     * @param self 协议对象实例指针
     * @param ms_delta 距离上次调用过去的时间（毫秒）
     * @return i32 错误码
     */
    i32 (*tick)(void* self, u32 ms_delta);

    /**
     * @brief 核心处理：输入字节流
     *
     * 调用者将从底层（如串口）接收到的原始数据喂入此接口。
     * 协议内部会进行状态机解析、拆包、校验。
     *
     * @param self 协议对象实例指针 (如 xmodem_t*)
     * @param in_buf 接收到的数据缓冲区
     * @param in_len 数据长度
     * @return i32 错误码
     */
    i32 (*process)(void* self, const u8* in_buf, usize in_len);

    /**
     * @brief 请求开始接收 (发送 'C' 或者握手信号) ，因为接受一般来说需要给发送端发送一个开始信号
     * 调用此函数后，协议层将主动发送初始握手信号（如 YMODEM 发 'C'），
     * 并进入等待数据状态。接收方需调用此接口。
     *
     * @param self 协议对象实例指针 (如 xmodem_t*)
     */
    void (*start_recv)(void *self);

    /**
     * @brief 启动发送模式
     * 
     * 调用此函数后，协议层将主动发起传输（如发送初始 SOH 包头），
     * 并等待对方响应（ACK/NAK/C）。发送方需调用此接口。
     * 
     * @note 接收方无需调用此接口。
     */
    void (*start_send)(void *self, const char* filename, u32 file_size);

    /**
     * @brief 获取当前传输的字节数
     *
     * @param self 协议对象实例指针 (如 xmodem_t*)
     * @return i32 成功则返回已传输的字节数 (>=0)，失败则返回负数错误码。
     */
    i32 (*get_transferred_size)(void *self);
} file_transfer_ops_t;

typedef enum
{
    TP_XMODEM,
    TP_YMODEM,
    TP_ZMODEM
} transfer_protocol_enum;

typedef struct file_transfer
{
    // 传输协议类型
    transfer_protocol_enum proto;
    // 指向实际协议操作接口对象的指针
    file_transfer_ops_t* ops;
    // 指向实际协议对象实例的指针（如 xmodem_t*）
    void* proto_ins;
} file_transfer_t;

void file_transfer_init(file_transfer_t* owner, transfer_protocol_enum proto, void* proto_ins);


#endif   // !LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
