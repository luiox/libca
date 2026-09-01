#include "fixed_buffer.h"
#include <em_base/debug.h>
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
    }
    else {
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
    }
    else {
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

i32 fixed_buf_peek(fixed_buffer_t* self, u8* buf, usize size)
{
    param_check(self != NULL);
    param_check(buf != NULL);

    usize available = fixed_buf_remaining_to_read(self);
    if (available == 0) {
        return FIXED_BUF_ERR_EMPTY;
    }
    if (size > available) {
        return FIXED_BUF_ERR_OOB;
    }
    memcpy(buf, self->raw + self->cursor, size);
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
