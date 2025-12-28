#include "xmodem.h"
#include "../em_util/crc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XMODEM_SOH 0x01
#define XMODEM_STX 0x02
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NAK 0x15
#define XMODEM_CAN 0x18
#define XMODEM_CRC 0x43 // 'C'

enum {
	STATE_WAIT_START,
	STATE_WAIT_PACKET,
	STATE_FINISHED,
	STATE_ERROR,
};

static u8 calculate_checksum(const u8* data, usize len) {
	u8 checksum = 0;
	for (usize i = 0; i < len; i++) {
		checksum += data[i];
	}
	return checksum;
}

// 1. 核心处理：输入字节流
// in_buf: 串口收到的数据
// in_len: 数据长度
// 返回值: >0 表示处理后生成了需要回复的数据长度，0 表示无回复
static i32 xmodem_process(void* owner, const u8* in_buf, usize in_len) {
	assert(owner != NULL);
	assert(in_buf != NULL);

	xmodem_t* xm = (xmodem_t*)owner;
	i32 reply_len = 0;

	// 把接收到的数据写入接收缓冲区
	ringbuffer_write(xm->rb, (uint8_t*)in_buf, in_len);

	while (ringbuffer_used(xm->rb) > 0) {
		u8 first_byte;
		ringbuffer_peek(xm->rb, &first_byte, 1);

		if (xm->state == STATE_WAIT_START) {
			if (first_byte == XMODEM_SOH || first_byte == XMODEM_STX || first_byte == XMODEM_EOT) {
				xm->state = STATE_WAIT_PACKET;
				// Don't consume, let STATE_WAIT_PACKET handle it
				continue;
			} else {
				// Garbage in start state, consume it
				ringbuffer_read(xm->rb, &first_byte, 1);
				continue;
			}
		}

		if (xm->state == STATE_WAIT_PACKET) {
			if (first_byte == XMODEM_EOT) {
				ringbuffer_read(xm->rb, &first_byte, 1);
				u8 ack = XMODEM_ACK;
				ringbuffer_write(xm->sb, &ack, 1);
				xm->state = STATE_FINISHED;
				if (xm->cbs.on_complete) {
					xm->cbs.on_complete(NULL, xm->offset);
				}
				reply_len++;
				break;
			} else if (first_byte == XMODEM_CAN) {
				ringbuffer_read(xm->rb, &first_byte, 1);
				xm->state = STATE_ERROR;
				if (xm->cbs.on_error) {
					xm->cbs.on_error(XMODEM_ERR_CANCELLED, xm);
				}
				break;
			} else if (first_byte == XMODEM_SOH || first_byte == XMODEM_STX) {
				usize data_len = (first_byte == XMODEM_SOH) ? 128 : 1024;
				usize packet_len = 1 + 1 + 1 + data_len + (xm->use_crc ? 2 : 1);

				// 检查 RingBuffer 是否足够大以容纳一个完整的包
				if (xm->rb->size < packet_len) {
					if (xm->cbs.on_error) {
						if (xm->cbs.on_error(XMODEM_ERR_RB_TOO_SMALL, xm) != 0) {
							// 如果用户未解决错误，则跳过该字节以尝试同步
							ringbuffer_read(xm->rb, &first_byte, 1);
							continue;
						}
					} else {
						ringbuffer_read(xm->rb, &first_byte, 1);
						continue;
					}
				}

				if (ringbuffer_used(xm->rb) < packet_len) {
					break;
				}

				u8* packet = xm->packet_buf;
				ringbuffer_peek(xm->rb, packet, packet_len);

				u8 pkt_num = packet[1];
				u8 pkt_num_inv = packet[2];
				u8* data_ptr = &packet[3];
				bool valid = false;

				if (pkt_num + pkt_num_inv == 255) {
					if (xm->use_crc) {
						u16 crc_received = (packet[packet_len - 2] << 8) | packet[packet_len - 1];
						u16 crc_calc = crc16_xmodem(data_ptr, data_len);
						if (crc_received == crc_calc) {
							valid = true;
						}
					} else {
						u8 chk_received = packet[packet_len - 1];
						u8 chk_calc = calculate_checksum(data_ptr, data_len);
						if (chk_received == chk_calc) {
							valid = true;
						}
					}
				}

				if (valid) {
					if (pkt_num == xm->packet_num) {
						if (xm->cbs.on_data) {
							xm->cbs.on_data(data_ptr, data_len, xm->offset);
						}
						xm->offset += data_len;
						xm->packet_num++;
						xm->retry_count = 0; // Reset retry on success
						xm->timer = 0;       // Reset timeout on success
					} else if (pkt_num == (u8)(xm->packet_num - 1)) {
						// Duplicate packet, just ACK it (sender might not have received previous ACK)
					} else {
						// Out of sync?
						valid = false;
					}
				}

				if (valid) {
					u8 ack = XMODEM_ACK;
					ringbuffer_write(xm->sb, &ack, 1);
					ringbuffer_read(xm->rb, packet, packet_len); // Consume
					reply_len++;
				} else {
					u8 nak = XMODEM_NAK;
					ringbuffer_write(xm->sb, &nak, 1);
					// On error, we might want to clear the buffer to resync, 
					// but XMODEM usually just resends the same packet.
					// We should at least consume the header to avoid infinite loop if it's garbage.
					ringbuffer_read(xm->rb, &first_byte, 1);
					reply_len++;
				}
			} else {
				// Garbage, consume
				ringbuffer_read(xm->rb, &first_byte, 1);
			}
		} else if (xm->state == STATE_FINISHED) {
			// Consume everything
			ringbuffer_read(xm->rb, &first_byte, 1);
		}
	}

	return reply_len;
}

