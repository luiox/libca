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
    // if (ym->io && ym->io->write) ym->io->write(&ack, 1, ym->io->ctx);

    return 0; // success
}

// 2. 核心处理：驱动定时器
static i32 ymodem_tick(void* owner, u32 ms_delta) {
    assert(owner != NULL);
    ymodem_t* ym = (ymodem_t*)owner;

    ym->timer += ms_delta;
    
    // 示例：启动时每隔一段时间发送 'C' 请求 CRC 模式
    if (ym->state == 0 && ym->timer >= 3000) {
        u8 c = CRC_C;
        if (ym->io && ym->io->write) ym->io->write(&c, 1, ym->io->ctx);
        ym->timer = 0;
        return 1;
    }

    return 0;
}

// (deprecated) poll - kept for compatibility but unused in push model
static i32 ymodem_poll(void* owner, u8* out_buf, usize out_len) {
    assert(owner != NULL);
    assert(out_buf != NULL);
    ymodem_t* ym = (ymodem_t*)owner;

    return 0;
}

// YModem 实现的文件传输协议操作接口 (new ops)
static i32 ymodem_init_op(void *self, transport_t *io, const file_transfer_cbs_t *cbs, void* user_data) {
    ymodem_t* ym = (ymodem_t*)self;
    if (!ym) return -1;
    ym->io = io;
    if (cbs) ym->cbs = *cbs;
    ym->user_data = user_data;
    return 0;
}

static void ymodem_start_recv_op(void *self) {
    ymodem_t* ym = (ymodem_t*)self;
    if (!ym) return;
    ym->state = 0;
    ym->timer = 0;
    u8 c = CRC_C;
    if (ym->io && ym->io->write) ym->io->write(&c, 1, ym->io->ctx);
}

static void ymodem_start_send_op(void *self, const char* filename, u32 file_size) {
    ymodem_t* ym = (ymodem_t*)self;
    if (!ym) return;
    ym->state = 1; // sender state
    ym->file_size = file_size;
    strncpy(ym->filename, filename ? filename : "", sizeof(ym->filename)-1);
}

static i32 ymodem_get_transferred_size_op(void *self) {
    ymodem_t* ym = (ymodem_t*)self;
    if (!ym) return 0;
    return (i32)ym->offset;
}

const file_transfer_ops_t g_ymodem_ops = {
    .init = ymodem_init_op,
    .tick = ymodem_tick,
    .process = ymodem_process,
    .start_recv = ymodem_start_recv_op,
    .start_send = ymodem_start_send_op,
    .get_transferred_size = ymodem_get_transferred_size_op
};

void ymodem_proto_init(ymodem_t* ym, ringbuffer_t* rb) {
    memset(ym, 0, sizeof(ymodem_t));
    ym->rb = rb;
    ym->io = NULL;
    ym->user_data = NULL;
    ym->legacy_on_data = NULL;
    ym->legacy_on_file_info = NULL;
}

void ymodem_set_on_data_cb(ymodem_t* ym, void (*on_data)(const u8 *data, usize len, usize offset)) {
    ym->legacy_on_data = on_data;
}

void ymodem_set_on_file_info_cb(ymodem_t* ym, void (*on_file_info)(const char *filename, usize file_size)) {
    ym->legacy_on_file_info = on_file_info;
}
