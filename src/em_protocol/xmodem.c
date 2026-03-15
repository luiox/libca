#include "xmodem.h"
#include "em_base/memory_util.h"
#include "em_util/crc.h"
#include "em_base/debug.h"
#include "em_util/soft_timer.h"
#include <string.h>

static inline bool is_use_crc(xmodem_mode_t mode)
{
    return (mode == XMODEM_MODE_CRC || mode == XMODEM_MODE_1K);
}

// Xmodem数据头
#define XMODEM_SOH 0x01
// 1K-Xmodem数据头
#define XMODEM_STX 0x02
// 发送结束
#define XMODEM_EOT 0x04
// 认可响应
#define XMODEM_ACK 0x06
// 不认可响应
#define XMODEM_NAK 0x15
// 终止传送
#define XMODEM_CAN 0x18
// 请求使用CRC校验，'C'
#define XMODEM_CRC 0x43
// 填充数据包
#define XMODEM_CTRLZ 0x1A

// 全局唯一的 xmodem 操作接口
static const file_transfer_ops_t g_xmodem_ops = {
    .init                 = xmodem_init,
    .tick                 = xmodem_tick,
    .process              = xmodem_process,
    .start_recv           = xmodem_start_recv,
    .start_send           = xmodem_start_send,
    .get_transferred_size = xmodem_get_transferred_size,
};

const file_transfer_ops_t* get_xmodem_file_transfer_ops(void)
{
    return &g_xmodem_ops;
}

// XModem 状态机定义
enum xmodem_state_enum
{
    /* --- 通用状态 (Common Terminal States) --- */
    
    // 传输成功完成：协议流程正常闭环
    S_FINISHED,
    
    // 传输异常错误：由于重试超限、超时、或收到强制取消信号(CAN)
    S_ERROR,

    /* --- 接收者状态 (Receiver States) --- */
    
    // 等待启动：接收方在此状态下周期性发送 'C'，直到收到发送方的第一个有效包头(SOH/STX)
    S_RX_WAIT_START,
    
    // 等待后续包：已处理上一包并回复过 ACK，现在等待下一个包头或传输结束标志(EOT)
    S_RX_WAIT_PACKET,
    
    // 正在接收包内容：已识别出 SOH 或 STX，正在收集后续的序号、数据及校验和
    S_RX_IN_PACKET,

    /* --- 发送者状态 (Transmitter States) --- */
    
    // 等待接收方就绪：等待接收方发回 'C' 或 NAK 以决定校验模式
    S_TX_WAIT_START,
    
    // 正在发送数据包：正在将内容推送到传输介质
    S_TX_SEND_PACKET,
    
    // 等待确认回复：等待接收方的 ACK (继续) 或 NAK (重发)
    S_TX_WAIT_ACK,
    
    // 发送结束序列：发送 EOT 告知接收方已到文件末尾
    S_TX_SEND_EOT,
    
    // 等待最终完成确认：等待接收方对 EOT 的最后一个确认 ACK
    S_TX_WAIT_EOT_ACK,
};

static void xmodem_send_char(xmodem_t* xm, u8 ch)
{
    xm->io->write(xm->io, &ch, 1);
}

static void xmodem_reset(xmodem_t* xm)
{
    // Initialize Timers
    acumulate_timer_init(&xm->retry_timer, 0);
    acumulate_timer_reset(&xm->retry_timer, 0);
    acumulate_timer_init(&xm->idle_timer, 0);
    acumulate_timer_reset(&xm->idle_timer, 0);

    // Common defaults
    xm->packet_num = 1;
    xm->offset = 0;
    xm->received_len = 0;
    xm->total_size = 0;
    xm->retry_count = 0;
    xm->current_mode = xm->config->mode;

    if (xm->config->is_transmitter) {
        // 发送者模式
        xm->state = S_TX_WAIT_START;
    }
    else {
        // 接收者模式
        xm->state = S_RX_WAIT_START;
    }
}

i32 xmodem_init(void* self, transport_t* io, const file_transfer_cbs_t* cbs, void* config)
{
    param_check(self != NULL);
    param_check(io != NULL);
    param_check(cbs != NULL);
    param_check(config != NULL);

    // 拿到self
    xmodem_t* xm = (xmodem_t*)self;
    // 初始化抽象io
    xm->io = io;
    // 初始化回调集合
    xm->cbs = (file_transfer_cbs_t*)cbs;
    // 初始化配置
    xm->config = (xmodem_config_t*)config;

    // 重置xmodem
    xmodem_reset(xm);

    return 0;
}


