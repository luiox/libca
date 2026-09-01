/* Auto-migrated from src/em_dstream/ring_buffer.c test blocks */
#include "ring_buffer.h"   // header renamed semantics: still same file but new declarations
#include <em_base/debug.h>


#include <em_test/test.h>

TEST_CASE(ring_buf_basic)
{
    u8            buf[16];
    ring_buffer_t rb;
    u8            data_to_write[] = {0x01, 0x02, 0x03, 0x04};
    u8            data_to_read[4] = {0};

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
    u8            buf[8];
    ring_buffer_t rb;
    u8            data1[] = {1, 2, 3, 4, 5, 6};
    u8            data2[] = {7, 8};
    u8            read_buf[8];

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
    u8            mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_u8(&rb, 0xAB);
    TEST_ASSERT_EQUAL_INT(0xAB, ring_buf_read_u8(&rb));
}

TEST_CASE(ring_buf_u16)
{
    u8            mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_u16(&rb, 0x1234);
    TEST_ASSERT_EQUAL_INT(0x1234, ring_buf_read_u16(&rb));
}

TEST_CASE(ring_buf_i16)
{
    u8            mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_i16(&rb, -1234);
    TEST_ASSERT_EQUAL_INT(-1234, ring_buf_read_i16(&rb));
}

TEST_CASE(ring_buf_u32)
{
    u8            mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_u32(&rb, 0x12345678);
    TEST_ASSERT_EQUAL_INT(0x12345678, ring_buf_read_u32(&rb));
}

TEST_CASE(ring_buf_i32)
{
    u8            mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    ring_buf_write_i32(&rb, -123456);
    TEST_ASSERT_EQUAL_INT(-123456, ring_buf_read_i32(&rb));
}

TEST_CASE(ring_buf_float)
{
    u8            mem[64];
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
    u8            mem[64];
    ring_buffer_t rb;
    ring_buf_init(&rb, mem, 64);

    u8 data[] = {1, 2, 3, 4};
    ring_buf_write(&rb, data, 4);
    TEST_ASSERT_EQUAL_INT(10, ring_buf_calculate_checksum(&rb));
}
