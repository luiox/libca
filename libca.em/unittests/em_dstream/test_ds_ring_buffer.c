/* Auto-migrated from src/em_dstream/ds_ring_buffer.c test blocks */
#include "ds_ring_buffer.h"
#include "dstream.h"
#include "ring_buffer.h"
#include <string.h>   // for memcpy

/* Keep private adapter internals visible for migrated inline tests. */
#include "../../src/em_dstream/ds_ring_buffer.c"


#include <em_test/test.h>

TEST_CASE(ds_ring_buf_basic_ops)
{
    uint8_t       mem[8];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 8);

    // Fill some data
    const uint8_t d[] = {'A', 'B', 'C', 'D'};
    ring_buf_write(&rb, d, 4);

    // Adapter state
    ring_buf_adapter_t a;
    a.rb     = &rb;
    a.cursor = 0;

    dstream_t ds = {0};
    ds.buf_obj   = &a;
    ds.ops       = ring_buf_get_dstream_ops();

    // Basic info
    TEST_ASSERT_EQUAL_UINT(8, dstream_capacity(&ds));
    TEST_ASSERT_EQUAL_UINT(4, dstream_used(&ds));
    TEST_ASSERT_EQUAL_UINT(0, dstream_offset(&ds));

    // read without consuming underlying
    u8  out[4] = {0};
    i32 rn     = dstream_read(&ds, out, 2);
    TEST_ASSERT_EQUAL_INT(2, rn);
    TEST_ASSERT_EQUAL_UINT('A', out[0]);
    TEST_ASSERT_EQUAL_UINT('B', out[1]);
    TEST_ASSERT_EQUAL_UINT(2, dstream_offset(&ds));
    TEST_ASSERT_EQUAL_UINT(4, dstream_used(&ds));   // underlying unchanged

    // peek with offset
    u8 p = 0;
    TEST_ASSERT_EQUAL_INT(1, dstream_peek(&ds, 0, &p, 1));
    TEST_ASSERT_EQUAL_UINT('C', p);   // cursor at 2, peek 0 -> 'C'

    // overwrite at cursor
    const u8 w[] = {'X', 'Y'};
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
    uint8_t       mem[8];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 8);

    // write a sequence
    const u8 seq[] = {1, 2, 3, 4, 5};
    ring_buf_write(&rb, seq, sizeof(seq));

    ring_buf_adapter_t a;
    a.rb     = &rb;
    a.cursor = 0;

    dstream_t ds = {0};
    ds.buf_obj   = &a;
    ds.ops       = ring_buf_get_dstream_ops();

    TEST_ASSERT_EQUAL_UINT(5, dstream_used(&ds));

    // read helpers
    TEST_ASSERT_EQUAL_UINT(1, dstream_peek_u8(&ds, 0));
    TEST_ASSERT_EQUAL_UINT(0x0201, dstream_peek_u16_le(&ds, 0));

    // read sequence via read helpers
    TEST_ASSERT_EQUAL_UINT(1, dstream_read_u8(&ds));
    TEST_ASSERT_EQUAL_UINT(0x0302, dstream_read_u16_le(&ds));
}