// 2. 核心处理：驱动定时器
// owner: 协议对象实例指针
// ms_delta: 距离上次调用过去的时间（毫秒）
// 返回值: >0 表示因为超时产生了需要回复的数据长度
static i32 xmodem_tick(void* owner, u32 ms_delta) {
	assert(owner != NULL);
	xmodem_t* xm = (xmodem_t*)owner;

	if (xm->state == STATE_WAIT_START) {
		xm->timer += ms_delta;
		if (xm->timer >= 3000) {
			xm->retry_count++;
			if (xm->retry_count > xm->max_retries) {
				xm->state = STATE_ERROR;
				if (xm->cbs.on_error) {
					xm->cbs.on_error(XMODEM_ERR_RETRY_EXCEED, xm);
				}
				return 0;
			}
			u8 start_char = xm->use_crc ? XMODEM_CRC : XMODEM_NAK;
			ringbuffer_write(xm->sb, &start_char, 1);
			xm->timer = 0;
			return 1;
		}
	} else if (xm->state == STATE_WAIT_PACKET) {
		xm->timer += ms_delta;
		if (xm->timer >= 3000) { // 3s timeout for packet
			xm->retry_count++;
			if (xm->retry_count > xm->max_retries) {
				xm->state = STATE_ERROR;
				if (xm->cbs.on_error) {
					xm->cbs.on_error(XMODEM_ERR_RETRY_EXCEED, xm);
				}
				return 0;
			}
			u8 nak = XMODEM_NAK;
			ringbuffer_write(xm->sb, &nak, 1);
			xm->timer = 0;
			return 1;
		}
	}

	return 0;
}

// 3. 核心处理：输出字节流
// out_buf: 用于存放待发送数据的缓冲区
// out_len: 缓冲区大小
// 返回值: 实际填入的数据长度 (如果为0，表示暂时不需要发数据)
static i32 xmodem_poll(void* owner, u8* out_buf, usize out_len) {
	assert(owner != NULL);
	assert(out_buf != NULL);

	xmodem_t* xm = (xmodem_t*)owner;

	// 从发送缓冲区读取数据到 out_buf
	return ringbuffer_read(xm->sb, out_buf, out_len);
}

// xmodem实现的文件传输协议操作接口
const file_transfer_ops_t g_xmodem_ops = {
	.process = xmodem_process,
	.tick = xmodem_tick,
	.poll = xmodem_poll
};

