#include "ringbuffer_util.h"
#include <stdlib.h>
#include <string.h>

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

TEST_CASE(ringbuffer_util_u8)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_u8(&rb, 0xAB);
    TEST_ASSERT_EQUAL_INT(0xAB, ringbuffer_read_u8(&rb));
}

TEST_CASE(ringbuffer_util_u16)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_u16(&rb, 0x1234);
    TEST_ASSERT_EQUAL_INT(0x1234, ringbuffer_read_u16(&rb));
}

TEST_CASE(ringbuffer_util_i16)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_i16(&rb, -1234);
    TEST_ASSERT_EQUAL_INT(-1234, ringbuffer_read_i16(&rb));
}

TEST_CASE(ringbuffer_util_u32)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_u32(&rb, 0x12345678);
    TEST_ASSERT_EQUAL_INT(0x12345678, ringbuffer_read_u32(&rb));
}

TEST_CASE(ringbuffer_util_i32)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    ringbuffer_write_i32(&rb, -123456);
    TEST_ASSERT_EQUAL_INT(-123456, ringbuffer_read_i32(&rb));
}

TEST_CASE(ringbuffer_util_float)
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

TEST_CASE(ringbuffer_util_checksum)
{
    u8           mem[64];
    ringbuffer_t rb;
    ringbuffer_init(&rb, mem, 64);

    u8 data[] = {1, 2, 3, 4};
    ringbuffer_write(&rb, data, 4);
    TEST_ASSERT_EQUAL_INT(10, ringbuffer_calculate_checksum(&rb));
}

#endif
