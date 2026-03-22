#include "ring_buffer.h" // header renamed semantics: still same file but new declarations
#include <em_base/debug.h>

/**
 * @brief 初始化环形缓冲区.
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区，要求是可用的内存，且大小为2的幂次方
 * @param size 缓冲区大小
 */
void ring_buf_init(ring_buffer_t* rb, u8* buffer, usize size)
{
    param_check(rb);
    param_check(buffer);
    rb->buffer = buffer;
    rb->size   = size;
    rb->used   = 0;
    rb->read   = 0;
    rb->write  = 0;
}

/**
 * @brief 重置环形缓冲区.
 * @param rb 环形缓冲区指针
 */
void ring_buf_reset(ring_buffer_t* rb)
{
    param_check(rb);
    rb->read  = 0;
    rb->write = 0;
    rb->used  = 0;
}

/**
 * @brief 往环形缓冲区里写数据.
 * @param rb 环形缓冲区指针
 * @param data 指向数据的指针
 * @param size 期望写入的数据大小
 * @return position_size_t 实际写入的数据大小
 */
usize ring_buf_write(ring_buffer_t* rb, const u8* data, usize size)
{
    param_check(rb);
    param_check(data);
    
    usize free_size = ring_buf_free(rb);
    if (size > free_size) {
        size = free_size;
    }

    // 写入数据
    for (usize i = 0; i < size; i++) {
        rb->buffer[rb->write] = data[i];
        rb->write = (rb->write + 1) & (rb->size - 1);
    }
    rb->used += size;
    return size;
}

/**
 * @brief 从环形缓冲区里读数据.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 期望读取的数据大小
 * @return position_size_t 实际读取的数据大小
 */
usize ring_buf_read(ring_buffer_t* rb, u8* buf, usize size)
{
    param_check(rb);
    param_check(buf);

    usize used_size = ring_buf_used(rb);
    if (size > used_size) {
        size = used_size;
    }

    // 读取数据
    for (usize i = 0; i < size; i++) {
        buf[i] = rb->buffer[rb->read];
        rb->read = (rb->read + 1) & (rb->size - 1);
    }
    rb->used -= size;
    return size;
}

/**
 * @brief 预览环形缓冲区里的数据（不弹出）.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 期望预览的数据大小
 * @return position_size_t 实际预览的数据大小
 */
usize ring_buf_peek(const ring_buffer_t* rb, u8* buf, usize size)
{
    param_check(rb);
    param_check(buf);

    usize used_size = ring_buf_used(rb);
    if (size > used_size) {
        size = used_size;
    }

    usize read_ptr = rb->read;
    for (usize i = 0; i < size; i++) {
        buf[i] = rb->buffer[read_ptr];
        read_ptr = (read_ptr + 1) & (rb->size - 1);
    }
    return size;
}

/**
 * @brief 跳过（丢弃）环形缓冲区里的数据.
 * @param rb 环形缓冲区指针
 * @param size 期望跳过的数据大小
 * @return position_size_t 实际跳过的数据大小
 */
usize ring_buf_skip(ring_buffer_t* rb, usize size)
{
    param_check(rb);
    
    usize used_size = ring_buf_used(rb);
    if (size > used_size) {
        size = used_size;
    }

    rb->read = (rb->read + size) & (rb->size - 1);
    rb->used -= size;
    return size;
}

/**
 * @brief 获取环形缓冲区里的数据大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 已使用的大小
 */
usize ring_buf_used(const ring_buffer_t* rb)
{
    param_check(rb);
    return rb->used;
}

/**
 * @brief 获取环形缓冲区里的空闲大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 空闲大小
 */
usize ring_buf_free(const ring_buffer_t* rb)
{
    param_check(rb);
    return rb->size - rb->used;
}

// 工具函数

u8 ring_buf_read_u8(ring_buffer_t* rb) {
    u8 val = 0;
    ring_buf_read(rb, &val, 1);
    return val;
}

void ring_buf_write_u8(ring_buffer_t* rb, u8 value) {
    ring_buf_write(rb, &value, 1);
}

i16 ring_buf_read_i16(ring_buffer_t* rb) {
    i16 val = 0;
    ring_buf_read(rb, (u8*)&val, sizeof(i16));
    return val;
}

u16 ring_buf_read_u16(ring_buffer_t* rb) {
    u16 val = 0;
    ring_buf_read(rb, (u8*)&val, sizeof(u16));
    return val;
}

void ring_buf_write_i16(ring_buffer_t* rb, i16 value) {
    ring_buf_write(rb, (const u8*)&value, sizeof(i16));
}

void ring_buf_write_u16(ring_buffer_t* rb, u16 value) {
    ring_buf_write(rb, (const u8*)&value, sizeof(u16));
}

i32 ring_buf_read_i32(ring_buffer_t* rb) {
    i32 val = 0;
    ring_buf_read(rb, (u8*)&val, sizeof(i32));
    return val;
}

u32 ring_buf_read_u32(ring_buffer_t* rb) {
    u32 val = 0;
    ring_buf_read(rb, (u8*)&val, sizeof(u32));
    return val;
}

void ring_buf_write_i32(ring_buffer_t* rb, i32 value) {
    ring_buf_write(rb, (const u8*)&value, sizeof(i32));
}

void ring_buf_write_u32(ring_buffer_t* rb, u32 value) {
    ring_buf_write(rb, (const u8*)&value, sizeof(u32));
}

float ring_buf_read_float(ring_buffer_t* rb) {
    float val = 0.0f;
    ring_buf_read(rb, (u8*)&val, sizeof(float));
    return val;
}

void ring_buf_write_float(ring_buffer_t* rb, float value) {
    ring_buf_write(rb, (const u8*)&value, sizeof(float));
}

u8 ring_buf_calculate_checksum(const ring_buffer_t* rb) {
    u8 checksum = 0;
    usize used = ring_buf_used(rb);
    if (used == 0) return 0;

    usize read_ptr = rb->read;
    for (usize i = 0; i < used; i++) {
        checksum += rb->buffer[read_ptr];
        read_ptr = (read_ptr + 1) & (rb->size - 1);
    }
    return checksum;
}