i32 xmodem_tick(void* self, u32 ms_delta)
{
    param_check(self != NULL);
    xmodem_t* xm = (xmodem_t*)self;

    // 更新定时器
    acumulate_timer_update(&xm->retry_timer, ms_delta);
    acumulate_timer_update(&xm->idle_timer, ms_delta);

    // 如果在握手阶段，每隔3秒发送一次'C'或'NAK'
    if (xm->state == S_RX_WAIT_START) {
        if (acumulate_timer_get_elapsed(&xm->retry_timer) >= 3000) {
            if (xm->retry_count < xm->config->max_retries || xm->config->max_retries == -1) {
                // Check for fallback to Checksum if we are in CRC/1K mode and retried enough
                if ((xm->config->mode == XMODEM_MODE_CRC || xm->config->mode == XMODEM_MODE_1K) && 
                    xm->retry_count >= 3 && xm->current_mode != XMODEM_MODE_STANDARD) {
                    xm->current_mode = XMODEM_MODE_STANDARD;
                    // Reset retry timer to send NAK immediately? No, wait next tick cycle effectively.
                    // But maybe we should send immediately? The timer just triggered so we are about to send.
                }

                // 根据模式发送启动字符
                // 标准模式发送NAK，CRC模式发送'C'
                u8 start_char = (xm->current_mode == XMODEM_MODE_STANDARD) ? XMODEM_NAK : XMODEM_CRC;
				xmodem_send_char(xm, start_char);
				// 累计重试次数
                xm->retry_count++;
                acumulate_timer_reset(&xm->retry_timer, 0);
            }
            else {
                xm->state = S_ERROR;
                xm->cbs->on_finish(xm->config->user_data, XMODEM_ERR_RETRY_EXCEED);
                return XMODEM_ERR_RETRY_EXCEED;
            }
        }
    }

    // 检查超时
    // 如果空闲时间大于10秒，则认为传输失败
    if (acumulate_timer_get_elapsed(&xm->idle_timer) > 10000) {
        debug_print("XModem transfer timeout (idle > 10s)");
        xm->state = S_ERROR;
        xm->cbs->on_finish(xm->config->user_data, XMODEM_ERR_TIMEOUT);
        return XMODEM_ERR_TIMEOUT;
    }


    return 0;
}

// 发送数据包
static void xmodem_send_data_packet(xmodem_t* xm)
{
    // 1. Determine packet size based on mode
    usize data_size = (xm->current_mode == XMODEM_MODE_1K) ? 1024 : 128;
    u8 *packet_buf = xm->config->recv_buffer; // Reuse buffer for TX construction
    
    // 2. Read data from user
    // Signature: i32 (*on_send)(void *user_data, u32 offset, u8* buf, usize len);
    // printf("Sending packet: offset=%zu, size=%zu\n", xm->offset, data_size);
    i32 read_len = xm->cbs->on_send(xm->config->user_data, xm->offset, 
                                    &packet_buf[3], data_size);
    // printf("Read len: %d\n", read_len);
    
    // 3. Check for EOF or Error
    if (read_len < 0) {
        // Read error
         xmodem_send_char(xm, XMODEM_CAN);
         xm->state = S_TX_WAIT_EOT_ACK; // Or error state
         xm->cbs->on_finish(xm->config->user_data, XMODEM_ERR_CANCELLED);
         return;
    }
    
    if (read_len == 0 && xm->total_size > 0 && xm->offset >= xm->total_size) {
        // EOF reached
        xmodem_send_char(xm, XMODEM_EOT);
        xm->state = S_TX_WAIT_EOT_ACK;
        return;
    }
    // Also handle case where read_len == 0 but offset < total_size (should not happen?)
    if (read_len == 0) {
        // Just send EOT?
        xmodem_send_char(xm, XMODEM_EOT);
        xm->state = S_TX_WAIT_EOT_ACK;
        return;
    }
    
    // 4. Fill Header
    packet_buf[0] = (data_size == 1024) ? XMODEM_STX : XMODEM_SOH;
    packet_buf[1] = xm->packet_num;
    packet_buf[2] = ~xm->packet_num;
    
    // 5. Padding
    if ((usize)read_len < data_size) {
        memset(&packet_buf[3 + read_len], XMODEM_CTRLZ, data_size - read_len);
    }
    
    // 6. Checksum/CRC
    usize idx = 3 + data_size;
    if (is_use_crc(xm->current_mode)) {
        u16 crc = crc16_xmodem(&packet_buf[3], data_size);
        packet_buf[idx++] = (crc >> 8) & 0xFF;
        packet_buf[idx++] = crc & 0xFF;
    } else {
        u8 sum = checksum_calc_u8(&packet_buf[3], data_size);
        packet_buf[idx++] = sum;
    }
    
    // 7. Send
    xm->io->write(xm->io, packet_buf, idx);
    
    // 8. Update state
    xm->state = S_TX_WAIT_ACK;
    xm->received_len = read_len; // Store temporary actual data length to update offset later
}

