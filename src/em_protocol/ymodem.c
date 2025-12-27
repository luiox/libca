#include "ymodem.h"
#include <assert.h>
#include <string.h>

// YModem 控制字符
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CRC_C 0x43

// 1. 核心处理：输入字节流
static i32 ymodem_process(void* owner, const u8* in_buf, usize in_len) {
    assert(owner != NULL);
    ymodem_t* ym = (ymodem_t*)owner;

    if (in_buf && in_len > 0) {
        ringbuffer_write(ym->rb, (uint8_t*)in_buf, in_len);
    }

    // YModem 状态机逻辑 (简化示例)
    // 1. 等待起始包 (Block 0: 包含文件名和大小)
    // 2. 等待数据包 (128 或 1024 字节)
    // 3. 处理 EOT
    
    // 示例：如果收到数据并解析成功，产生 ACK
    // u8 ack = ACK;
    // ringbuffer_write(ym->sb, &ack, 1);

    return ringbuffer_used(ym->sb);
}

// 2. 核心处理：驱动定时器
static i32 ymodem_tick(void* owner, u32 ms_delta) {
    assert(owner != NULL);
    ymodem_t* ym = (ymodem_t*)owner;

    ym->timer += ms_delta;
    
    // 示例：启动时每隔一段时间发送 'C' 请求 CRC 模式
    if (ym->state == 0 && ym->timer >= 3000) {
        u8 c = CRC_C;
        ringbuffer_write(ym->sb, &c, 1);
        ym->timer = 0;
        return 1;
    }

    return 0;
}

// 3. 核心处理：输出字节流
static i32 ymodem_poll(void* owner, u8* out_buf, usize out_len) {
    assert(owner != NULL);
    assert(out_buf != NULL);
    ymodem_t* ym = (ymodem_t*)owner;

    return ringbuffer_read(ym->sb, out_buf, out_len);
}

// YModem 实现的文件传输协议操作接口
const file_transfer_ops_t g_ymodem_ops = {
    .process = ymodem_process,
    .tick = ymodem_tick,
    .poll = ymodem_poll
};

void ymodem_proto_init(ymodem_t* ym, ringbuffer_t* rb, ringbuffer_t* sb) {
    memset(ym, 0, sizeof(ymodem_t));
    ym->rb = rb;
    ym->sb = sb;
}

void ymodem_set_on_data_cb(ymodem_t* ym, void (*on_data)(const u8 *data, usize len, usize offset)) {
    ym->cbs.on_data = on_data;
}

void ymodem_set_on_file_info_cb(ymodem_t* ym, void (*on_file_info)(const char *filename, usize file_size)) {
    ym->cbs.on_file_info = on_file_info;
}
