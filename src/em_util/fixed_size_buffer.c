#include "fixed_size_buffer.h"
#include "debug.h"
#include <string.h>

void fsb_init(fixed_size_buffer_t* self, u8* data, usize capacity)
{
    param_check(self != NULL);
    param_check(data != NULL);

    self->raw      = data;
    self->capacity = capacity;
    self->used     = 0;
    self->cursor   = 0;
}

void fsb_skip(fixed_size_buffer_t* self, usize size)
{
    param_check(self != NULL);

    self->cursor += size;
    if (self->cursor > self->used) {
        self->cursor = self->used;
    }
}

void fsb_rewind(fixed_size_buffer_t* self, usize size)
{
    param_check(self != NULL);

    if (size > self->cursor) {
        self->cursor = 0;
    } else {
        self->cursor -= size;
    }
}

void fsb_flush(fixed_size_buffer_t* self)
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

void fsb_new_from_cursor(fixed_size_buffer_t* self, fixed_size_buffer_t* new_b)
{
    param_check(self != NULL);
    param_check(new_b != NULL);

    usize remaining = fsb_remaining_to_read(self);
    new_b->raw      = self->raw + self->cursor;
    new_b->capacity = remaining;
    new_b->used     = remaining;
    new_b->cursor   = 0;
}

i32 fsb_read_u8(fixed_size_buffer_t* self, u8* value)
{
    param_check(self != NULL);
    param_check(value != NULL);

    if (self->cursor >= self->used) {
        return FSB_ERR_EMPTY;
    }
    *value = self->raw[self->cursor++];
    return FSB_OK;
}

i32 fsb_read(fixed_size_buffer_t* self, u8* buffer, usize size)
{
    param_check(self != NULL);
    param_check(buffer != NULL);

    usize available = fsb_remaining_to_read(self);
    usize to_read   = (size > available) ? available : size;
    if (to_read > 0) {
        memcpy(buffer, self->raw + self->cursor, to_read);
        self->cursor += to_read;
    }
    return (i32)to_read;
}

i32 fsb_peek(fixed_size_buffer_t* self, u8* value)
{
    param_check(self != NULL);
    param_check(value != NULL);

    if (self->cursor >= self->used) {
        return FSB_ERR_EMPTY;
    }
    *value = self->raw[self->cursor];
    return FSB_OK;
}

u8 fsb_peek_at(fixed_size_buffer_t* self, usize offset)
{
    param_check(self != NULL);

    // 防止offset过大导致self->cursor + offset可能回绕，
    // 所以不能用 (self->cursor + offset) >= self->used方式处理
    if (offset >= fsb_remaining_to_read(self)) {
        return 0;
    }
    return self->raw[self->cursor + offset];
}

i32 fsb_append(fixed_size_buffer_t* self, const u8* data, usize size)
{
    param_check(self != NULL);
    param_check(data != NULL);

    usize available = fsb_available(self);
    usize to_write  = (size > available) ? available : size;
    if (to_write > 0) {
        memcpy(self->raw + self->used, data, to_write);
        self->used += to_write;
    }
    return (i32)to_write;
}

i32 fsb_merge(fixed_size_buffer_t* self, const fixed_size_buffer_t* other)
{
    param_check(self != NULL);
    param_check(other != NULL);

    return fsb_append(self, other->raw, other->used);
}

void fsb_write_u8(fixed_size_buffer_t* self, usize index, u8 value)
{
    param_check(self != NULL);

    if (index >= self->capacity) {
        return;
    }
    self->raw[index] = value;
}

i32 fsb_write(fixed_size_buffer_t* self, const u8* data, usize size)
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

