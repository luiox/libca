#include "dummy.h"
#include "../em_base/debug.h"
#include <stdio.h>
#include <string.h>

void dummy_proto_init(dummy_t* d) {
    if (!d) return;
    d->io = NULL;
    memset(&d->cbs, 0, sizeof(d->cbs));
    d->user_data = NULL;
}

// init compatible with file_transfer_ops_t
i32 dummy_init(void* self, transport_t* io, const file_transfer_cbs_t* cbs, void* user_data) {
    (void)self;
    debug_print("Dummy Inited");
    if (!self) return -1;
    dummy_t* d = (dummy_t*)self;
    d->io = io;
    if (cbs) d->cbs = *cbs;
    d->user_data = user_data;
    return 0;
}

// process: echo bytes back and print using debug_print
i32 dummy_process(void* self, const u8* in_buf, usize in_len) {
    if (!self || !in_buf || in_len == 0) return 0;
    dummy_t* d = (dummy_t*)self;

    for (usize i = 0; i < in_len; i++) {
        debug_print("Echo: 0x%02X", in_buf[i]);
        // echo via transport if available
        if (d->io && d->io->write) {
            d->io->write(d->io, &in_buf[i], 1);
        }
    }

    return 0;
}

// no-op tick
i32 dummy_tick(void* self, u32 ms_delta) {
    (void)self; (void)ms_delta; return 0;
}

void dummy_start_recv(void* self) {
    (void)self; debug_print("Dummy start_recv called");
}

void dummy_start_send(void* self, const char* filename, u32 file_size) {
    (void)self; (void)filename; (void)file_size; debug_print("Dummy start_send called");
}

i32 dummy_get_transferred_size(void* self) {
    (void)self; return 0;
}

const file_transfer_ops_t g_dummy_ops = {
    .init = dummy_init,
    .tick = dummy_tick,
    .process = dummy_process,
    .start_recv = dummy_start_recv,
    .start_send = dummy_start_send,
    .get_transferred_size = dummy_get_transferred_size
};

#if TEST_ENABLE
#include "../em_test/test.h"

/*
 * Unit tests for dummy protocol (moved here per code_rule.md:
 * "对于每个模块的单元测试，应该写在对应的源文件的最下面")
 */

typedef struct {
    u8 buf[64];
    usize len;
} capture_ctx_t;

static i32 capture_write(transport_t* t, const u8* buf, usize len) {
    capture_ctx_t* c = (capture_ctx_t*)t->ctx;
    if (!c || !buf) return -1;
    if (len + c->len > sizeof(c->buf)) len = sizeof(c->buf) - c->len;
    memcpy(&c->buf[c->len], buf, len);
    c->len += len;
    debug_print("transport_write: len=%d, data=0x%02X", (int)len, c->buf[c->len-1]);
    return (i32)len;
}

static void hw_puts_output(const char* s) {
    (void)s; fputs(s, stdout);
}

TEST_CASE(dummy_echo)
{
    debug_init(hw_puts_output);

    capture_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    transport_t tr = {0};
    tr.write = capture_write;
    tr.read = NULL;
    tr.flush = NULL;
    tr.ctx = &ctx;

    dummy_t d;
    dummy_proto_init(&d);

    file_transfer_t ft;
    ft.proto = TP_XMODEM; // arbitrary
    ft.ops = &g_dummy_ops;
    ft.proto_ins = &d;

    // init should print "Dummy Inited"
    TEST_ASSERT_EQUAL_INT(0, ft.ops->init(ft.proto_ins, &tr, NULL, NULL));

    // simulate data
    u8 data = 0xAA;
    ft.ops->process(ft.proto_ins, &data, 1);

    // expect transport wrote back 1 byte 0xAA
    TEST_ASSERT_EQUAL_INT(1, (int)ctx.len);
    TEST_ASSERT_EQUAL_INT(0xAA, (int)ctx.buf[0]);
}

#endif // TEST_ENABLE
