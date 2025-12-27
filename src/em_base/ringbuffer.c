#include "ringbuffer.h"
#include <assert.h>

/**
 * @brief 初始化环形缓冲区.
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区，要求是可用的内存，且大小为2的幂次方
 * @param size 缓冲区大小
 */
void ringbuffer_init(ringbuffer_t* rb, uint8_t* buffer, position_size_t size)
{
    assert(rb);
    assert(buffer);
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
void ringbuffer_reset(ringbuffer_t* rb)
{
    assert(rb);
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
position_size_t ringbuffer_write(ringbuffer_t* rb, const uint8_t* data, position_size_t size)
{
    assert(rb);
    assert(data);
    
    position_size_t free_size = ringbuffer_free(rb);
    if (size > free_size) {
        size = free_size;
    }

    // 写入数据
    for (position_size_t i = 0; i < size; i++) {
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
position_size_t ringbuffer_read(ringbuffer_t* rb, uint8_t* buf, position_size_t size)
{
    assert(rb);
    assert(buf);

    position_size_t used_size = ringbuffer_used(rb);
    if (size > used_size) {
        size = used_size;
    }

    // 读取数据
    for (position_size_t i = 0; i < size; i++) {
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
position_size_t ringbuffer_peek(const ringbuffer_t* rb, uint8_t* buf, position_size_t size)
{
    assert(rb);
    assert(buf);

    position_size_t used_size = ringbuffer_used(rb);
    if (size > used_size) {
        size = used_size;
    }

    position_size_t read_ptr = rb->read;
    for (position_size_t i = 0; i < size; i++) {
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
position_size_t ringbuffer_skip(ringbuffer_t* rb, position_size_t size)
{
    assert(rb);
    
    position_size_t used_size = ringbuffer_used(rb);
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
position_size_t ringbuffer_used(const ringbuffer_t* rb)
{
    assert(rb);
    return rb->used;
}

/**
 * @brief 获取环形缓冲区里的空闲大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 空闲大小
 */
position_size_t ringbuffer_free(const ringbuffer_t* rb)
{
    assert(rb);
    return rb->size - rb->used;
}
