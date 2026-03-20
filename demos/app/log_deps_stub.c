#include <em_dstream/ring_buffer.h>
#include <em_platform/async.h>
#include <em_platform/time_util.h>

void ring_buf_init(ring_buffer_t* rb, u8* buffer, usize size)
{
    if (!rb) {
        return;
    }
    rb->buffer = buffer;
    rb->size = size;
    rb->used = 0;
    rb->read = 0;
    rb->write = 0;
}

usize ring_buf_write(ring_buffer_t* rb, const u8* data, usize size)
{
    (void)rb;
    (void)data;
    return size;
}

usize ring_buf_read(ring_buffer_t* rb, u8* buf, usize size)
{
    (void)rb;
    (void)buf;
    return size;
}

usize ring_buf_peek(const ring_buffer_t* rb, u8* buf, usize size)
{
    (void)rb;
    (void)buf;
    return size;
}

usize ring_buf_skip(ring_buffer_t* rb, usize size)
{
    (void)rb;
    return size;
}

usize ring_buf_used(const ring_buffer_t* rb)
{
    return rb ? rb->used : 0;
}

usize ring_buf_free(const ring_buffer_t* rb)
{
    return rb ? (rb->size - rb->used) : 0;
}

bool async_submit(async_t* self, task_item_fn_t func, void* arg)
{
    (void)self;
    (void)func;
    (void)arg;
    return true;
}

u32 time_get_ms(void)
{
    return 0;
}

u32 time_get_us(void)
{
    return 0;
}
