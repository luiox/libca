#include "ymodem.h"
#include <em_base/memory_util.h>
#include <em_base/debug.h>
#include <em_util/crc.h>
#include <em_util/soft_timer.h>

// YModem 控制字符
#define YMODEM_SOH 0x01
#define YMODEM_STX 0x02
#define YMODEM_EOT 0x04
#define YMODEM_ACK 0x06
#define YMODEM_NAK 0x15
#define YMODEM_CAN 0x18
#define YMODEM_CRC 0x43

#define YMODEM_BLOCK0_LEN 128
#define YMODEM_DATA_LEN   1024
#define YMODEM_PACKET_OVERHEAD 5
#define YMODEM_MAX_PACKET_LEN (YMODEM_DATA_LEN + YMODEM_PACKET_OVERHEAD)

// 全局唯一的 ymodem 操作接口
const file_transfer_ops_t g_ymodem_ops = {
    .init                 = ymodem_init,
    .tick                 = ymodem_tick,
    .process              = ymodem_process,
    .start_recv           = ymodem_start_recv,
    .start_send           = ymodem_start_send,
    .get_transferred_size = ymodem_get_transferred_size,
};

const file_transfer_ops_t* get_ymodem_file_transfer_ops(void)
{
    return &g_ymodem_ops;
}

// YModem 状态机定义
enum ymodem_state_enum
{
    S_FINISHED,
    S_ERROR,

    S_RX_WAIT_START,
    S_RX_WAIT_PACKET,
    S_RX_IN_PACKET,
    S_RX_WAIT_EOT,
    S_RX_WAIT_END_PACKET,
};

/**
 * @brief 发送单个控制字节到传输层
 * @param ym YMODEM 实例指针
 * @param ch 要发送的字节
 * @return 无
 */
static void ymodem_send_char(ymodem_t* ym, u8 ch)
{
    if (ym->io != NULL && ym->io->write != NULL) {
        ym->io->write(ym->io, &ch, 1);
    }
}

/**
 * @brief 重置协议状态与计时器
 * @param ym YMODEM 实例指针
 * @return 无
 */
static void ymodem_reset(ymodem_t* ym)
{
    acumulate_timer_init(&ym->retry_timer, 0);
    acumulate_timer_reset(&ym->retry_timer, 0);
    acumulate_timer_init(&ym->idle_timer, 0);
    acumulate_timer_reset(&ym->idle_timer, 0);

    ym->packet_num = 0;
    ym->offset = 0;
    ym->received_len = 0;
    ym->file_size = 0;
    ym->file_info_ready = false;
    ym->retry_count = 0;
    mem_zero(ym->filename, sizeof(ym->filename));

    if (ym->config->is_transmitter) {
        ym->state = S_ERROR;
    }
    else {
        ym->state = S_RX_WAIT_START;
    }
}

/**
 * @brief 判断是否超过最大重试次数
 * @param ym YMODEM 实例指针
 * @return true 表示超过上限，false 表示未超过或允许无限重试
 */
static bool ymodem_retry_exceeded(ymodem_t* ym)
{
    i32 max_retries = (i32)ym->config->max_retries;
    if (max_retries < 0) {
        return false;
    }

    return ym->retry_count > (u8)max_retries;
}

/**
 * @brief 解析 YMODEM 的文件信息包（Block0）
 * @param ym YMODEM 实例指针
 * @param data 数据区指针
 * @param data_len 数据区长度
 * @param is_end 是否为结束包（空文件名）输出标志
 * @return true 解析成功，false 解析失败
 */
