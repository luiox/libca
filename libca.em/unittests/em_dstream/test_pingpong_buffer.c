/* Auto-migrated from src/em_dstream/pingpong_buffer.c test blocks */
#include "pingpong_buffer.h"
#include <string.h>   // for memset


#include <em_test/test.h>

#define TEST_BUFFER_SIZE 256
static u8 test_buffer1[TEST_BUFFER_SIZE];
static u8 test_buffer2[TEST_BUFFER_SIZE];

TEST_CASE(pingpong_buf_init)
{
    pingpong_buffer_t pingpong_buf;
    pingpong_buf_init(&pingpong_buf, test_buffer1, test_buffer2, TEST_BUFFER_SIZE);

    TEST_ASSERT_EQUAL_INT(TEST_BUFFER_SIZE, (int)pingpong_buf_get_size(&pingpong_buf));
    TEST_ASSERT(pingpong_buf_get_read_buffer(&pingpong_buf) == test_buffer1);
    TEST_ASSERT(pingpong_buf_get_write_buffer(&pingpong_buf) == test_buffer2);
    TEST_ASSERT_EQUAL_INT(0, pingpong_buf_is_writing(&pingpong_buf));
}

TEST_CASE(pingpong_buf_write_and_switch)
{
    pingpong_buffer_t pingpong_buf;
    pingpong_buf_init(&pingpong_buf, test_buffer1, test_buffer2, TEST_BUFFER_SIZE);

    // 开始写入
    pingpong_buf_start_write(&pingpong_buf);
    TEST_ASSERT_EQUAL_INT(1, pingpong_buf_is_writing(&pingpong_buf));

    // 写入过程中尝试切换（应该失败）
    u8 result = pingpong_buf_switch(&pingpong_buf);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT(pingpong_buf_get_read_buffer(&pingpong_buf) == test_buffer1);

    // 结束写入
    pingpong_buf_end_write(&pingpong_buf);
    TEST_ASSERT_EQUAL_INT(0, pingpong_buf_is_writing(&pingpong_buf));

    // 再次尝试切换（应该成功）
    result = pingpong_buf_switch(&pingpong_buf);
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT(pingpong_buf_get_read_buffer(&pingpong_buf) == test_buffer2);
    TEST_ASSERT(pingpong_buf_get_write_buffer(&pingpong_buf) == test_buffer1);
}

TEST_CASE(pingpong_buf_data_integrity)
{
    pingpong_buffer_t pingpong_buf;
    pingpong_buf_init(&pingpong_buf, test_buffer1, test_buffer2, TEST_BUFFER_SIZE);

    u8* write_buf = pingpong_buf_get_write_buffer(&pingpong_buf);
    for (int i = 0; i < 10; i++) {
        write_buf[i] = i + 1;
    }

    pingpong_buf_switch(&pingpong_buf);

    u8* read_buf = pingpong_buf_get_read_buffer(&pingpong_buf);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(i + 1, read_buf[i]);
    }
}

TEST_CASE(pingpong_buf_clear)
{
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        test_buffer1[i] = 0xFF;
    }

    pingpong_buf_clear(test_buffer1, TEST_BUFFER_SIZE);

    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        TEST_ASSERT_EQUAL_INT(0, test_buffer1[i]);
    }
}