// 发送模式的process处理
i32 xmodem_tx_process(xmodem_t* xm, const u8* in_buf, usize in_len)
{
    usize i = 0;
    while(i < in_len) {
        u8 ch = in_buf[i];
        switch(xm->state) {
            case S_TX_WAIT_START:
                if (ch == XMODEM_CRC) {
                    xm->current_mode = XMODEM_MODE_CRC; 
                    if (xm->config->mode == XMODEM_MODE_1K) {
                        xm->current_mode = XMODEM_MODE_1K;
                    } 
                    xmodem_send_data_packet(xm);
                }
                else if (ch == XMODEM_NAK) {
                    xm->current_mode = XMODEM_MODE_STANDARD;
                    xmodem_send_data_packet(xm);
                }
                break;
                
            case S_TX_WAIT_ACK:
                if (ch == XMODEM_ACK) {
                    // Success, commit offest
                    xm->offset += xm->received_len; 
                    xm->packet_num++;
                    
                    // Check if we are done
                    if ((xm->total_size > 0 && xm->offset >= xm->total_size)) {
                         // Send EOT
                         xmodem_send_char(xm, XMODEM_EOT);
                         xm->state = S_TX_WAIT_EOT_ACK;
                    } else {
                         // Send next
                         xmodem_send_data_packet(xm);
                    }
                }
                else if (ch == XMODEM_NAK) {
                    // 重新构建数据包，然后发送，不依赖于副作用
                    xmodem_send_data_packet(xm);
                }
                else if (ch == XMODEM_CAN) {
                    xm->state = S_ERROR;
                    xm->cbs->on_finish(xm->config->user_data, XMODEM_ERR_CANCELLED);
                    return 0;
                }
                break;
                
            case S_TX_WAIT_EOT_ACK:
                if (ch == XMODEM_ACK) {
                    // All done
                    xm->state = S_FINISHED; 
                    xm->cbs->on_finish(xm->config->user_data, XMODEM_OK);
                    return 0;
                }
                else if (ch == XMODEM_NAK) {
                    // Resend EOT
                    xmodem_send_char(xm, XMODEM_EOT);
                }
                break;
        }
        i++;
    }
    return 0;
}

