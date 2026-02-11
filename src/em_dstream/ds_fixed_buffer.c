#include "ds_fixed_buffer.h"
#include "dstream.h"
#include "fixed_buffer.h"
#include <string.h> // for memcpy


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
    usize remaining = fixed_buf_remaining_to_read(buf);
    if (offset >= remaining || len == 0) {
        return 0;
    }
    usize available = remaining - offset;
    usize to_read = (len > available) ? available : len;
    memcpy(dest, buf->raw + buf->cursor + offset, to_read);
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

TEST_CASE(ds_fixed_buf_detailed_ops)
{
    u8 mem[10];
    fixed_buffer_t fb;
    fixed_buf_init(&fb, mem, 10);
    fixed_buf_append(&fb, (u8*)"ABCDE", 5);

    dstream_t ds = {0};
    ds.buf_obj = &fb;
    ds.ops = fixed_buf_get_dstream_ops();

    // capacity / used / offset
    TEST_ASSERT_EQUAL_UINT(10, dstream_capacity(&ds));
    TEST_ASSERT_EQUAL_UINT(5, dstream_used(&ds));
    TEST_ASSERT_EQUAL_UINT(0, dstream_offset(&ds));

    // skip normal and overshoot
    dstream_skip(&ds, 3);
    TEST_ASSERT_EQUAL_UINT(3, dstream_offset(&ds));
    dstream_skip(&ds, 10);
    TEST_ASSERT_EQUAL_UINT(5, dstream_offset(&ds));

    // rewind normal and overshoot
    dstream_rewind(&ds, 2);
    TEST_ASSERT_EQUAL_UINT(3, dstream_offset(&ds));
    dstream_rewind(&ds, 10);
    TEST_ASSERT_EQUAL_UINT(0, dstream_offset(&ds));

    // reset valid and invalid
    TEST_ASSERT_EQUAL_INT(true, dstream_reset(&ds, 2));
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds));
    TEST_ASSERT_EQUAL_INT(false, dstream_reset(&ds, 6));
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds)); // unchanged

    // read partial and to-end
    u8 out[6] = {0};
    i32 rn = dstream_read(&ds, out, 3);
    TEST_ASSERT_EQUAL_INT(3, rn);
    TEST_ASSERT_EQUAL_UINT('C', out[0]);
    TEST_ASSERT_EQUAL_UINT('D', out[1]);
    TEST_ASSERT_EQUAL_UINT('E', out[2]);
    TEST_ASSERT_EQUAL_UINT(5, dstream_offset(&ds)); // at end

    // read when empty
    TEST_ASSERT_EQUAL_INT(0, dstream_read(&ds, out, 2));

    // reset and peek tests
    dstream_reset(&ds, 1);
    u8 pbuf[3] = {0};
    i32 pr = dstream_peek(&ds, 1, pbuf, 2); // peek at cursor+1 => should be 'C','D'
    TEST_ASSERT_EQUAL_INT(2, pr);
    TEST_ASSERT_EQUAL_UINT('C', pbuf[0]);
    TEST_ASSERT_EQUAL_UINT('D', pbuf[1]);

    // peek beyond used
    TEST_ASSERT_EQUAL_INT(0, dstream_peek(&ds, 10, pbuf, 1));

    // write overwrite
    dstream_reset(&ds, 1);
    const u8 w[] = {'X', 'Y'};
    TEST_ASSERT_EQUAL_INT(2, dstream_write(&ds, w, 2));
    TEST_ASSERT_EQUAL_UINT('X', mem[1]);
    TEST_ASSERT_EQUAL_UINT('Y', mem[2]);
    TEST_ASSERT_EQUAL_UINT(3, dstream_offset(&ds));
    TEST_ASSERT_EQUAL_UINT(5, dstream_used(&ds)); // used unchanged if within previous used

    // write append
    dstream_reset(&ds, 5);
    const u8 w2[] = {'Z'};
    TEST_ASSERT_EQUAL_INT(1, dstream_write(&ds, w2, 1));
    TEST_ASSERT_EQUAL_UINT('Z', mem[5]);
    TEST_ASSERT_EQUAL_UINT(6, dstream_used(&ds));

    // partial write near capacity: first extend used to 9, then try to write 2 bytes
    fixed_buf_append(&fb, (u8*)"123", 3); // used becomes 9
    dstream_reset(&ds, 9);
    const u8 w3[] = {'A','B'};
    TEST_ASSERT_EQUAL_INT(1, dstream_write(&ds, w3, 2));
    TEST_ASSERT_EQUAL_UINT('A', mem[9]);
    TEST_ASSERT_EQUAL_UINT(10, dstream_used(&ds));
}

