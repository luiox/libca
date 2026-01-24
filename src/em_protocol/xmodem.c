#include "xmodem.h"
#include "../em_util/crc.h"
#include "../em_base/debug.h"
#include <string.h>

i32 xmodem_init(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* user_data);
i32 xmodem_tick(void* self, u32 ms_delta);
i32 xmodem_process(void* self, const u8* in_buf, usize in_len);
void xmodem_start_recv(void *self);
void xmodem_start_send(void *self, const char* filename, u32 file_size);
i32 xmodem_get_transferred_size(void *self);

static const file_transfer_ops_t g_xmodem_ops = {
	.init = xmodem_init,
	.tick = xmodem_tick,
	.process = xmodem_process,
	.start_recv = xmodem_start_recv,
	.start_send = xmodem_start_send,
	.get_transferred_size = xmodem_get_transferred_size,
};

const file_transfer_ops_t* get_xmodem_file_transfer_ops(void)
{
	return &g_xmodem_ops;
}

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

	// Transmitter states
	STATE_TX_WAIT_START,
	STATE_TX_SEND_PACKET,
	STATE_TX_WAIT_ACK,
	STATE_TX_SEND_EOT,
	STATE_TX_WAIT_EOT_ACK,
};

#define XMODEM_CTRLZ 0x1A


static void xmodem_tx_send_packet(xmodem_t* xm) {
    u8*   buf      = xm->packet_buf;
    usize data_len = 128; // Default to 128 for basic XMODEM

    // Fetch data from application via on_send
    usize fetched = 0;
    if (xm->cbs.on_send) {
        i32 r = xm->cbs.on_send(xm->user_data, xm->offset, &buf[3], data_len);
        if (r > 0) fetched = (usize)r;
    }

    if (fetched < data_len) {
        // Padding with CTRL-Z
        memset(&buf[3 + fetched], XMODEM_CTRLZ, data_len - fetched);
    }

    buf[0] = XMODEM_SOH;
    buf[1] = xm->packet_num;
    buf[2] = 0xFF - xm->packet_num;

    if (xm->use_crc) {
        u16 crc = crc16_xmodem(&buf[3], data_len);
        buf[3 + data_len] = (u8)(crc >> 8);
        buf[3 + data_len + 1] = (u8)(crc & 0xFF);
        
    } else {
        u8 chk = checksum_calc_u8(&buf[3], data_len);
        buf[3 + data_len] = chk;
    }
}

// 1. 核心处理：输入字节流
// in_buf: 串口收到的数据
// in_len: 数据长度
// 返回值: >0 表示处理后生成了需要回复的数据长度，0 表示无回复
static i32 xmodem_process(void* owner, const u8* in_buf, usize in_len) {
	param_check(owner != NULL);
	param_check(in_buf != NULL);

	xmodem_t* xm = (xmodem_t*)owner;
	i32 reply_len = 0;

	// 把接收到的数据写入接收缓冲区
	ringbuffer_write(xm->rb, (uint8_t*)in_buf, in_len);


	return reply_len;
}

