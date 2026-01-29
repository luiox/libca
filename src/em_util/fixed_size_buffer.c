#include "fixed_size_buffer.h"
#include <string.h>

void fsb_init(fixed_size_buffer_t* self, u8* data, usize capacity)
{
    if (self == NULL || data == NULL) {
        return;
    }
    self->raw      = data;
    self->capacity = capacity;
    self->used     = 0;
    self->cursor   = 0;
}

void fsb_skip(fixed_size_buffer_t* self, usize size)
{
    if (self == NULL) {
        return;
    }
    self->cursor += size;
    if (self->cursor > self->used) {
        self->cursor = self->used;
    }
}

void fsb_rewind(fixed_size_buffer_t* self, usize size)
{
    if (self == NULL) {
        return;
    }
    if (size > self->cursor) {
        self->cursor = 0;
    } else {
        self->cursor -= size;
    }
}

void fsb_flush(fixed_size_buffer_t* self)
{
    if (self == NULL || self->cursor == 0) {
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
    if (self == NULL || new_b == NULL) {
        return;
    }
    usize remaining = fsb_remaining_to_read(self);
    new_b->raw      = self->raw + self->cursor;
    new_b->capacity = remaining;
    new_b->used     = remaining;
    new_b->cursor   = 0;
}

i32 fsb_read_u8(fixed_size_buffer_t* self, u8* value)
{
    if (self == NULL || value == NULL) {
        return FSB_ERR_INVALID;
    }
    if (self->cursor >= self->used) {
        return FSB_ERR_EMPTY;
    }
    *value = self->raw[self->cursor++];
    return FSB_OK;
}

i32 fsb_read(fixed_size_buffer_t* self, u8* buffer, usize size)
{
    if (self == NULL || buffer == NULL) {
        return FSB_ERR_INVALID;
    }
    usize available = fsb_remaining_to_read(self);
    usize to_read   = (size > available) ? available : size;
    if (to_read > 0) {
        memcpy(buffer, self->raw + self->cursor, to_read);
        self->cursor += to_read;
    }
    return (i32)to_read;
}

u8 fsb_peek(fixed_size_buffer_t* self)
{
    if (self == NULL || self->cursor >= self->used) {
        return 0;
    }
    return self->raw[self->cursor];
}

u8 fsb_peek_at(fixed_size_buffer_t* self, usize offset)
{
    if (self == NULL || (self->cursor + offset) >= self->used) {
        return 0;
    }
    return self->raw[self->cursor + offset];
}

i32 fsb_append(fixed_size_buffer_t* self, const u8* data, usize size)
{
    if (self == NULL || data == NULL) {
        return FSB_ERR_INVALID;
    }
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
    if (self == NULL || other == NULL) {
        return FSB_ERR_INVALID;
    }
    return fsb_append(self, other->raw, other->used);
}

void fsb_write_u8(fixed_size_buffer_t* self, usize index, u8 value)
{
    if (self == NULL || index >= self->capacity) {
        return;
    }
    self->raw[index] = value;
}

i32 fsb_write(fixed_size_buffer_t* self, const u8* data, usize size)
{
    if (self == NULL || data == NULL) {
        return FSB_ERR_INVALID;
    }
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

    // NULL check
    fsb_init(NULL, buf, 10);
    fsb_init(&fsb, NULL, 10);

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

    // Skip & Rewind on NULL
    fsb_skip(NULL, 1);
    fsb_rewind(NULL, 1);

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
    // NULL checks
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_read_u8(NULL, &val));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_read_u8(&fsb, NULL));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_read(NULL, buf, 1));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_read(&fsb, NULL, 1));

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
    TEST_ASSERT_EQUAL_UINT('A', fsb_peek(&fsb));
    TEST_ASSERT_EQUAL_UINT('B', fsb_peek_at(&fsb, 1));
    TEST_ASSERT_EQUAL_UINT('C', fsb_peek_at(&fsb, 2));
    TEST_ASSERT_EQUAL_UINT(0, fsb_peek_at(&fsb, 3)); // OOB

    // Peek NULL/empty
    TEST_ASSERT_EQUAL_UINT(0, fsb_peek(NULL));
    TEST_ASSERT_EQUAL_UINT(0, fsb_peek_at(NULL, 0));
    fsb_skip(&fsb, 10);
    TEST_ASSERT_EQUAL_UINT(0, fsb_peek(&fsb));
}

TEST_CASE(fsb_test_write_ops)
{
    u8                  buf[5];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 5);

    // NULL Checks
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_append(NULL, (u8*)"1", 1));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_append(&fsb, NULL, 1));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_write(NULL, (u8*)"1", 1));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_write(&fsb, NULL, 1));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_merge(NULL, &fsb));
    TEST_ASSERT_EQUAL_INT(FSB_ERR_INVALID, fsb_merge(&fsb, NULL));
    fsb_write_u8(NULL, 0, 0); // No crash side effect

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

    // Flush NULL/No-op
    fsb_flush(NULL);
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
    // NULL check
    fsb_new_from_cursor(NULL, &sub);
    fsb_new_from_cursor(&fsb, NULL);
    
    fsb_new_from_cursor(&fsb, &sub);
    TEST_ASSERT_EQUAL_UINT(3, fsb_used(&sub));
    TEST_ASSERT_EQUAL_UINT(3, fsb_capacity(&sub));
    TEST_ASSERT_EQUAL_UINT(0, sub.cursor);
    TEST_ASSERT_EQUAL_UINT('3', sub.raw[0]);
}
#endif
