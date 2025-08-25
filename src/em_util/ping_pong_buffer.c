#include "ping_pong_buffer.h"

/**
 * @brief 初始化乒乓缓冲区
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @param buffer1 缓冲区1指针
 * @param buffer2 缓冲区2指针
 * @param buffer_size 缓冲区大小
 */
void ping_pong_buffer_init(ping_pong_buffer_t* ping_pong_buf, u8* buffer1, u8* buffer2,
                           usize buffer_size)
{
    ping_pong_buf->read_buffer  = buffer1;
    ping_pong_buf->write_buffer = buffer2;
    ping_pong_buf->buffer_size  = buffer_size;
    ping_pong_buf->write_flag   = PING_PONG_BUFFER_IDLE;
}

/**
 * @brief 切换读写缓冲区
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8 切换结果，1表示成功，0表示失败（正在写入无法切换）
 */
u8 ping_pong_buffer_switch(ping_pong_buffer_t* ping_pong_buf)
{
    // 检查是否正在写入
    if (ping_pong_buffer_is_writing(ping_pong_buf)) {
        // 正在写入，不能切换
        return 0;
    }

    u8* temp                    = ping_pong_buf->read_buffer;
    ping_pong_buf->read_buffer  = ping_pong_buf->write_buffer;
    ping_pong_buf->write_buffer = temp;

    return 1;
}

/**
 * @brief 获取读缓冲区指针
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8* 读缓冲区指针
 */
u8* ping_pong_buffer_get_read_buffer(ping_pong_buffer_t* ping_pong_buf)
{
    return ping_pong_buf->read_buffer;
}

/**
 * @brief 获取写缓冲区指针
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8* 写缓冲区指针
 */
u8* ping_pong_buffer_get_write_buffer(ping_pong_buffer_t* ping_pong_buf)
{
    return ping_pong_buf->write_buffer;
}

/**
 * @brief 获取缓冲区大小
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return usize 缓冲区大小
 */
usize ping_pong_buffer_get_size(ping_pong_buffer_t* ping_pong_buf)
{
    return ping_pong_buf->buffer_size;
}

/**
 * @brief 清空指定缓冲区
 *
 * @param buffer 要清空的缓冲区指针
 * @param size 缓冲区大小
 */
void ping_pong_buffer_clear(u8* buffer, usize size)
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
void ping_pong_buffer_start_write(ping_pong_buffer_t* ping_pong_buf)
{
    ping_pong_buf->write_flag = PING_PONG_BUFFER_WRITING;
}

/**
 * @brief 结束写入操作，清除写入标志
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 */
void ping_pong_buffer_end_write(ping_pong_buffer_t* ping_pong_buf)
{
    ping_pong_buf->write_flag = PING_PONG_BUFFER_IDLE;
}

/**
 * @brief 检查是否正在写入
 *
 * @param ping_pong_buf 乒乓缓冲区结构体指针
 * @return u8 写入状态，1表示正在写入，0表示空闲
 */
u8 ping_pong_buffer_is_writing(ping_pong_buffer_t* ping_pong_buf)
{
    return ping_pong_buf->write_flag == PING_PONG_BUFFER_WRITING;
}