// 2. 核心处理：驱动定时器
// owner: 协议对象实例指针
// ms_delta: 距离上次调用过去的时间（毫秒）
// 返回值: >0 表示因为超时产生了需要回复的数据长度
static i32 xmodem_tick(void* owner, u32 ms_delta) {
	param_check(owner != NULL);
	xmodem_t* xm = (xmodem_t*)owner;

	if (xm->is_transmitter) {
		if (xm->state == STATE_TX_WAIT_START || xm->state == STATE_TX_WAIT_ACK || xm->state == STATE_TX_WAIT_EOT_ACK) {
			xm->timer += ms_delta;
			if (xm->timer >= 3000) {
				xm->retry_count++;
				if (xm->retry_count > xm->max_retries) {
					xm->state = STATE_ERROR;
					if (xm->cbs.on_finish) xm->cbs.on_finish(xm->user_data, -XMODEM_ERR_RETRY_EXCEED);
					return 0;
				}
				if (xm->state == STATE_TX_WAIT_ACK) {
					xm->state = STATE_TX_SEND_PACKET;
				} else if (xm->state == STATE_TX_WAIT_EOT_ACK) {
					xm->state = STATE_TX_SEND_EOT;
				}
				xm->timer = 0;
			}
		}
		return 0;
	}

	if (xm->state == STATE_WAIT_START) {
		xm->timer += ms_delta;
		if (xm->timer >= 3000) {
			xm->retry_count++;
			if (xm->retry_count > xm->max_retries) {
				xm->state = STATE_ERROR;
				if (xm->cbs.on_finish) xm->cbs.on_finish(xm->user_data, -XMODEM_ERR_RETRY_EXCEED);
				return 0;
			}
			u8 start_char = xm->use_crc ? XMODEM_CRC : XMODEM_NAK;
			xm->timer = 0;
			return 1;
		}
	} else if (xm->state == STATE_WAIT_PACKET) {
		xm->timer += ms_delta;
		if (xm->timer >= 3000) { // 3s timeout for packet
			xm->retry_count++;
			if (xm->retry_count > xm->max_retries) {
				xm->state = STATE_ERROR;
				if (xm->cbs.on_finish) xm->cbs.on_finish(xm->user_data, -XMODEM_ERR_RETRY_EXCEED);

				return 0;
			}
			u8 nak = XMODEM_NAK;
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
	param_check(owner != NULL);
	param_check(out_buf != NULL);

	xmodem_t* xm = (xmodem_t*)owner;

	// 从发送缓冲区读取数据到 out_buf
	return ringbuffer_read(xm->rb, out_buf, out_len);
}

// xmodem实现的文件传输协议操作接口 (new ops shape)
static i32 xmodem_init_op(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* user_data) {
    xmodem_t* xm = (xmodem_t*)self;
    if (!xm) return -1;
    xm->io = io;
    if (cbs) xm->cbs = *cbs; // copy callbacks
    xm->user_data = user_data;
    return 0;
}

static void xmodem_start_recv_op(void *self) {
    xmodem_t* xm = (xmodem_t*)self;
    if (!xm) return;
    xm->is_transmitter = 0;
    xm->state = STATE_WAIT_START;
    xm->timer = 0;
    // send initial handshake: request CRC mode
    u8 c = XMODEM_CRC;
}

static void xmodem_start_send_op(void *self, const char* filename, u32 file_size) {
    xmodem_t* xm = (xmodem_t*)self;
    if (!xm) return;
    xm->is_transmitter = 1;
    xm->total_size = file_size;
    xm->state = STATE_TX_WAIT_START;
    xm->timer = 0;
}

static i32 xmodem_get_transferred_size_op(void *self) {
    xmodem_t* xm = (xmodem_t*)self;
    if (!xm) return 0;
    return (i32)xm->offset;
}

void xmodem_proto_init(xmodem_t* xm, ringbuffer_t* rb)
{
    xm->rb = rb;
    xm->io = NULL;
    memset(&xm->cbs, 0, sizeof(xm->cbs));
    xm->user_data = NULL;
    xm->state = STATE_WAIT_START;
    xm->packet_num = 1;
    xm->use_crc = 1; // Default to CRC
    xm->offset = 0;
    xm->total_size = 0;
    xm->timer = 0;
    xm->retry_count = 0;
    xm->max_retries = 10;
    xm->is_transmitter = 0;
}

void xmodem_set_as_transmitter(xmodem_t* xm, usize total_size)
{
    xm->is_transmitter = 1;
    xm->total_size = total_size;
    xm->state = STATE_TX_WAIT_START;
}

#if TEST_ENABLE

#include "../em_test/test.h"

// static u8    g_test_data[128];
// static usize g_test_len    = 0;
// static usize g_test_offset = 0;
// static bool  g_complete    = false;

// static void test_on_data(const u8* data, usize len, usize offset)
// {
// 	printf("  on_data: len=%d, offset=%d\n", (int)len, (int)offset);
// 	memcpy(g_test_data, data, len);
// 	g_test_len    = len;
// 	g_test_offset = offset;
// }

// static void test_on_complete(const u8* data, usize len)
// {
// 	printf("  on_complete: total_len=%d\n", (int)len);
// 	g_complete = true;
// }

// static bool g_error_triggered = false;
// static i32 test_on_error(i32 err_code, void* err_data)
// {
// 	printf("  on_error: code=%d\n", err_code);
// 	g_error_triggered = true;
// 	return 0; // Handled
// }

// TEST_CASE(xmodem_initial_timeout_nak)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[128], sb_buf[128];
// 	u8           reply;

// 	ringbuffer_init(&rb, rb_buf, 128);
// 	ringbuffer_init(&sb, sb_buf, 128);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xm.use_crc = 0;

// 	// Should send NAK after 3s
// 	xmodem_tick(&xm, 3000);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_NAK, reply);
// }

// TEST_CASE(xmodem_initial_timeout_crc)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[128], sb_buf[128];
// 	u8           reply;

// 	ringbuffer_init(&rb, rb_buf, 128);
// 	ringbuffer_init(&sb, sb_buf, 128);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xm.use_crc = 1;

// 	// Should send 'C' after 3s
// 	xmodem_tick(&xm, 3000);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_CRC, reply);
// }

// TEST_CASE(xmodem_receive_checksum_packet)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[512], sb_buf[512];
// 	u8           packet[132];
// 	u8           reply;

// 	ringbuffer_init(&rb, rb_buf, 512);
// 	ringbuffer_init(&sb, sb_buf, 512);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_data_cb(&xm, test_on_data);
// 	xm.use_crc = 0;
// 	xm.state   = STATE_WAIT_PACKET; // Skip start state

// 	packet[0] = XMODEM_SOH;
// 	packet[1] = 1;
// 	packet[2] = 254;
// 	for (int i = 0; i < 128; i++) packet[3 + i] = (u8)i;
// 	u8 chk = 0;
// 	for (int i = 0; i < 128; i++) chk += (u8)i;
// 	packet[131] = chk;

// 	xmodem_process(&xm, packet, 132);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
// 	TEST_ASSERT_EQUAL_INT(128, (int)xm.offset);
// 	TEST_ASSERT_EQUAL_INT(2, (int)xm.packet_num);
// }

// TEST_CASE(xmodem_receive_crc_packet)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[512], sb_buf[512];
// 	u8           packet[133];
// 	u8           reply;

// 	ringbuffer_init(&rb, rb_buf, 512);
// 	ringbuffer_init(&sb, sb_buf, 512);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_data_cb(&xm, test_on_data);
// 	xm.use_crc = 1;
// 	xm.state   = STATE_WAIT_PACKET;

// 	packet[0] = XMODEM_SOH;
// 	packet[1] = 1;
// 	packet[2] = 254;
// 	for (int i = 0; i < 128; i++) packet[3 + i] = (u8)i;
// 	u16 crc = crc16_xmodem(&packet[3], 128);
// 	packet[131] = (u8)(crc >> 8);
// 	packet[132] = (u8)(crc & 0xFF);

// 	xmodem_process(&xm, packet, 133);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
// }

// TEST_CASE(xmodem_receive_eot)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[128], sb_buf[128];
// 	u8           eot = XMODEM_EOT;
// 	u8           reply;

// 	ringbuffer_init(&rb, rb_buf, 128);
// 	ringbuffer_init(&sb, sb_buf, 128);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_complete_cb(&xm, test_on_complete);
// 	xm.state   = STATE_WAIT_PACKET;
// 	g_complete = false;

// 	xmodem_process(&xm, &eot, 1);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
// 	TEST_ASSERT_EQUAL_INT(STATE_FINISHED, xm.state);
// 	TEST_ASSERT_EQUAL_INT(true, g_complete);
// }

// TEST_CASE(xmodem_full_transfer_integration)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[1024], sb_buf[1024];
// 	u8           packet[133];
// 	u8           reply;

// 	ringbuffer_init(&rb, rb_buf, 1024);
// 	ringbuffer_init(&sb, sb_buf, 1024);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_data_cb(&xm, test_on_data);
// 	xmodem_set_on_complete_cb(&xm, test_on_complete);
// 	xm.use_crc = 1;
// 	g_complete = false;

// 	// 1. Start: Wait for 'C'
// 	xmodem_tick(&xm, 3000);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_CRC, reply);

// 	// 2. Send Packet 1
// 	packet[0] = XMODEM_SOH;
// 	packet[1] = 1;
// 	packet[2] = 254;
// 	memset(&packet[3], 0xAA, 128);
// 	u16 crc = crc16_xmodem(&packet[3], 128);
// 	packet[131] = (u8)(crc >> 8);
// 	packet[132] = (u8)(crc & 0xFF);
// 	xmodem_process(&xm, packet, 133);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);

// 	// 3. Send EOT
// 	u8 eot = XMODEM_EOT;
// 	xmodem_process(&xm, &eot, 1);
// 	TEST_ASSERT_EQUAL_INT(1, ringbuffer_read(&sb, &reply, 1));
// 	TEST_ASSERT_EQUAL_INT(XMODEM_ACK, reply);
// 	TEST_ASSERT_EQUAL_INT(true, g_complete);
// 	printf("  integration test finished\n");
// }

// TEST_CASE(xmodem_ringbuffer_too_small)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[64], sb_buf[64]; // Too small for 128B packet
// 	u8           packet[132];

// 	ringbuffer_init(&rb, rb_buf, 64);
// 	ringbuffer_init(&sb, sb_buf, 64);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_error_cb(&xm, test_on_error);
// 	xm.state = STATE_WAIT_PACKET;
// 	g_error_triggered = false;

// 	packet[0] = XMODEM_SOH;
// 	xmodem_process(&xm, packet, 1);
// 	// Should trigger on_error because rb size (64) < packet_len (132)
// 	TEST_ASSERT_EQUAL_INT(true, g_error_triggered);
// }

// TEST_CASE(xmodem_cancel_by_sender)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[128], sb_buf[128];
// 	u8           can = XMODEM_CAN;

// 	ringbuffer_init(&rb, rb_buf, 128);
// 	ringbuffer_init(&sb, sb_buf, 128);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_error_cb(&xm, test_on_error);
// 	xm.state = STATE_WAIT_PACKET;
// 	g_error_triggered = false;

// 	xmodem_process(&xm, &can, 1);
// 	TEST_ASSERT_EQUAL_INT(STATE_ERROR, xm.state);
// 	TEST_ASSERT_EQUAL_INT(true, g_error_triggered);
// }

// TEST_CASE(xmodem_timeout_retry_exceed)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[128], sb_buf[128];

// 	ringbuffer_init(&rb, rb_buf, 128);
// 	ringbuffer_init(&sb, sb_buf, 128);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_on_error_cb(&xm, test_on_error);
// 	xm.max_retries = 2;
// 	g_error_triggered = false;

// 	// 1st timeout
// 	xmodem_tick(&xm, 3000);
// 	// 2nd timeout
// 	xmodem_tick(&xm, 3000);
// 	// 3rd timeout -> exceed
// 	xmodem_tick(&xm, 3000);

// 	TEST_ASSERT_EQUAL_INT(STATE_ERROR, xm.state);
// 	TEST_ASSERT_EQUAL_INT(true, g_error_triggered);
// }

// static usize test_on_tx_fetch(u8* buf, usize len, usize offset)
// {
// 	for (usize i = 0; i < len; i++) {
// 		buf[i] = (u8)(offset + i);
// 	}
// 	return len;
// }

// TEST_CASE(xmodem_transmitter_basic)
// {
// 	xmodem_t     xm;
// 	ringbuffer_t rb, sb;
// 	u8           rb_buf[512], sb_buf[512];
// 	u8           reply;
// 	u8           packet[133];

// 	ringbuffer_init(&rb, rb_buf, 512);
// 	ringbuffer_init(&sb, sb_buf, 512);
// 	xmodem_proto_init(&xm, &rb, &sb);
// 	xmodem_set_as_transmitter(&xm, 128);
// 	xmodem_set_on_tx_fetch_cb(&xm, test_on_tx_fetch);
// 	xmodem_set_on_complete_cb(&xm, test_on_complete);
// 	g_complete = false;

// 	// 1. Send 'C' to start
// 	u8 start_crc = XMODEM_CRC;
// 	xmodem_process(&xm, &start_crc, 1);
	
// 	// 2. Check if packet 1 is in sb
// 	TEST_ASSERT_EQUAL_INT(133, (int)ringbuffer_used(&sb));
// 	ringbuffer_read(&sb, packet, 133);
// 	TEST_ASSERT_EQUAL_INT(XMODEM_SOH, packet[0]);
// 	TEST_ASSERT_EQUAL_INT(1, packet[1]);
// 	TEST_ASSERT_EQUAL_INT(254, packet[2]);
	
// 	// 3. Send ACK
// 	u8 ack = XMODEM_ACK;
// 	xmodem_process(&xm, &ack, 1);
	
// 	// 4. Check if EOT is in sb
// 	int used = (int)ringbuffer_used(&sb);
// 	TEST_ASSERT_EQUAL_INT(1, used);
// 	u8 reply_eot = 0;
// 	int read_len = (int)ringbuffer_read(&sb, &reply_eot, 1);
// 	TEST_ASSERT_EQUAL_INT(1, read_len);
// 	TEST_ASSERT_EQUAL_INT(XMODEM_EOT, reply_eot);
	
// 	// 5. Send ACK for EOT
// 	xmodem_process(&xm, &ack, 1);
// 	TEST_ASSERT_EQUAL_INT(STATE_FINISHED, xm.state);
// 	TEST_ASSERT_TRUE(g_complete);
// }

#endif