void xmodem_proto_init(xmodem_t* xm, ringbuffer_t* rb, ringbuffer_t* sb)
{
	xm->rb = rb;
	xm->sb = sb;
	xm->state = STATE_WAIT_START;
	xm->packet_num = 1;
	xm->use_crc = 1; // Default to CRC
	xm->offset = 0;
	xm->timer = 0;
	xm->retry_count = 0;
	xm->max_retries = 10;
	xm->cbs.on_data = NULL;
	xm->cbs.on_complete = NULL;
	xm->cbs.on_error = NULL;
}

void xmodem_set_on_data_cb(xmodem_t* xm, void (*on_data)(const u8 *data, usize len, usize offset))
{
	xm->cbs.on_data = on_data;
}

void xmodem_set_on_complete_cb(xmodem_t* xm, void (*on_complete)(const u8 *data, usize len))
{
	xm->cbs.on_complete = on_complete;
}

void xmodem_set_on_error_cb(xmodem_t* xm, i32 (*on_error)(i32 err_code, void* err_data))
{
	xm->cbs.on_error = on_error;
}

#if TEST_ENABLE

#include "../em_test/test.h"

static u8    g_test_data[128];
static usize g_test_len    = 0;
static usize g_test_offset = 0;
static bool  g_complete    = false;

static void test_on_data(const u8* data, usize len, usize offset)
{
	printf("  on_data: len=%d, offset=%d\n", (int)len, (int)offset);
	memcpy(g_test_data, data, len);
	g_test_len    = len;
	g_test_offset = offset;
}

static void test_on_complete(const u8* data, usize len)
{
	printf("  on_complete: total_len=%d\n", (int)len);
	g_complete = true;
}

static bool g_error_triggered = false;
static i32 test_on_error(i32 err_code, void* err_data)
{
	printf("  on_error: code=%d\n", err_code);
	g_error_triggered = true;
	return 0; // Handled
}

TEST_CASE(xmodem_initial_timeout_nak)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[128], sb_buf[128];
	u8           reply;

	ringbuffer_init(&rb, rb_buf, 128);
	ringbuffer_init(&sb, sb_buf, 128);
	xmodem_proto_init(&xm, &rb, &sb);
	xm.use_crc = 0;

	// Should send NAK after 3s
	xmodem_tick(&xm, 3000);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_NAK, reply);
}

TEST_CASE(xmodem_initial_timeout_crc)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[128], sb_buf[128];
	u8           reply;

	ringbuffer_init(&rb, rb_buf, 128);
	ringbuffer_init(&sb, sb_buf, 128);
	xmodem_proto_init(&xm, &rb, &sb);
	xm.use_crc = 1;

	// Should send 'C' after 3s
	xmodem_tick(&xm, 3000);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_CRC, reply);
}

TEST_CASE(xmodem_receive_checksum_packet)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[512], sb_buf[512];
	u8           packet[132];
	u8           reply;

	ringbuffer_init(&rb, rb_buf, 512);
	ringbuffer_init(&sb, sb_buf, 512);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_data_cb(&xm, test_on_data);
	xm.use_crc = 0;
	xm.state   = STATE_WAIT_PACKET; // Skip start state

	packet[0] = XMODEM_SOH;
	packet[1] = 1;
	packet[2] = 254;
	for (int i = 0; i < 128; i++) packet[3 + i] = (u8)i;
	u8 chk = 0;
	for (int i = 0; i < 128; i++) chk += (u8)i;
	packet[131] = chk;

	xmodem_process(&xm, packet, 132);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
	TEST_ASSERT_EQUAL_INT(128, (int)xm.offset);
	TEST_ASSERT_EQUAL_INT(2, (int)xm.packet_num);
}

TEST_CASE(xmodem_receive_crc_packet)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[512], sb_buf[512];
	u8           packet[133];
	u8           reply;

	ringbuffer_init(&rb, rb_buf, 512);
	ringbuffer_init(&sb, sb_buf, 512);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_data_cb(&xm, test_on_data);
	xm.use_crc = 1;
	xm.state   = STATE_WAIT_PACKET;

	packet[0] = XMODEM_SOH;
	packet[1] = 1;
	packet[2] = 254;
	for (int i = 0; i < 128; i++) packet[3 + i] = (u8)i;
	u16 crc = crc16_xmodem(&packet[3], 128);
	packet[131] = (u8)(crc >> 8);
	packet[132] = (u8)(crc & 0xFF);

	xmodem_process(&xm, packet, 133);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
}

