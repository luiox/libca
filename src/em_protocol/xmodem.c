#include "xmodem.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
			} else if (first_byte == XMODEM_SOH || first_byte == XMODEM_STX) {
				usize data_len = (first_byte == XMODEM_SOH) ? 128 : 1024;
				usize packet_len = 1 + 1 + 1 + data_len + (xm->use_crc ? 2 : 1);

				if (ringbuffer_used(xm->rb) < packet_len) {
					break;
				}

				u8* packet = (u8*)malloc(packet_len);
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
				free(packet);
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
			u8 start_char = xm->use_crc ? XMODEM_CRC : XMODEM_NAK;
			ringbuffer_write(xm->sb, &start_char, 1);
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
	xm->cbs.on_data = NULL;
	xm->cbs.on_complete = NULL;
}

#if TEST_ENABLE

#include "../em_test/test.h"

static u8    g_test_data[128];
static usize g_test_len    = 0;
static usize g_test_offset = 0;
static bool  g_complete    = false;

static void test_on_data(const u8* data, usize len, usize offset)
{
	memcpy(g_test_data, data, len);
	g_test_len    = len;
	g_test_offset = offset;
}

static void test_on_complete(const u8* data, usize len)
{
	g_complete = true;
}

TEST_CASE(xmodem_basic_transfer)
{
	xmodem_t     xm;
	ringbuffer_t rb, sb;
	u8           rb_buf[512], sb_buf[512];

	ringbuffer_init(&rb, rb_buf, 512);
	ringbuffer_init(&sb, sb_buf, 512);
	xmodem_proto_init(&xm, &rb, &sb);
	xmodem_set_on_data_cb(&xm, test_on_data);
	xmodem_set_on_complete_cb(&xm, test_on_complete);

	xm.use_crc = 0; // Use checksum for simplicity in test

	// 1. Initial state: should send NAK after timeout
	xmodem_tick(&xm, 3000);
	u8 reply;
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_NAK, reply);

	// 2. Send a packet
	u8 packet[1030];
	packet[0] = XMODEM_SOH;
	packet[1] = 1;
	packet[2] = 254;
	for (int i = 0; i < 128; i++)
		packet[3 + i] = (u8)i;

	u8 chk = 0;
	for (int i = 0; i < 128; i++)
		chk += (u8)i;
	packet[131] = chk;

	xmodem_process(&xm, packet, 132);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
	TEST_ASSERT_EQUAL_INT(128, xm.offset);
	TEST_ASSERT_EQUAL_INT(2, xm.packet_num);

	// Verify data
	TEST_ASSERT_EQUAL_INT(128, g_test_len);
	TEST_ASSERT_EQUAL_INT(0, g_test_offset);
	for (int i = 0; i < 128; i++) {
		if (g_test_data[i] != (u8)i) {
			printf("Data mismatch at %d: expected %d, got %d\n", i, i, g_test_data[i]);
		}
	}

	// 3. Send EOT
	u8 eot = XMODEM_EOT;
	xmodem_process(&xm, &eot, 1);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
	TEST_ASSERT_EQUAL_INT(STATE_FINISHED, xm.state);
	TEST_ASSERT_EQUAL_INT(true, g_complete);

	// 4. Test CRC mode
	xmodem_proto_init(&xm, &rb, &sb);
	xm.use_crc = 1;
	xmodem_tick(&xm, 3000);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_CRC, reply);

	packet[0] = XMODEM_SOH;
	packet[1] = 1;
	packet[2] = 254;
	for (int i = 0; i < 128; i++)
		packet[3 + i] = (u8)i;
	u16 crc = crc16_xmodem(&packet[3], 128);
	packet[131] = (u8)(crc >> 8);
	packet[132] = (u8)(crc & 0xFF);

	xmodem_process(&xm, packet, 133);
	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
}

#endif

void xmodem_set_on_data_cb(xmodem_t* xm, void (*on_data)(const u8 *data, usize len, usize offset))
{
	xm->cbs.on_data = on_data;
}

void xmodem_set_on_complete_cb(xmodem_t* xm, void (*on_complete)(const u8 *data, usize len))
{
	xm->cbs.on_complete = on_complete;
}
