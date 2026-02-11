#include "ring_buffer.h" // header renamed semantics: still same file but new declarations
#include "../em_base/debug.h"

/**
 * @brief 初始化环形缓冲区.
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区，要求是可用的内存，且大小为2的幂次方
 * @param size 缓冲区大小
 */
void ring_buf_init(ring_buffer_t* rb, uint8_t* buffer, usize size)
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
usize ring_buf_write(ring_buffer_t* rb, const uint8_t* data, usize size)
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
usize ring_buf_read(ring_buffer_t* rb, uint8_t* buf, usize size)
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
usize ring_buf_peek(const ring_buffer_t* rb, uint8_t* buf, usize size)
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

#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(ring_buf_basic)
{
    uint8_t      buf[16];
    ring_buffer_t rb;
    uint8_t      data_to_write[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t      data_to_read[4] = {0};

    ring_buf_init(&rb, buf, 16);

    // 测试初始状态
    TEST_ASSERT_EQUAL_INT(0, ring_buf_used(&rb));
    TEST_ASSERT_EQUAL_INT(16, ring_buf_free(&rb));

    // 测试写入
    usize written = ring_buf_write(&rb, data_to_write, 4);
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(4, ring_buf_used(&rb));
    TEST_ASSERT_EQUAL_INT(12, ring_buf_free(&rb));

    // 测试读取
    usize read = ring_buf_read(&rb, data_to_read, 4);
    TEST_ASSERT_EQUAL_INT(4, read);
    TEST_ASSERT_EQUAL_INT(0, ring_buf_used(&rb));
    TEST_ASSERT_EQUAL_INT(0x01, data_to_read[0]);
    TEST_ASSERT_EQUAL_INT(0x04, data_to_read[3]);
}

TEST_CASE(ring_buf_wrap_around)
{
    uint8_t      buf[8];
    ring_buffer_t rb;
    uint8_t      data1[] = {1, 2, 3, 4, 5, 6};
    uint8_t      data2[] = {7, 8};
    uint8_t      read_buf[8];

    ring_buf_init(&rb, buf, 8);

    // 写入6字节
    ring_buf_write(&rb, data1, 6);
    // 读取4字节，此时 read=4, write=6, used=2
    ring_buf_read(&rb, read_buf, 4);
    TEST_ASSERT_EQUAL_INT(2, ring_buf_used(&rb));

    // 再次写入4字节，会发生回环 (6+4=10, 10%8=2)
    usize written = ring_buf_write(&rb, data2, 4);
    // 剩余空间是 8-2=6，所以4字节应该能全部写进去
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(6, ring_buf_used(&rb));

    // 读取所有数据验证正确性
    ring_buf_read(&rb, read_buf, 6);
    TEST_ASSERT_EQUAL_INT(5, read_buf[0]);
    TEST_ASSERT_EQUAL_INT(6, read_buf[1]);
    TEST_ASSERT_EQUAL_INT(7, read_buf[2]);
    TEST_ASSERT_EQUAL_INT(8, read_buf[3]);
}

TEST_CASE(ring_buf_u8)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_u8(&rb, 0xAB);
    TEST_ASSERT_EQUAL_INT(0xAB, ring_buf_read_u8(&rb));
}

TEST_CASE(ring_buf_u16)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_u16(&rb, 0x1234);
    TEST_ASSERT_EQUAL_INT(0x1234, ring_buf_read_u16(&rb));
}

TEST_CASE(ring_buf_i16)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_i16(&rb, -1234);
    TEST_ASSERT_EQUAL_INT(-1234, ring_buf_read_i16(&rb));
}

TEST_CASE(ring_buf_u32)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_u32(&rb, 0x12345678);
    TEST_ASSERT_EQUAL_INT(0x12345678, ring_buf_read_u32(&rb));
}

TEST_CASE(ring_buf_i32)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_i32(&rb, -123456);
    TEST_ASSERT_EQUAL_INT(-123456, ring_buf_read_i32(&rb));
}

TEST_CASE(ring_buf_float)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    float f_in = 3.14159f;
    ring_buf_write_float(&rb, f_in);
    float f_out = ring_buf_read_float(&rb);
    
    if (fabs(f_out - f_in) > 0.00001f) {
        printf("Float test failed: expected %f, got %f\n", f_in, f_out);
    }
}

TEST_CASE(ring_buf_checksum)
{
    u8           mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    u8 data[] = {1, 2, 3, 4};
    ring_buf_write(&rb, data, 4);
    TEST_ASSERT_EQUAL_INT(10, ring_buf_calculate_checksum(&rb));
}

#endif
