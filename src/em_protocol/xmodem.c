#include "xmodem.h"
#include <cassert>
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

// 2. 核心处理：输出字节流
// out_buf: 用于存放待发送数据的缓冲区
// out_len: 缓冲区大小
// 返回值: 实际填入的数据长度 (如果为0，表示暂时不需要发数据)
static i32 xmodem_poll(void* owner, u8* out_buf, usize out_len) {
	assert(owner != NULL);
	assert(out_buf != NULL);

	xmodem_t* xm = (xmodem_t*)owner;

	// 根据状态机的状态，生成需要发送的数据到out_buf

	// 返回实际填入的数据长度

	return 0;
}

void xmodem_init(xmodem_t* xm, ringbuffer_t* rb, ringbuffer_t* sb)
{
	xm->rb = rb;
	xm->sb = sb;
	xm->ops.cbs = &xm->cbs;
	xm->ops.process = xmodem_process;
	xm->ops.poll = xmodem_poll;
	xm->state = 0;
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
