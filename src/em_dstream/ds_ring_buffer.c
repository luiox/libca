#include "ds_ring_buffer.h"
#include "dstream.h"
#include "ring_buffer.h"
#include <string.h> // for memcpy


typedef struct ring_buf_adapter
{
    ring_buffer_t* rb;
    usize cursor; // offset from current rb->read
} ring_buf_adapter_t;

static inline usize ring_buf_dstream_capacity(dstream_t* self)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    return a->rb->size;
}

static inline usize ring_buf_dstream_used(dstream_t* self)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    return ring_buf_used(a->rb);
}

static inline void ring_buf_dstream_skip(dstream_t* self, usize len)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    usize used = ring_buf_used(a->rb);
    usize avail = (used > a->cursor) ? (used - a->cursor) : 0;
    if (len >= avail) {
        a->cursor = used;
    } else {
        a->cursor += len;
    }
}

static inline void ring_buf_dstream_rewind(dstream_t* self, usize len)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    if (len >= a->cursor) {
        a->cursor = 0;
    } else {
        a->cursor -= len;
    }
}

static inline usize ring_buf_dstream_offset(dstream_t* self)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    return a->cursor;
}

static inline bool ring_buf_dstream_reset(dstream_t* self, usize pos)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    usize used = ring_buf_used(a->rb);
    if (pos > used) {
        return false;
    }
    a->cursor = pos;
    return true;
}

static inline i32 ring_buf_dstream_read(dstream_t* self, void* dest, usize len)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    ring_buffer_t* rb = a->rb;
    usize used = ring_buf_used(rb);
    usize remaining = (used > a->cursor) ? (used - a->cursor) : 0;
    if (remaining == 0 || len == 0) return 0;
    usize to_read = (len > remaining) ? remaining : len;
    uint8_t* out = (uint8_t*)dest;
    usize start_idx = (rb->read + a->cursor) & (rb->size - 1);
    if (start_idx + to_read <= rb->size) {
        memcpy(out, &rb->buffer[start_idx], to_read);
    } else {
        usize first = rb->size - start_idx;
        memcpy(out, &rb->buffer[start_idx], first);
        memcpy(out + first, rb->buffer, to_read - first);
    }
    a->cursor += to_read;
    return (i32)to_read;
}

static inline i32 ring_buf_dstream_peek(dstream_t* self, usize offset, void* dest, usize len)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    ring_buffer_t* rb = a->rb;
    usize used = ring_buf_used(rb);
    usize remaining = (used > a->cursor) ? (used - a->cursor) : 0;
    if (offset >= remaining || len == 0) return 0;
    usize available = remaining - offset;
    usize to_read = (len > available) ? available : len;
    uint8_t* out = (uint8_t*)dest;
    usize start_idx = (rb->read + a->cursor + offset) & (rb->size - 1);
    if (start_idx + to_read <= rb->size) {
        memcpy(out, &rb->buffer[start_idx], to_read);
    } else {
        usize first = rb->size - start_idx;
        memcpy(out, &rb->buffer[start_idx], first);
        memcpy(out + first, rb->buffer, to_read - first);
    }
    return (i32)to_read;
}

static inline i32 ring_buf_dstream_write(dstream_t* self, const void* src, usize len)
{
    ring_buf_adapter_t* a = (ring_buf_adapter_t*)self->buf_obj;
    ring_buffer_t* rb = a->rb;
    const uint8_t* in = (const uint8_t*)src;
    usize written = 0;
    usize used = ring_buf_used(rb);
    usize mask = rb->size - 1;

    for (usize i = 0; i < len; ++i) {
        usize pos = a->cursor + i;
        if (pos < used) {
            // overwrite existing data
            usize idx = (rb->read + pos) & mask;
            rb->buffer[idx] = in[i];
            written++;
        } else {
            // append if free
            usize free_space = ring_buf_free(rb);
            if (free_space == 0) break;
            rb->buffer[rb->write] = in[i];
            rb->write = (rb->write + 1) & mask;
            rb->used += 1;
            written++;
        }
    }

    // advance cursor by written (cap at used)
    used = ring_buf_used(rb);
    if (a->cursor + written > used) {
        a->cursor = used;
    } else {
        a->cursor += written;
    }

    return (i32)written;
}