// 接收模式的process处理
i32 xmodem_rx_process(xmodem_t* xm, const u8* in_buf, usize in_len)
{
    acumulate_timer_reset(&xm->idle_timer, 0);
    usize i = 0;
    while (i < in_len) {
        u8 ch = in_buf[i];
        switch (xm->state) {
        case S_RX_WAIT_START:
        case S_RX_WAIT_PACKET:
        {
            if (ch == XMODEM_SOH || ch == XMODEM_STX) {
                xm->config->recv_buffer[0] = ch;
                xm->received_len = 1;
                xm->state = S_RX_IN_PACKET;
            }
            else if (ch == XMODEM_EOT) {
                xmodem_send_char(xm, XMODEM_ACK);
                xm->state = S_FINISHED;
                xm->cbs->on_finish(xm->config->user_data, XMODEM_OK);
                return 0;
            }
            else if (ch == XMODEM_CAN) {
                xm->state = S_ERROR;
                xm->cbs->on_finish(xm->config->user_data, XMODEM_ERR_CANCELLED);
                return 0;
            }
            i++;
        } break;
        case S_RX_IN_PACKET:
        {
            u8 header = xm->config->recv_buffer[0];
            usize data_len = (header == XMODEM_SOH) ? 128 : 1024;
            bool use_crc = is_use_crc(xm->current_mode);
            // XMODEM-CRC: [HEADER][SEQ][~SEQ][DATA][CRC_H][CRC_L] (Overhead=5)
            // XMODEM-SUM: [HEADER][SEQ][~SEQ][DATA][SUM]         (Overhead=4)
            usize packet_total = data_len + (use_crc ? 5 : 4);

            usize remain_in_packet = packet_total - xm->received_len;
            usize remain_in_buf = in_len - i;
            usize copy_len = (remain_in_packet < remain_in_buf) ? remain_in_packet : remain_in_buf;

            mem_cpy(&xm->config->recv_buffer[xm->received_len], &in_buf[i], copy_len);
            xm->received_len += copy_len;
            i += copy_len;

            if (xm->received_len >= packet_total) {
                // 处理完整包
                u8 seq = xm->config->recv_buffer[1];
                u8 nseq = xm->config->recv_buffer[2];
                u8* data = &xm->config->recv_buffer[3];
                
                bool check_ok = false;
                if (use_crc) {
                    u16 crc_recv = (xm->config->recv_buffer[packet_total - 2] << 8) |
                                   xm->config->recv_buffer[packet_total - 1];
                    u16 crc_calc = crc16_xmodem(data, data_len);
                    check_ok = (crc_calc == crc_recv);
                } else {
                    u8 sum_recv = xm->config->recv_buffer[packet_total - 1];
                    u8 sum_calc = checksum_calc_u8(data, data_len);
                    check_ok = (sum_calc == sum_recv);
                }

                if (seq == (u8)(~nseq) && check_ok &&
                    (seq == xm->packet_num || seq == (u8)(xm->packet_num - 1))) {
                    if (seq == xm->packet_num) {
                        // 新包
                        if (xm->cbs->on_recv(xm->config->user_data, xm->offset, data, data_len) == 0) {
                            xm->offset += data_len;
                            xm->packet_num++;
                            xmodem_send_char(xm, XMODEM_ACK);
                            xm->state = S_RX_WAIT_PACKET;
                            xm->retry_count = 0;
                        }
                        else {
                            // 回调失败，要求重新发那一包内容
                            xmodem_send_char(xm, XMODEM_NAK);
                            xm->state = S_RX_WAIT_PACKET;
                        }
                    }
                    else {
                        // 重传包
                        xmodem_send_char(xm, XMODEM_ACK);
                        xm->state = S_RX_WAIT_PACKET;
                    }
                }
                else {
                    // 校验失败或序号不对
                    xm->retry_count++;
                    if (xm->config->max_retries != -1 && xm->retry_count > xm->config->max_retries) {
                        xmodem_send_char(xm, XMODEM_CAN);
                        xm->state = S_ERROR;
                        xm->cbs->on_finish(xm->config->user_data, XMODEM_ERR_RETRY_EXCEED);
                    }
                    else {
                        xmodem_send_char(xm, XMODEM_NAK);
                        xm->state = S_RX_WAIT_PACKET;
                    }
                }
            }
        } break;
        case S_FINISHED:
        {
            i++;
        } break;
        default:
            i++;
            break;
        }
    }

    return 0;
}

i32 xmodem_process(void* self, const u8* in_buf, usize in_len)
{
    param_check(self != NULL);
    param_check(in_buf != NULL);
    xmodem_t* xm = (xmodem_t*)self;

    // 重置空闲定时器
    acumulate_timer_reset(&xm->idle_timer, 0);

    if (xm->config->is_transmitter) {
        return xmodem_tx_process(xm, in_buf, in_len);
    }
    else {
        return xmodem_rx_process(xm, in_buf, in_len);
    }
}

void xmodem_start_recv(void* self)
{
    param_check(self != NULL);
    xmodem_t* xm = (xmodem_t*)self;
    xmodem_reset(xm);

    acumulate_timer_reset(&xm->retry_timer, 0);
    acumulate_timer_reset(&xm->idle_timer, 0);

	// 设置重试时间为3000，保证下一次进入tick时能发送'C'
	acumulate_timer_update(&xm->retry_timer, 3000);
}

void xmodem_start_send(void* self, const char* filename, u32 file_size)
{
    param_check(self != NULL);
    xmodem_t* xm = (xmodem_t*)self;
    xmodem_reset(xm); // Will set state to S_TX_WAIT_START

    xm->total_size = file_size;
    xm->offset = 0;
    xm->packet_num = 1;
    
    acumulate_timer_reset(&xm->idle_timer, 0);
}

i32 xmodem_get_transferred_size(void* self)
{
    param_check(self != NULL);

    return ((xmodem_t*)self)->offset;
}