TEST_CASE(ds_fixed_buf_peek_helpers)
{
    u8 mem[16];
    fixed_buffer_t fb;
    const u8 seq[] = {1,2,3,4, 0xFF,0xFF, 0x01,0x02};
    fixed_buf_init(&fb, mem, 16);
    fixed_buf_append(&fb, seq, sizeof(seq));

    dstream_t ds = {0};
    ds.buf_obj = &fb;
    ds.ops = fixed_buf_get_dstream_ops();

    TEST_ASSERT_EQUAL_UINT(8, dstream_used(&ds));

    // basic peek helpers (unsigned)
    TEST_ASSERT_EQUAL_UINT(1, dstream_peek_u8(&ds, 0));
    TEST_ASSERT_EQUAL_UINT(0x0201, dstream_peek_u16_le(&ds, 0));
    TEST_ASSERT_EQUAL_UINT(0x0102, dstream_peek_u16_be(&ds, 0));
    TEST_ASSERT_EQUAL_UINT(0x04030201, dstream_peek_u32_le(&ds, 0));
    TEST_ASSERT_EQUAL_UINT(0x01020304, dstream_peek_u32_be(&ds, 0));

    // signed peek
    TEST_ASSERT_EQUAL_INT(-1, dstream_peek_i8(&ds, 4));         // 0xFF
    TEST_ASSERT_EQUAL_INT(-1, dstream_peek_i16_le(&ds, 4));     // 0xFFFF

    // out-of-range peek yields zero via helpers
    TEST_ASSERT_EQUAL_UINT(0, dstream_peek_u8(&ds, 20));
}

TEST_CASE(ds_fixed_buf_read_helpers)
{
    u8 mem[16];
    fixed_buffer_t fb;
    const u8 seq[] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    fixed_buf_init(&fb, mem, 16);
    fixed_buf_append(&fb, seq, sizeof(seq));

    dstream_t ds = {0};
    ds.buf_obj = &fb;
    ds.ops = fixed_buf_get_dstream_ops();

    dstream_reset(&ds, 0);
    TEST_ASSERT_EQUAL_UINT(0x11, dstream_read_u8(&ds));
    TEST_ASSERT_EQUAL_UINT(0x3322, dstream_read_u16_le(&ds)); // reads 0x22,0x33 -> 0x3322 due to helper order
    TEST_ASSERT_EQUAL_UINT(0x44, dstream_read_u8(&ds));
    TEST_ASSERT_EQUAL_UINT(0x5566, dstream_read_u16_be(&ds)); // reads 0x55,0x66 (big-endian -> 0x5566)

    // read beyond available returns actual bytes read (partial reads)
    dstream_reset(&ds, 7);
    TEST_ASSERT_EQUAL_INT(1, dstream_read(&ds, mem, 4));
}

TEST_CASE(ds_fixed_buf_write_helpers)
{
    u8 mem[8] = {0};
    fixed_buffer_t fb;
    fixed_buf_init(&fb, mem, sizeof(mem));

    dstream_t ds = {0};
    ds.buf_obj = &fb;
    ds.ops = fixed_buf_get_dstream_ops();

    dstream_reset(&ds, 0);
    TEST_ASSERT_EQUAL_INT(1, dstream_write_u8(&ds, 0xAA));
    TEST_ASSERT_EQUAL_UINT(0xAA, mem[0]);
    TEST_ASSERT_EQUAL_UINT(1, dstream_offset(&ds));
    TEST_ASSERT_EQUAL_UINT(1, dstream_used(&ds));

    TEST_ASSERT_EQUAL_INT(2, dstream_write_u16_le(&ds, 0x2233));
    TEST_ASSERT_EQUAL_UINT(0x33, mem[1]);
    TEST_ASSERT_EQUAL_UINT(0x22, mem[2]);
    TEST_ASSERT_EQUAL_UINT(3, dstream_offset(&ds));
    TEST_ASSERT_EQUAL_UINT(3, dstream_used(&ds));

    // append to near capacity and do partial write
    fixed_buf_append(&fb, (u8*)"12345", 5); // used becomes 8
    dstream_reset(&ds, 7);
    TEST_ASSERT_EQUAL_INT(1, dstream_write_u16_le(&ds, 0x4455)); // only 1 byte fits
    TEST_ASSERT_EQUAL_UINT(0x55, mem[7]);
    TEST_ASSERT_EQUAL_UINT(8, dstream_used(&ds));
}

TEST_CASE(ds_fixed_buf_misc_helpers)
{
    u8 mem[8];
    fixed_buffer_t fb;
    fixed_buf_init(&fb, mem, sizeof(mem));
    fixed_buf_append(&fb, (u8*)"ABCD", 4);

    dstream_t ds = {0};
    ds.buf_obj = &fb;
    ds.ops = fixed_buf_get_dstream_ops();

    TEST_ASSERT_EQUAL_UINT(4, dstream_used(&ds));
    TEST_ASSERT_EQUAL_UINT(8, dstream_capacity(&ds));
    TEST_ASSERT_EQUAL_UINT(4, dstream_available(&ds));

    dstream_reset(&ds, 2);
    TEST_ASSERT_EQUAL_UINT(4, dstream_available(&ds)); // capacity - used (cursor doesn't affect available)

}

#endif // TEST_ENABLE