static bool ymodem_parse_file_info(ymodem_t* ym, const u8* data, usize data_len, bool* is_end)
{
    *is_end = false;

    usize i = 0;
    while (i < data_len && data[i] != '\0') {
        i++;
    }

    if (i >= data_len) {
        return false;
    }

    if (i == 0) {
        *is_end = true;
        return true;
    }

    mem_zero(ym->filename, sizeof(ym->filename));
    usize copy_len = i;
    if (copy_len > (sizeof(ym->filename) - 1)) {
        copy_len = sizeof(ym->filename) - 1;
    }
    mem_cpy(ym->filename, data, copy_len);

    usize size_pos = i + 1;
    u32 size = 0;
    while (size_pos < data_len && data[size_pos] != '\0' && data[size_pos] != ' ') {
        char c = (char)data[size_pos];
        if (c < '0' || c > '9') {
            break;
        }
        u32 digit = (u32)(c - '0');
        if (size > (0xFFFFFFFFu - digit) / 10u) {
            size = 0xFFFFFFFFu;
        }
        else {
            size = (size * 10u) + digit;
        }
        size_pos++;
    }

    ym->file_size = size;
    return true;
}

/**
 * @brief 进入错误状态并回调通知上层
 * @param ym YMODEM 实例指针
 * @param err 错误码
 * @return 无
 */
static void ymodem_handle_error(ymodem_t* ym, i32 err)
{
    ym->state = S_ERROR;
    if (ym->cbs != NULL && ym->cbs->on_finish != NULL) {
        ym->cbs->on_finish(ym->config->user_data, err);
    }
}

i32 ymodem_init(void* self, transport_t* io, const file_transfer_cbs_t* cbs, void* config)
{
    param_check(self != NULL);
    param_check(io != NULL);
    param_check(cbs != NULL);
    param_check(config != NULL);

    ymodem_t* ym = (ymodem_t*)self;
    ym->io = io;
    ym->cbs = (file_transfer_cbs_t*)cbs;
    ym->config = (ymodem_config_t*)config;

    if (ym->config->recv_buffer == NULL) {
        return YMODEM_ERR_RB_TOO_SMALL;
    }

    if (ym->config->recv_buffer_size < YMODEM_MAX_PACKET_LEN) {
        return YMODEM_ERR_RB_TOO_SMALL;
    }

    ymodem_reset(ym);
    return 0;
}

i32 ymodem_tick(void* self, u32 ms_delta)
{
    param_check(self != NULL);
    ymodem_t* ym = (ymodem_t*)self;

    if (ym->config->is_transmitter) {
        return YMODEM_ERR_UNSUPPORTED;
    }

    acumulate_timer_update(&ym->retry_timer, ms_delta);
    acumulate_timer_update(&ym->idle_timer, ms_delta);

    if (ym->state == S_RX_WAIT_START) {
        if (acumulate_timer_get_elapsed(&ym->retry_timer) >= 3000) {
            if (!ymodem_retry_exceeded(ym)) {
                ymodem_send_char(ym, YMODEM_CRC);
                ym->retry_count++;
                acumulate_timer_reset(&ym->retry_timer, 0);
            }
            else {
                ymodem_handle_error(ym, YMODEM_ERR_RETRY_EXCEED);
                return YMODEM_ERR_RETRY_EXCEED;
            }
        }
    }

    if (acumulate_timer_get_elapsed(&ym->idle_timer) > 10000) {
        debug_print("YModem transfer timeout (idle > 10s)");
        ymodem_handle_error(ym, YMODEM_ERR_TIMEOUT);
        return YMODEM_ERR_TIMEOUT;
    }

    return 0;
}

/**
 * @brief 发送 ACK 并请求 CRC 模式
 * @param ym YMODEM 实例指针
 * @return 无
 */
static void ymodem_send_ack_and_c(ymodem_t* ym)
{
    ymodem_send_char(ym, YMODEM_ACK);
    ymodem_send_char(ym, YMODEM_CRC);
}

/**
 * @brief 处理已接收完成的一个包
 * @param ym YMODEM 实例指针
 * @return 无
 */