TEST_CASE(fsb_test_init)
{
    u8                  buf[10];
    fixed_size_buffer_t fsb;

    // Success
    fsb_init(&fsb, buf, 10);
    TEST_ASSERT_EQUAL_UINT(10, fsb_capacity(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
}

TEST_CASE(fsb_test_cursor_ops)
{
    u8                  buf[10];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 10);

    // Normal skip
    fsb_append(&fsb, (u8*)"abc", 3);
    fsb_skip(&fsb, 1);
    TEST_ASSERT_EQUAL_UINT(1, fsb.cursor);

    // Overshoot skip (capped at used)
    fsb_skip(&fsb, 10);
    TEST_ASSERT_EQUAL_UINT(3, fsb.cursor);

    // Normal rewind
    fsb_rewind(&fsb, 1);
    TEST_ASSERT_EQUAL_UINT(2, fsb.cursor);

    // Undershoot rewind (capped at 0)
    fsb_rewind(&fsb, 10);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);

    // Reset
    fsb_skip(&fsb, 2);
    fsb_reset_cursor(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
}

TEST_CASE(fsb_test_state_info)
{
    u8                  buf[10];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 10);

    TEST_ASSERT_EQUAL_UINT(10, fsb_capacity(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(10, fsb_available(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb_remaining_to_read(&fsb));
    TEST_ASSERT(fsb_data(&fsb) == buf);

    fsb_append(&fsb, (u8*)"12345", 5);
    TEST_ASSERT_EQUAL_UINT(5, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(5, fsb_available(&fsb));
    TEST_ASSERT_EQUAL_UINT(5, fsb_remaining_to_read(&fsb));

    fsb_skip(&fsb, 2);
    TEST_ASSERT_EQUAL_UINT(3, fsb_remaining_to_read(&fsb));

    fsb_skip(&fsb, 10); // cursor at used
    TEST_ASSERT_EQUAL_UINT(0, fsb_remaining_to_read(&fsb));
}

TEST_CASE(fsb_test_read_ops)
{
    u8                  buf[10];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 10);
    fsb_append(&fsb, (u8*)"ABC", 3);

    u8 val;

    // Success reading
    TEST_ASSERT_EQUAL_INT(FSB_OK, fsb_read_u8(&fsb, &val));
    TEST_ASSERT_EQUAL_UINT('A', val);

    u8 r_buf[5];
    i32 n = fsb_read(&fsb, r_buf, 2);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT('B', r_buf[0]);
    TEST_ASSERT_EQUAL_UINT('C', r_buf[1]);

    // Empty read
    TEST_ASSERT_EQUAL_INT(FSB_ERR_EMPTY, fsb_read_u8(&fsb, &val));
    TEST_ASSERT_EQUAL_INT(0, fsb_read(&fsb, r_buf, 1));

    // Peek
    fsb_reset_cursor(&fsb);
    TEST_ASSERT_EQUAL_INT(FSB_OK, fsb_peek(&fsb, &val));
    TEST_ASSERT_EQUAL_UINT('A', val);
    TEST_ASSERT_EQUAL_UINT('B', fsb_peek_at(&fsb, 1));
    TEST_ASSERT_EQUAL_UINT('C', fsb_peek_at(&fsb, 2));
    TEST_ASSERT_EQUAL_UINT(0, fsb_peek_at(&fsb, 3)); // OOB

    // Peek empty
    fsb_skip(&fsb, 10);
    TEST_ASSERT_EQUAL_INT(FSB_ERR_EMPTY, fsb_peek(&fsb, &val));
}

TEST_CASE(fsb_test_write_ops)
{
    u8                  buf[5];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 5);

    // Success append & partial write
    TEST_ASSERT_EQUAL_INT(3, fsb_append(&fsb, (u8*)"123", 3));
    TEST_ASSERT_EQUAL_INT(2, fsb_append(&fsb, (u8*)"456", 3)); // 45 written, 6 dropped
    TEST_ASSERT_EQUAL_UINT(5, fsb_used(&fsb));

    // write_u8
    fsb_write_u8(&fsb, 0, 'X');
    TEST_ASSERT_EQUAL_UINT('X', buf[0]);
    fsb_write_u8(&fsb, 10, 'Y'); // OOB index, no crash

    // Write at cursor
    fsb_reset_cursor(&fsb);
    fsb_skip(&fsb, 2);
    TEST_ASSERT_EQUAL_INT(3, fsb_write(&fsb, (u8*)"ABC", 3)); // Overwrite from index 2
    TEST_ASSERT_EQUAL_UINT('A', buf[2]);
    TEST_ASSERT_EQUAL_UINT(5, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT(5, fsb_used(&fsb));

    // Write extending used
    fsb_init(&fsb, buf, 5);
    fsb_append(&fsb, (u8*)"123", 3);
    fsb_reset_cursor(&fsb);
    fsb_skip(&fsb, 2); // cursor at 2
    fsb_write(&fsb, (u8*)"XY", 2); // Overwrite index 2,3. Index 3 is new.
    TEST_ASSERT_EQUAL_UINT(4, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(4, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT('X', buf[2]);
    TEST_ASSERT_EQUAL_UINT('Y', buf[3]);

    // Partial write at cursor
    fsb_reset_cursor(&fsb);
    fsb_skip(&fsb, 4);
    TEST_ASSERT_EQUAL_INT(1, fsb_write(&fsb, (u8*)"123", 3)); // Only 1 byte fits
    TEST_ASSERT_EQUAL_UINT(5, fsb_used(&fsb));

    // Merge
    fixed_size_buffer_t other;
    u8                  o_buf[2] = { 0xAA, 0xBB };
    fsb_init(&other, o_buf, 2);
    other.used = 2;

    fsb_init(&fsb, buf, 5);
    TEST_ASSERT_EQUAL_INT(2, fsb_merge(&fsb, &other));
    TEST_ASSERT_EQUAL_UINT(0xAA, buf[0]);
    TEST_ASSERT_EQUAL_UINT(0xBB, buf[1]);

    // Partial Merge
    fsb_init(&fsb, buf, 2);
    TEST_ASSERT_EQUAL_INT(2, fsb_merge(&fsb, &other));
    fsb_init(&fsb, buf, 1);
    TEST_ASSERT_EQUAL_INT(1, fsb_merge(&fsb, &other));
}

TEST_CASE(fsb_test_management)
{
    u8                  buf[10];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 10);
    fsb_append(&fsb, (u8*)"0123456789", 10);

    // Flush No-op
    fsb_reset_cursor(&fsb);
    fsb_flush(&fsb); // cursor is 0
    TEST_ASSERT_EQUAL_UINT(10, fsb_used(&fsb));

    // Normal Flush
    fsb_skip(&fsb, 4); // cursor at 4
    fsb_flush(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT(6, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT('4', buf[0]);

    // Flush all
    fsb_skip(&fsb, 6);
    fsb_flush(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fsb_used(&fsb));

    // New from cursor
    fsb_append(&fsb, (u8*)"12345", 5);
    fsb_skip(&fsb, 2);
    fixed_size_buffer_t sub;
    
    fsb_new_from_cursor(&fsb, &sub);
    TEST_ASSERT_EQUAL_UINT(3, fsb_used(&sub));
    TEST_ASSERT_EQUAL_UINT(3, fsb_capacity(&sub));
    TEST_ASSERT_EQUAL_UINT(0, sub.cursor);
    TEST_ASSERT_EQUAL_UINT('3', sub.raw[0]);
}
#endif
