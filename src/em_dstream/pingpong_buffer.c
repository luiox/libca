#include "pingpong_buffer.h"

/**
 * @brief 初始化乒乓缓冲区
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @param buffer1 缓冲区1指针
 * @param buffer2 缓冲区2指针
 * @param buffer_size 缓冲区大小
 */
void pingpong_buf_init(pingpong_buffer_t* pingpong_buf, u8* buffer1, u8* buffer2,
                           usize buffer_size)
{
    pingpong_buf->read_buffer  = buffer1;
    pingpong_buf->write_buffer = buffer2;
    pingpong_buf->buffer_size  = buffer_size;
    pingpong_buf->write_flag   = PINGPONG_BUF_IDLE;
}

/**
 * @brief 切换读写缓冲区
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8 切换结果，1表示成功，0表示失败（正在写入无法切换）
 */
u8 pingpong_buf_switch(pingpong_buffer_t* pingpong_buf)
{
    // 检查是否正在写入
    if (pingpong_buf_is_writing(pingpong_buf)) {
        // 正在写入，不能切换
        return 0;
    }

    u8* temp                    = pingpong_buf->read_buffer;
    pingpong_buf->read_buffer  = pingpong_buf->write_buffer;
    pingpong_buf->write_buffer = temp;

    return 1;
}

/**
 * @brief 获取读缓冲区指针
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8* 读缓冲区指针
 */
u8* pingpong_buf_get_read_buffer(pingpong_buffer_t* pingpong_buf)
{
    return pingpong_buf->read_buffer;
}

/**
 * @brief 获取写缓冲区指针
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8* 写缓冲区指针
 */
u8* pingpong_buf_get_write_buffer(pingpong_buffer_t* pingpong_buf)
{
    return pingpong_buf->write_buffer;
}

/**
 * @brief 获取缓冲区大小
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return usize 缓冲区大小
 */
usize pingpong_buf_get_size(pingpong_buffer_t* pingpong_buf)
{
    return pingpong_buf->buffer_size;
}

/**
 * @brief 清空指定缓冲区
 *
 * @param buffer 要清空的缓冲区指针
 * @param size 缓冲区大小
 */
void pingpong_buf_clear(u8* buffer, usize size)
{
    for (u16 i = 0; i < size; i++) {
        buffer[i] = 0;
    }
}

/**
 * @brief 开始写入操作，设置写入标志
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 */
void pingpong_buf_start_write(pingpong_buffer_t* pingpong_buf)
{
    pingpong_buf->write_flag = PINGPONG_BUF_WRITING;
}

/**
 * @brief 结束写入操作，清除写入标志
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 */
void pingpong_buf_end_write(pingpong_buffer_t* pingpong_buf)
{
    pingpong_buf->write_flag = PINGPONG_BUF_IDLE;
}

/**
 * @brief 检查是否正在写入
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8 写入状态，1表示正在写入，0表示空闲
 */
u8 pingpong_buf_is_writing(pingpong_buffer_t* pingpong_buf)
{
    return pingpong_buf->write_flag == PINGPONG_BUF_WRITING;
}

#if TEST_ENABLE

#include "../em_test/test.h"

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

#endif
