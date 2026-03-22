/* Auto-migrated from src/em_protocol/dummy.c test blocks */
#include "dummy.h"
#include <em_base/debug.h>
#include <stdio.h>
#include <string.h>

#include <em_test/test.h>

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

