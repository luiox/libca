#include "fixed_buffer.h"
#include "../em_base/debug.h"
#include <string.h>

void fixed_buf_init(fixed_buffer_t* self, u8* data, usize capacity)
{
    param_check(self != NULL);
    param_check(data != NULL);

    self->raw      = data;
    self->capacity = capacity;
    self->used     = 0;
    self->cursor   = 0;
}

void fixed_buf_skip(fixed_buffer_t* self, usize size)
{
    param_check(self != NULL);

    self->cursor += size;
    if (self->cursor > self->used) {
        self->cursor = self->used;
    }
}

void fixed_buf_rewind(fixed_buffer_t* self, usize size)
{
    param_check(self != NULL);

    if (size > self->cursor) {
        self->cursor = 0;
    } else {
        self->cursor -= size;
    }
}

void fixed_buf_flush(fixed_buffer_t* self)
{
    param_check(self != NULL);

    if (self->cursor == 0) {
        return;
    }
    if (self->cursor < self->used) {
        usize remaining = self->used - self->cursor;
        memmove(self->raw, self->raw + self->cursor, remaining);
        self->used = remaining;
    } else {
        self->used = 0;
    }
    self->cursor = 0;
}

void fixed_buf_new_from_cursor(fixed_buffer_t* self, fixed_buffer_t* new_b)
{
    param_check(self != NULL);
    param_check(new_b != NULL);

    usize remaining = fixed_buf_remaining_to_read(self);
    new_b->raw      = self->raw + self->cursor;
    new_b->capacity = remaining;
    new_b->used     = remaining;
    new_b->cursor   = 0;
}

i32 fixed_buf_read_u8(fixed_buffer_t* self, u8* value)
{
    param_check(self != NULL);
    param_check(value != NULL);

    if (self->cursor >= self->used) {
        return FIXED_BUF_ERR_EMPTY;
    }
    *value = self->raw[self->cursor++];
    return FIXED_BUF_OK;
}

i32 fixed_buf_read(fixed_buffer_t* self, u8* buffer, usize size)
{
    param_check(self != NULL);
    param_check(buffer != NULL);

    usize available = fixed_buf_remaining_to_read(self);
    usize to_read   = (size > available) ? available : size;
    if (to_read > 0) {
        memcpy(buffer, self->raw + self->cursor, to_read);
        self->cursor += to_read;
    }
    return (i32)to_read;
}

i32 fixed_buf_peek(fixed_buffer_t* self, u8* value)
{
    param_check(self != NULL);
    param_check(value != NULL);

    if (self->cursor >= self->used) {
        return FIXED_BUF_ERR_EMPTY;
    }
    *value = self->raw[self->cursor];
    return FIXED_BUF_OK;
}

u8 fixed_buf_peek_at(fixed_buffer_t* self, usize offset)
{
    param_check(self != NULL);

    // 防止offset过大导致self->cursor + offset可能回绕，
    // 所以不能用 (self->cursor + offset) >= self->used方式处理
    if (offset >= fixed_buf_remaining_to_read(self)) {
        return 0;
    }
    return self->raw[self->cursor + offset];
}

i32 fixed_buf_append(fixed_buffer_t* self, const u8* data, usize size)
{
    param_check(self != NULL);
    param_check(data != NULL);

    usize available = fixed_buf_available(self);
    usize to_write  = (size > available) ? available : size;
    if (to_write > 0) {
        memcpy(self->raw + self->used, data, to_write);
        self->used += to_write;
    }
    return (i32)to_write;
}

i32 fixed_buf_merge(fixed_buffer_t* self, const fixed_buffer_t* other)
{
    param_check(self != NULL);
    param_check(other != NULL);

    return fixed_buf_append(self, other->raw, other->used);
}

void fixed_buf_write_u8(fixed_buffer_t* self, usize index, u8 value)
{
    param_check(self != NULL);

    if (index >= self->capacity) {
        return;
    }
    self->raw[index] = value;
}

i32 fixed_buf_write(fixed_buffer_t* self, const u8* data, usize size)
{
    param_check(self != NULL);
    param_check(data != NULL);

    usize available = self->capacity - self->cursor;
    usize to_write  = (size > available) ? available : size;
    if (to_write > 0) {
        memcpy(self->raw + self->cursor, data, to_write);
        self->cursor += to_write;
        if (self->cursor > self->used) {
            self->used = self->cursor;
        }
    }
    return (i32)to_write;
}