static void ymodem_handle_packet(ymodem_t* ym)
{
    u8 header = ym->config->recv_buffer[0];
    usize data_len = (header == YMODEM_SOH) ? YMODEM_BLOCK0_LEN : YMODEM_DATA_LEN;
    usize packet_total = data_len + YMODEM_PACKET_OVERHEAD;

    if (packet_total > ym->config->recv_buffer_size) {
        ymodem_send_char(ym, YMODEM_CAN);
        ymodem_handle_error(ym, YMODEM_ERR_RB_TOO_SMALL);
        return;
    }

    u8 seq = ym->config->recv_buffer[1];
    u8 nseq = ym->config->recv_buffer[2];
    const u8* data = &ym->config->recv_buffer[3];

    u16 crc_recv = (u16)((ym->config->recv_buffer[packet_total - 2] << 8) |
                         ym->config->recv_buffer[packet_total - 1]);
    u16 crc_calc = crc16_ymodem(data, data_len);

    bool seq_ok = ((u8)(seq + nseq) == 0xFF);
    bool crc_ok = (crc_calc == crc_recv);

    if (!seq_ok || !crc_ok) {
        ym->retry_count++;
        if (ymodem_retry_exceeded(ym)) {
            ymodem_send_char(ym, YMODEM_CAN);
            ymodem_handle_error(ym, YMODEM_ERR_RETRY_EXCEED);
        }
        else {
            ymodem_send_char(ym, YMODEM_NAK);
            ym->state = S_RX_WAIT_PACKET;
        }
        return;
    }

    if (seq == (u8)(ym->packet_num - 1)) {
        ymodem_send_char(ym, YMODEM_ACK);
        ym->state = S_RX_WAIT_PACKET;
        return;
    }

    if (seq != ym->packet_num) {
        ym->retry_count++;
        if (ymodem_retry_exceeded(ym)) {
            ymodem_send_char(ym, YMODEM_CAN);
            ymodem_handle_error(ym, YMODEM_ERR_RETRY_EXCEED);
        }
        else {
            ymodem_send_char(ym, YMODEM_NAK);
            ym->state = S_RX_WAIT_PACKET;
        }
        return;
    }

    if (seq == 0) {
        if (header != YMODEM_SOH) {
            ymodem_send_char(ym, YMODEM_NAK);
            ym->state = S_RX_WAIT_PACKET;
            return;
        }

        bool is_end = false;
        if (!ymodem_parse_file_info(ym, data, data_len, &is_end)) {
            ymodem_send_char(ym, YMODEM_NAK);
            ym->state = S_RX_WAIT_PACKET;
            return;
        }

        if (is_end) {
            ymodem_send_char(ym, YMODEM_ACK);
            ym->state = S_FINISHED;
            if (ym->cbs != NULL && ym->cbs->on_finish != NULL) {
                ym->cbs->on_finish(ym->config->user_data, YMODEM_OK);
            }
            return;
        }

        ym->file_info_ready = true;
        ym->offset = 0;
        ym->packet_num = 1;
        ym->retry_count = 0;

        if (ym->cbs != NULL && ym->cbs->on_start != NULL) {
            ym->cbs->on_start(ym->config->user_data, ym->file_size, ym->filename);
        }

        ymodem_send_ack_and_c(ym);
        ym->state = S_RX_WAIT_PACKET;
        return;
    }

    if (header != YMODEM_STX) {
        ymodem_send_char(ym, YMODEM_NAK);
        ym->state = S_RX_WAIT_PACKET;
        return;
    }

    if (!ym->file_info_ready) {
        ymodem_send_char(ym, YMODEM_NAK);
        ym->state = S_RX_WAIT_PACKET;
        return;
    }

    usize actual_len = data_len;
    if (ym->file_size > 0) {
        if (ym->offset >= ym->file_size) {
            actual_len = 0;
        }
        else {
            u32 remain = ym->file_size - (u32)ym->offset;
            if (remain < actual_len) {
                actual_len = remain;
            }
        }
    }

    if (actual_len > 0) {
        if (ym->cbs != NULL && ym->cbs->on_recv != NULL) {
            if (ym->cbs->on_recv(ym->config->user_data, (u32)ym->offset, data, actual_len) != 0) {
                ym->retry_count++;
                if (ymodem_retry_exceeded(ym)) {
                    ymodem_send_char(ym, YMODEM_CAN);
                    ymodem_handle_error(ym, YMODEM_ERR_RETRY_EXCEED);
                }
                else {
                    ymodem_send_char(ym, YMODEM_NAK);
                    ym->state = S_RX_WAIT_PACKET;
                }
                return;
            }
        }

    }

    ym->offset += actual_len;
    ym->packet_num++;
    ym->retry_count = 0;
    ymodem_send_char(ym, YMODEM_ACK);
    ym->state = S_RX_WAIT_PACKET;
}

