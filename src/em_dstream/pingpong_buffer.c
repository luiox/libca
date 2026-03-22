#include "pingpong_buffer.h"
#include <string.h> // for memset

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
    memset(buffer, 0, size);
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