static dstream_ops_t g_ring_buf_dstream_ops = {
    .capacity = ring_buf_dstream_capacity,
    .used     = ring_buf_dstream_used,
    .skip     = ring_buf_dstream_skip,
    .rewind   = ring_buf_dstream_rewind,
    .offset   = ring_buf_dstream_offset,
    .reset    = ring_buf_dstream_reset,
    .read     = ring_buf_dstream_read,
    .peek     = ring_buf_dstream_peek,
    .write    = ring_buf_dstream_write
};

const dstream_ops_t* ring_buf_get_dstream_ops(void)
{
    return &g_ring_buf_dstream_ops;
}


#if TEST_ENABLE

#include <em_test/test.h>

TEST_CASE(ds_ring_buf_basic_ops)
{
    uint8_t mem[8];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 8);

    // Fill some data
    const uint8_t d[] = { 'A', 'B', 'C', 'D' };
    ring_buf_write(&rb, d, 4);

    // Adapter state
    ring_buf_adapter_t a;
    a.rb = &rb;
    a.cursor = 0;

    dstream_t ds = {0};
    ds.buf_obj = &a;
    ds.ops = ring_buf_get_dstream_ops();

    // Basic info
    TEST_ASSERT_EQUAL_UINT(8, dstream_capacity(&ds));
    TEST_ASSERT_EQUAL_UINT(4, dstream_used(&ds));
    TEST_ASSERT_EQUAL_UINT(0, dstream_offset(&ds));

    // read without consuming underlying
    u8 out[4] = {0};
    i32 rn = dstream_read(&ds, out, 2);
    TEST_ASSERT_EQUAL_INT(2, rn);
    TEST_ASSERT_EQUAL_UINT('A', out[0]);
    TEST_ASSERT_EQUAL_UINT('B', out[1]);
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds));
    TEST_ASSERT_EQUAL_UINT(4, dstream_used(&ds)); // underlying unchanged

    // peek with offset
    u8 p = 0;
    TEST_ASSERT_EQUAL_INT(1, dstream_peek(&ds, 0, &p, 1));
    TEST_ASSERT_EQUAL_UINT('C', p); // cursor at 2, peek 0 -> 'C'

    // overwrite at cursor
    const u8 w[] = {'X','Y'};
    TEST_ASSERT_EQUAL_INT(2, dstream_write(&ds, w, 2));
    TEST_ASSERT_EQUAL_UINT('X', mem[2]);
    TEST_ASSERT_EQUAL_UINT('Y', mem[3]);

    // append at end
    TEST_ASSERT_EQUAL_INT(1, dstream_write(&ds, (u8*)"Z", 1));
    TEST_ASSERT_EQUAL_UINT('Z', mem[4]);
    TEST_ASSERT_EQUAL_UINT(5, dstream_used(&ds));

    // reset and skip/rewind
    TEST_ASSERT_EQUAL_INT(true, dstream_reset(&ds, 1));
    TEST_ASSERT_EQUAL_UINT(1, dstream_offset(&ds));
    dstream_skip(&ds, 2);
    TEST_ASSERT_EQUAL_UINT(3, dstream_offset(&ds));
    dstream_rewind(&ds, 1);
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds));
}

TEST_CASE(ds_ring_buf_helpers)
{
    uint8_t mem[8];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 8);

    // write a sequence
    const u8 seq[] = {1,2,3,4,5};
    ring_buf_write(&rb, seq, sizeof(seq));

    ring_buf_adapter_t a;
    a.rb = &rb;
    a.cursor = 0;

    dstream_t ds = {0};
    ds.buf_obj = &a;
    ds.ops = ring_buf_get_dstream_ops();

    TEST_ASSERT_EQUAL_UINT(5, dstream_used(&ds));

    // read helpers
    TEST_ASSERT_EQUAL_UINT(1, dstream_peek_u8(&ds, 0));
    TEST_ASSERT_EQUAL_UINT(0x0201, dstream_peek_u16_le(&ds, 0));

    // read sequence via read helpers
    TEST_ASSERT_EQUAL_UINT(1, dstream_read_u8(&ds));
    TEST_ASSERT_EQUAL_UINT(0x0302, dstream_read_u16_le(&ds));
}

#endif // TEST_ENABLE