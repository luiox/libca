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

TEST_CASE(fsb_basic_ops)
{
    u8                  buf[20];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 20);

    TEST_ASSERT_EQUAL_UINT(20, fsb_capacity(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);

    // Append
    u8  data1[] = { 0x01, 0x02, 0x03 };
    i32 written = fsb_append(&fsb, data1, 3);
    TEST_ASSERT_EQUAL_INT(3, written);
    TEST_ASSERT_EQUAL_UINT(3, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);

    // Read
    u8  val;
    i32 res = fsb_read_u8(&fsb, &val);
    TEST_ASSERT_EQUAL_INT(FSB_OK, res);
    TEST_ASSERT_EQUAL_UINT(0x01, val);
    TEST_ASSERT_EQUAL_UINT(1, fsb.cursor);

    // Peek
    TEST_ASSERT_EQUAL_UINT(0x02, fsb_peek(&fsb));
    TEST_ASSERT_EQUAL_UINT(0x03, fsb_peek_at(&fsb, 1));

    // Skip
    fsb_skip(&fsb, 1);
    TEST_ASSERT_EQUAL_UINT(2, fsb.cursor);
    fsb_read_u8(&fsb, &val);
    TEST_ASSERT_EQUAL_UINT(0x03, val);

    // Flush
    fsb_reset_cursor(&fsb);
    fsb_skip(&fsb, 1);   // cursor at 1, data [1, 3) is {02, 03}
    fsb_flush(&fsb);
    TEST_ASSERT_EQUAL_UINT(0, fsb.cursor);
    TEST_ASSERT_EQUAL_UINT(2, fsb_used(&fsb));
    TEST_ASSERT_EQUAL_UINT(0x02, buf[0]);
    TEST_ASSERT_EQUAL_UINT(0x03, buf[1]);
}

TEST_CASE(fsb_overflow_and_boundary)
{
    u8                  buf[5];
    fixed_size_buffer_t fsb;
    fsb_init(&fsb, buf, 5);

    u8  data[]  = { 1, 2, 3, 4, 5, 6 };
    i32 written = fsb_append(&fsb, data, 6);
    TEST_ASSERT_EQUAL_INT(5, written);   // Partial write
    TEST_ASSERT_EQUAL_UINT(5, fsb_used(&fsb));

    fsb_skip(&fsb, 10);
    TEST_ASSERT_EQUAL_UINT(5, fsb.cursor);   // Capped at used

    u8 val;
    TEST_ASSERT_EQUAL_INT(FSB_ERR_EMPTY, fsb_read_u8(&fsb, &val));

    fsb_reset_cursor(&fsb);
    u8  r_buf[10];
    i32 read_bytes = fsb_read(&fsb, r_buf, 10);
    TEST_ASSERT_EQUAL_INT(5, read_bytes);
}
#endif