i32 ymodem_process(void* self, const u8* in_buf, usize in_len)
{
    param_check(self != NULL);
    ymodem_t* ym = (ymodem_t*)self;

    if (ym->config->is_transmitter) {
        return YMODEM_ERR_UNSUPPORTED;
    }

    if (in_buf == NULL || in_len == 0) {
        return 0;
    }

    acumulate_timer_reset(&ym->idle_timer, 0);

    usize i = 0;
    while (i < in_len) {
        u8 ch = in_buf[i];
        switch (ym->state) {
            case S_RX_WAIT_START:
            case S_RX_WAIT_PACKET:
            case S_RX_WAIT_END_PACKET:
            {
                if (ch == YMODEM_SOH || ch == YMODEM_STX) {
                    ym->config->recv_buffer[0] = ch;
                    ym->received_len = 1;
                    ym->state = S_RX_IN_PACKET;
                }
                else if (ch == YMODEM_EOT) {
                    ymodem_send_char(ym, YMODEM_NAK);
                    ym->state = S_RX_WAIT_EOT;
                }
                else if (ch == YMODEM_CAN) {
                    ymodem_handle_error(ym, YMODEM_ERR_CANCELLED);
                    return YMODEM_ERR_CANCELLED;
                }
                i++;
            } break;

            case S_RX_WAIT_EOT:
            {
                if (ch == YMODEM_EOT) {
                    ymodem_send_char(ym, YMODEM_ACK);
                    ymodem_send_char(ym, YMODEM_CRC);
                    ym->packet_num = 0;
                    ym->state = S_RX_WAIT_END_PACKET;
                }
                else if (ch == YMODEM_CAN) {
                    ymodem_handle_error(ym, YMODEM_ERR_CANCELLED);
                    return YMODEM_ERR_CANCELLED;
                }
                i++;
            } break;

            case S_RX_IN_PACKET:
            {
                u8 header = ym->config->recv_buffer[0];
                usize data_len = (header == YMODEM_SOH) ? YMODEM_BLOCK0_LEN : YMODEM_DATA_LEN;
                usize packet_total = data_len + YMODEM_PACKET_OVERHEAD;

                usize remain_in_packet = packet_total - ym->received_len;
                usize remain_in_buf = in_len - i;
                usize copy_len = (remain_in_packet < remain_in_buf) ? remain_in_packet : remain_in_buf;

                mem_cpy(&ym->config->recv_buffer[ym->received_len], &in_buf[i], copy_len);
                ym->received_len += copy_len;
                i += copy_len;

                if (ym->received_len >= packet_total) {
                    ymodem_handle_packet(ym);
                }
            } break;

            case S_FINISHED:
            case S_ERROR:
            default:
                i++;
                break;
        }
    }

    return 0;
}

void ymodem_start_recv(void* self)
{
    param_check(self != NULL);
    ymodem_t* ym = (ymodem_t*)self;
    ymodem_reset(ym);

    acumulate_timer_reset(&ym->retry_timer, 0);
    acumulate_timer_reset(&ym->idle_timer, 0);

    ymodem_send_char(ym, YMODEM_CRC);
    acumulate_timer_update(&ym->retry_timer, 3000);
}

void ymodem_start_send(void* self, const char* filename, u32 file_size)
{
    (void)filename;
    (void)file_size;

    param_check(self != NULL);
    ymodem_t* ym = (ymodem_t*)self;
    ymodem_handle_error(ym, YMODEM_ERR_UNSUPPORTED);
}

i32 ymodem_get_transferred_size(void* self)
{
    param_check(self != NULL);
    return (i32)((ymodem_t*)self)->offset;
}
