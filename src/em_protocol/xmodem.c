#include "xmodem.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 1. 核心处理：输入字节流
// in_buf: 串口收到的数据
// in_len: 数据长度
// 返回值: >0 表示处理后生成了需要回复的数据长度，0 表示无回复
static i32 xmodem_process(void* owner, const u8* in_buf, usize in_len) {
	assert(owner != NULL);
	assert(in_buf != NULL);

	xmodem_t* xm = (xmodem_t*)owner;

	// 把接收到的数据写入接收缓冲区
	ringbuffer_write(xm->rb, (uint8_t*)in_buf, in_len);

	// 使用状态机处理数据
	
	// 清除掉处理完的数据

	// 返回需要回复的数据长度
	return 0;
}

// 2. 核心处理：驱动定时器
// owner: 协议对象实例指针
// ms_delta: 距离上次调用过去的时间（毫秒）
// 返回值: >0 表示因为超时产生了需要回复的数据长度
static i32 xmodem_tick(void* owner, u32 ms_delta) {
	assert(owner != NULL);
	xmodem_t* xm = (xmodem_t*)owner;

	// 处理超时逻辑，例如：
	// if (xm->state == STATE_WAIT_SOH) {
	//     xm->timer += ms_delta;
	//     if (xm->timer > 3000) { // 3秒超时
	//         u8 nak = NAK;
	//         ringbuffer_write(xm->sb, &nak, 1);
	//         xm->timer = 0;
	//         return 1;
	//     }
	// }

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
	xm->state = 0;
	xm->offset = 0;
	xm->timer = 0;
	xm->cbs.on_data = NULL;
	xm->cbs.on_complete = NULL;
}

void xmodem_set_on_data_cb(xmodem_t* xm, void (*on_data)(const u8 *data, usize len, usize offset))
{
	xm->cbs.on_data = on_data;
}

void xmodem_set_on_complete_cb(xmodem_t* xm, void (*on_complete)(const u8 *data, usize len))
{
	xm->cbs.on_complete = on_complete;
}
