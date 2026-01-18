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

// 工具函数

u8 ringbuffer_read_u8(ringbuffer_t* rb) {
    u8 val = 0;
    ringbuffer_read(rb, &val, 1);
    return val;
}

void ringbuffer_write_u8(ringbuffer_t* rb, u8 value) {
    ringbuffer_write(rb, &value, 1);
}

i16 ringbuffer_read_i16(ringbuffer_t* rb) {
    i16 val = 0;
    ringbuffer_read(rb, (u8*)&val, sizeof(i16));
    return val;
}

u16 ringbuffer_read_u16(ringbuffer_t* rb) {
    u16 val = 0;
    ringbuffer_read(rb, (u8*)&val, sizeof(u16));
    return val;
}

void ringbuffer_write_i16(ringbuffer_t* rb, i16 value) {
    ringbuffer_write(rb, (const u8*)&value, sizeof(i16));
}

void ringbuffer_write_u16(ringbuffer_t* rb, u16 value) {
    ringbuffer_write(rb, (const u8*)&value, sizeof(u16));
}

i32 ringbuffer_read_i32(ringbuffer_t* rb) {
    i32 val = 0;
    ringbuffer_read(rb, (u8*)&val, sizeof(i32));
    return val;
}

u32 ringbuffer_read_u32(ringbuffer_t* rb) {
    u32 val = 0;
    ringbuffer_read(rb, (u8*)&val, sizeof(u32));
    return val;
}

void ringbuffer_write_i32(ringbuffer_t* rb, i32 value) {
    ringbuffer_write(rb, (const u8*)&value, sizeof(i32));
}

void ringbuffer_write_u32(ringbuffer_t* rb, u32 value) {
    ringbuffer_write(rb, (const u8*)&value, sizeof(u32));
}

float ringbuffer_read_float(ringbuffer_t* rb) {
    float val = 0.0f;
    ringbuffer_read(rb, (u8*)&val, sizeof(float));
    return val;
}

void ringbuffer_write_float(ringbuffer_t* rb, float value) {
    ringbuffer_write(rb, (const u8*)&value, sizeof(float));
}

u8 ringbuffer_calculate_checksum(const ringbuffer_t* rb) {
    u8 checksum = 0;
    position_size_t used = ringbuffer_used(rb);
    if (used == 0) return 0;

    position_size_t read_ptr = rb->read;
    for (position_size_t i = 0; i < used; i++) {
        checksum += rb->buffer[read_ptr];
        read_ptr = (read_ptr + 1) & (rb->size - 1);
    }
    return checksum;
}

#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(ringbuffer_basic)
{
    uint8_t      buf[16];
    ringbuffer_t rb;
    uint8_t      data_to_write[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t      data_to_read[4] = {0};

    ringbuffer_init(&rb, buf, 16);

    // 测试初始状态
    TEST_ASSERT_EQUAL_INT(0, ringbuffer_used(&rb));
    TEST_ASSERT_EQUAL_INT(16, ringbuffer_free(&rb));

    // 测试写入
    position_size_t written = ringbuffer_write(&rb, data_to_write, 4);
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(4, ringbuffer_used(&rb));
    TEST_ASSERT_EQUAL_INT(12, ringbuffer_free(&rb));

    // 测试读取
    position_size_t read = ringbuffer_read(&rb, data_to_read, 4);
    TEST_ASSERT_EQUAL_INT(4, read);
    TEST_ASSERT_EQUAL_INT(0, ringbuffer_used(&rb));
    TEST_ASSERT_EQUAL_INT(0x01, data_to_read[0]);
    TEST_ASSERT_EQUAL_INT(0x04, data_to_read[3]);
}

TEST_CASE(ringbuffer_wrap_around)
{
    uint8_t      buf[8];
    ringbuffer_t rb;
    uint8_t      data1[] = {1, 2, 3, 4, 5, 6};
    uint8_t      data2[] = {7, 8};
    uint8_t      read_buf[8];

    ringbuffer_init(&rb, buf, 8);

    // 写入6字节
    ringbuffer_write(&rb, data1, 6);
    // 读取4字节，此时 read=4, write=6, used=2
    ringbuffer_read(&rb, read_buf, 4);
    TEST_ASSERT_EQUAL_INT(2, ringbuffer_used(&rb));

    // 再次写入4字节，会发生回环 (6+4=10, 10%8=2)
    position_size_t written = ringbuffer_write(&rb, data2, 4);
    // 剩余空间是 8-2=6，所以4字节应该能全部写进去
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(6, ringbuffer_used(&rb));

    // 读取所有数据验证正确性
    ringbuffer_read(&rb, read_buf, 6);
    TEST_ASSERT_EQUAL_INT(5, read_buf[0]);
    TEST_ASSERT_EQUAL_INT(6, read_buf[1]);
    TEST_ASSERT_EQUAL_INT(7, read_buf[2]);
    TEST_ASSERT_EQUAL_INT(8, read_buf[3]);
}

TEST_CASE(ringbuffer_u8)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_u8(&rb, 0xAB);
    TEST_ASSERT_EQUAL_INT(0xAB, ringbuffer_read_u8(&rb));
}

TEST_CASE(ringbuffer_u16)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_u16(&rb, 0x1234);
    TEST_ASSERT_EQUAL_INT(0x1234, ringbuffer_read_u16(&rb));
}

TEST_CASE(ringbuffer_i16)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_i16(&rb, -1234);
    TEST_ASSERT_EQUAL_INT(-1234, ringbuffer_read_i16(&rb));
}

TEST_CASE(ringbuffer_u32)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_u32(&rb, 0x12345678);
    TEST_ASSERT_EQUAL_INT(0x12345678, ringbuffer_read_u32(&rb));
}

TEST_CASE(ringbuffer_i32)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_i32(&rb, -123456);
    TEST_ASSERT_EQUAL_INT(-123456, ringbuffer_read_i32(&rb));
}

TEST_CASE(ringbuffer_float)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    float f_in = 3.14159f;
    ringbuffer_write_float(&rb, f_in);
    float f_out = ringbuffer_read_float(&rb);
    
    if (fabs(f_out - f_in) > 0.00001f) {
        printf("Float test failed: expected %f, got %f\n", f_in, f_out);
    }
}

TEST_CASE(ringbuffer_checksum)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    u8 data[] = {1, 2, 3, 4};
    ringbuffer_write(&rb, data, 4);
    TEST_ASSERT_EQUAL_INT(10, ringbuffer_calculate_checksum(&rb));
}

#endif
