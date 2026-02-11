#include "ds_fixed_buffer.h"
#include "dstream.h"
#include "fixed_buffer.h"

static inline usize fixed_buf_dstream_capacity(dstream_t* self)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    return fixed_buf_capacity(buf);
}

static inline usize fixed_buf_dstream_used(dstream_t* self)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    return fixed_buf_used(buf);
}

static inline void fixed_buf_dstream_skip(dstream_t* self, usize len)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    fixed_buf_skip(buf, len);
}

static inline void fixed_buf_dstream_rewind(dstream_t* self, usize len)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    fixed_buf_rewind(buf, len);
}

static inline usize fixed_buf_dstream_offset(dstream_t* self)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    return buf->cursor;
}

static inline bool fixed_buf_dstream_reset(dstream_t* self, usize pos)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    if (pos > buf->used) {
        return false;
    }
    buf->cursor = pos;
    return true;
}

static inline i32 fixed_buf_dstream_read(dstream_t* self, void* dest, usize len)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    return fixed_buf_read(buf, dest, len);
}

static inline i32 fixed_buf_dstream_peek(dstream_t* self, usize offset, void* dest, usize len)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    // ensure offset from cursor is within used range
    if (buf->cursor + offset >= buf->used || len == 0) {
        return 0;
    }
    usize available = buf->used - (buf->cursor + offset);
    usize to_read = (len > available) ? available : len;
    u8* out = (u8*)dest;
    for (usize i = 0; i < to_read; ++i) {
        out[i] = fixed_buf_peek_at(buf, offset + i);
    }
    return (i32)to_read;
}

static inline i32 fixed_buf_dstream_write(dstream_t* self, const void* src, usize len)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    return fixed_buf_write(buf, src, len);
}

dstream_ops_t g_fixed_buf_dstream_ops = {
    .capacity = fixed_buf_dstream_capacity,
    .used     = fixed_buf_dstream_used,
    .skip     = fixed_buf_dstream_skip,
    .rewind   = fixed_buf_dstream_rewind,
    .offset   = fixed_buf_dstream_offset,
    .reset    = fixed_buf_dstream_reset,
    .read     = fixed_buf_dstream_read,
    .peek     = fixed_buf_dstream_peek,
    .write    = fixed_buf_dstream_write
};

const dstream_ops_t* fixed_buf_get_dstream_ops(void)
{
    return &g_fixed_buf_dstream_ops;
}

#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(ds_fixed_buf_basic_ops)
{
    u8 mem[10];
    fixed_buffer_t fb;
    fixed_buf_init(&fb, mem, 10);
    fixed_buf_append(&fb, (u8*)"ABCDE", 5);

    dstream_t ds;
    ds.buf_obj = &fb;
    ds.ops = fixed_buf_get_dstream_ops();

    // Basic info
    TEST_ASSERT_EQUAL_UINT(10, dstream_capacity(&ds));
    TEST_ASSERT_EQUAL_UINT(5, dstream_used(&ds));
    TEST_ASSERT_EQUAL_UINT(0, dstream_offset(&ds));

    // Read 2 bytes
    u8 out[6] = {0};
    i32 n = dstream_read(&ds, out, 2);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT('A', out[0]);
    TEST_ASSERT_EQUAL_UINT('B', out[1]);
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds));

    // Peek next byte (should be 'C')
    u8 v = 0;
    i32 pr = dstream_peek(&ds, 0, &v, 1);
    TEST_ASSERT_EQUAL_INT(1, pr);
    TEST_ASSERT_EQUAL_UINT('C', v);

    // Skip and rewind
    dstream_skip(&ds, 1);
    TEST_ASSERT_EQUAL_UINT(3, dstream_offset(&ds));
    dstream_rewind(&ds, 1);
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds));

    // Reset (valid)
    TEST_ASSERT_EQUAL_INT(true, dstream_reset(&ds, 1));
    TEST_ASSERT_EQUAL_UINT(1, dstream_offset(&ds));

    // Reset (invalid)
    TEST_ASSERT_EQUAL_INT(false, dstream_reset(&ds, 10));

    // Write at cursor (overwrite 'B' if cursor at 1)
    const u8 w[] = {'X','Y'};
    dstream_reset(&ds, 2);
    i32 wn = dstream_write(&ds, w, 2);
    TEST_ASSERT_EQUAL_INT(2, wn);
    TEST_ASSERT_EQUAL_UINT('X', mem[2]);
    TEST_ASSERT_EQUAL_UINT('Y', mem[3]);

    // Write appending beyond used
    dstream_reset(&ds, 5); // cursor at end
    const u8 w2[] = {'Z'};
    i32 wn2 = dstream_write(&ds, w2, 1);
    TEST_ASSERT_EQUAL_INT(1, wn2);
    TEST_ASSERT_EQUAL_UINT('Z', mem[5]);
    TEST_ASSERT_EQUAL_UINT(6, dstream_used(&ds));
}

#endif // TEST_ENABLE

