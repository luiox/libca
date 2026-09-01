#include "ds_fixed_buffer.h"
#include "dstream.h"
#include "fixed_buffer.h"
#include <string.h>   // for memcpy


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
    fixed_buffer_t* buf       = (fixed_buffer_t*)self->buf_obj;
    usize           remaining = fixed_buf_remaining_to_read(buf);
    if (offset >= remaining || len == 0) {
        return 0;
    }
    usize available = remaining - offset;
    usize to_read   = (len > available) ? available : len;
    memcpy(dest, buf->raw + buf->cursor + offset, to_read);
    return (i32)to_read;
}

static inline i32 fixed_buf_dstream_write(dstream_t* self, const void* src, usize len)
{
    fixed_buffer_t* buf = (fixed_buffer_t*)self->buf_obj;
    return fixed_buf_write(buf, src, len);
}

dstream_ops_t g_fixed_buf_dstream_ops = {.capacity = fixed_buf_dstream_capacity,
                                         .used     = fixed_buf_dstream_used,
                                         .skip     = fixed_buf_dstream_skip,
                                         .rewind   = fixed_buf_dstream_rewind,
                                         .offset   = fixed_buf_dstream_offset,
                                         .reset    = fixed_buf_dstream_reset,
                                         .read     = fixed_buf_dstream_read,
                                         .peek     = fixed_buf_dstream_peek,
                                         .write    = fixed_buf_dstream_write};

const dstream_ops_t* fixed_buf_get_dstream_ops(void)
{
    return &g_fixed_buf_dstream_ops;
}