#if TEST_ENABLE
#include "../em_test/test.h"

TEST_CASE(fixed_buf_test_init)
{
    u8                  buf[10];
    fixed_buffer_t fsb;

    // Success
    fixed_buf_init(&fsb, buf, 10);
    TEST_ASSERT_EQUAL_UINT(10, fixed_buf_capacity(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fixed_buf_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
}

TEST_CASE(fixed_buf_test_cursor_ops)
{
    u8                  buf[10];
    fixed_buffer_t fsb;
    fixed_buf_init(&fsb, buf, 10);

    // Normal skip
    fixed_buf_append(&fsb, (u8*)"abc", 3);
    fixed_buf_skip(&fsb, 1);
    TEST_ASSERT_EQUAL_UINT(1, fsb.cursor);

    // Overshoot skip (capped at used)
    fixed_buf_skip(&fsb, 10);
    TEST_ASSERT_EQUAL_UINT(3, fsb.cursor);

    // Normal rewind
    fixed_buf_rewind(&fsb, 1);
    TEST_ASSERT_EQUAL_UINT(2, fsb.cursor);

    // Undershoot rewind (capped at 0)
    fixed_buf_rewind(&fsb, 10);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);

    // Reset
    fixed_buf_skip(&fsb, 2);
    fixed_buf_reset_cursor(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
}

TEST_CASE(fixed_buf_test_state_info)
{
    u8                  buf[10];
    fixed_buffer_t fsb;
    fixed_buf_init(&fsb, buf, 10);

    TEST_ASSERT_EQUAL_UINT(10, fixed_buf_capacity(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fixed_buf_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(10, fixed_buf_available(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fixed_buf_remaining_to_read(&fsb));
    TEST_ASSERT(fixed_buf_data(&fsb) == buf);

    fixed_buf_append(&fsb, (u8*)"12345", 5);
    TEST_ASSERT_EQUAL_UINT(5, fixed_buf_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(5, fixed_buf_available(&fsb));
    TEST_ASSERT_EQUAL_UINT(5, fixed_buf_remaining_to_read(&fsb));

    fixed_buf_skip(&fsb, 2);
    TEST_ASSERT_EQUAL_UINT(3, fixed_buf_remaining_to_read(&fsb));

    fixed_buf_skip(&fsb, 10); // cursor at used
    TEST_ASSERT_EQUAL_UINT(0, fixed_buf_remaining_to_read(&fsb));
}

TEST_CASE(fixed_buf_test_read_ops)
{
    u8                  buf[10];
    fixed_buffer_t fsb;
    fixed_buf_init(&fsb, buf, 10);
    fixed_buf_append(&fsb, (u8*)"ABC", 3);

    u8 val;

    // Success reading
    TEST_ASSERT_EQUAL_INT(FIXED_BUF_OK, fixed_buf_read_u8(&fsb, &val));
    TEST_ASSERT_EQUAL_UINT('A', val);

    u8 r_buf[5];
    i32 n = fixed_buf_read(&fsb, r_buf, 2);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT('B', r_buf[0]);
    TEST_ASSERT_EQUAL_UINT('C', r_buf[1]);

    // Empty read
    TEST_ASSERT_EQUAL_INT(FIXED_BUF_ERR_EMPTY, fixed_buf_read_u8(&fsb, &val));
    TEST_ASSERT_EQUAL_INT(0, fixed_buf_read(&fsb, r_buf, 1));

    // Peek
    fixed_buf_reset_cursor(&fsb);
    TEST_ASSERT_EQUAL_INT(FIXED_BUF_OK, fixed_buf_peek(&fsb, &val));
    TEST_ASSERT_EQUAL_UINT('A', val);
    TEST_ASSERT_EQUAL_UINT('B', fixed_buf_peek_at(&fsb, 1));
    TEST_ASSERT_EQUAL_UINT('C', fixed_buf_peek_at(&fsb, 2));
    TEST_ASSERT_EQUAL_UINT(0, fixed_buf_peek_at(&fsb, 3)); // OOB

    // Peek empty
    fixed_buf_skip(&fsb, 10);
    TEST_ASSERT_EQUAL_INT(FIXED_BUF_ERR_EMPTY, fixed_buf_peek(&fsb, &val));
}

TEST_CASE(fixed_buf_test_write_ops)
{
    u8                  buf[5];
    fixed_buffer_t fsb;
    fixed_buf_init(&fsb, buf, 5);

    // Success append & partial write
    TEST_ASSERT_EQUAL_INT(3, fixed_buf_append(&fsb, (u8*)"123", 3));
    TEST_ASSERT_EQUAL_INT(2, fixed_buf_append(&fsb, (u8*)"456", 3)); // 45 written, 6 dropped
    TEST_ASSERT_EQUAL_UINT(5, fixed_buf_used(&fsb));

    // write_u8
    fixed_buf_write_u8(&fsb, 0, 'X');
    TEST_ASSERT_EQUAL_UINT('X', buf[0]);
    fixed_buf_write_u8(&fsb, 10, 'Y'); // OOB index, no crash

    // Write at cursor
    fixed_buf_reset_cursor(&fsb);
    fixed_buf_skip(&fsb, 2);
    TEST_ASSERT_EQUAL_INT(3, fixed_buf_write(&fsb, (u8*)"ABC", 3)); // Overwrite from index 2
    TEST_ASSERT_EQUAL_UINT('A', buf[2]);
    TEST_ASSERT_EQUAL_UINT(5, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT(5, fixed_buf_used(&fsb));

    // Write extending used
    fixed_buf_init(&fsb, buf, 5);
    fixed_buf_append(&fsb, (u8*)"123", 3);
    fixed_buf_reset_cursor(&fsb);
    fixed_buf_skip(&fsb, 2); // cursor at 2
    fixed_buf_write(&fsb, (u8*)"XY", 2); // Overwrite index 2,3. Index 3 is new.
    TEST_ASSERT_EQUAL_UINT(4, fixed_buf_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(4, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT('X', buf[2]);
    TEST_ASSERT_EQUAL_UINT('Y', buf[3]);

    // Partial write at cursor
    fixed_buf_reset_cursor(&fsb);
    fixed_buf_skip(&fsb, 4);
    TEST_ASSERT_EQUAL_INT(1, fixed_buf_write(&fsb, (u8*)"123", 3)); // Only 1 byte fits
    TEST_ASSERT_EQUAL_UINT(5, fixed_buf_used(&fsb));

    // Merge
    fixed_buffer_t other;
    u8                  o_buf[2] = { 0xAA, 0xBB };
    fixed_buf_init(&other, o_buf, 2);
    other.used = 2;

    fixed_buf_init(&fsb, buf, 5);
    TEST_ASSERT_EQUAL_INT(2, fixed_buf_merge(&fsb, &other));
    TEST_ASSERT_EQUAL_UINT(0xAA, buf[0]);
    TEST_ASSERT_EQUAL_UINT(0xBB, buf[1]);

    // Partial Merge
    fixed_buf_init(&fsb, buf, 2);
    TEST_ASSERT_EQUAL_INT(2, fixed_buf_merge(&fsb, &other));
    fixed_buf_init(&fsb, buf, 1);
    TEST_ASSERT_EQUAL_INT(1, fixed_buf_merge(&fsb, &other));
}

TEST_CASE(fixed_buf_test_management)
{
    u8                  buf[10];
    fixed_buffer_t fsb;
    fixed_buf_init(&fsb, buf, 10);
    fixed_buf_append(&fsb, (u8*)"0123456789", 10);

    // Flush No-op
    fixed_buf_reset_cursor(&fsb);
    fixed_buf_flush(&fsb); // cursor is 0
    TEST_ASSERT_EQUAL_UINT(10, fixed_buf_used(&fsb));

    // Normal Flush
    fixed_buf_skip(&fsb, 4); // cursor at 4
    fixed_buf_flush(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT(6, fixed_buf_used(&fsb));
    TEST_ASSERT_EQUAL_UINT('4', buf[0]);

    // Flush all
    fixed_buf_skip(&fsb, 6);
    fixed_buf_flush(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fixed_buf_used(&fsb));

    // New from cursor
    fixed_buf_append(&fsb, (u8*)"12345", 5);
    fixed_buf_skip(&fsb, 2);
    fixed_buffer_t sub;
    
    fixed_buf_new_from_cursor(&fsb, &sub);
    TEST_ASSERT_EQUAL_UINT(3, fixed_buf_used(&sub));
    TEST_ASSERT_EQUAL_UINT(3, fixed_buf_capacity(&sub));
    TEST_ASSERT_EQUAL_UINT(0, sub.cursor);
    TEST_ASSERT_EQUAL_UINT('3', sub.raw[0]);
}
#endif