TEST_CASE(xmodem_receive_eot)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[128], sb_buf[128];
	u8           eot = XMODEM_EOT;
	u8           reply;

	ringbuffer_init(&rb, rb_buf, 128);
	ringbuffer_init(&sb, sb_buf, 128);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_complete_cb(&xm, test_on_complete);
	xm.state   = STATE_WAIT_PACKET;
	g_complete = false;

	xmodem_process(&xm, &eot, 1);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
	TEST_ASSERT_EQUAL_INT(STATE_FINISHED, xm.state);
	TEST_ASSERT_EQUAL_INT(true, g_complete);
}

TEST_CASE(xmodem_full_transfer_integration)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[1024], sb_buf[1024];
	u8           packet[133];
	u8           reply;

	ringbuffer_init(&rb, rb_buf, 1024);
	ringbuffer_init(&sb, sb_buf, 1024);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_data_cb(&xm, test_on_data);
	xmodem_set_on_complete_cb(&xm, test_on_complete);
	xm.use_crc = 1;
	g_complete = false;

	// 1. Start: Wait for 'C'
	xmodem_tick(&xm, 3000);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_CRC, reply);

	// 2. Send Packet 1
	packet[0] = XMODEM_SOH;
	packet[1] = 1;
	packet[2] = 254;
	memset(&packet[3], 0xAA, 128);
	u16 crc = crc16_xmodem(&packet[3], 128);
	packet[131] = (u8)(crc >> 8);
	packet[132] = (u8)(crc & 0xFF);
	xmodem_process(&xm, packet, 133);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);

	// 3. Send EOT
	u8 eot = XMODEM_EOT;
	xmodem_process(&xm, &eot, 1);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
	TEST_ASSERT_EQUAL_INT(true, g_complete);
	printf("  integration test finished\n");
}

TEST_CASE(xmodem_ringbuffer_too_small)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[64], sb_buf[64]; // Too small for 128B packet
	u8           packet[132];

	ringbuffer_init(&rb, rb_buf, 64);
	ringbuffer_init(&sb, sb_buf, 64);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_error_cb(&xm, test_on_error);
	xm.state = STATE_WAIT_PACKET;
	g_error_triggered = false;

	packet[0] = XMODEM_SOH;
	xmodem_process(&xm, packet, 1);
	// Should trigger on_error because rb size (64) < packet_len (132)
	TEST_ASSERT_EQUAL_INT(true, g_error_triggered);
}

TEST_CASE(xmodem_cancel_by_sender)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[128], sb_buf[128];
	u8           can = XMODEM_CAN;

	ringbuffer_init(&rb, rb_buf, 128);
	ringbuffer_init(&sb, sb_buf, 128);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_error_cb(&xm, test_on_error);
	xm.state = STATE_WAIT_PACKET;
	g_error_triggered = false;

	xmodem_process(&xm, &can, 1);
	TEST_ASSERT_EQUAL_INT(STATE_ERROR, xm.state);
	TEST_ASSERT_EQUAL_INT(true, g_error_triggered);
}

TEST_CASE(xmodem_timeout_retry_exceed)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[128], sb_buf[128];

	ringbuffer_init(&rb, rb_buf, 128);
	ringbuffer_init(&sb, sb_buf, 128);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_error_cb(&xm, test_on_error);
	xm.max_retries = 2;
	g_error_triggered = false;

	// 1st timeout
	xmodem_tick(&xm, 3000);
	// 2nd timeout
	xmodem_tick(&xm, 3000);
	// 3rd timeout -> exceed
	xmodem_tick(&xm, 3000);

	TEST_ASSERT_EQUAL_INT(STATE_ERROR, xm.state);
	TEST_ASSERT_EQUAL_INT(true, g_error_triggered);
}

#endif
