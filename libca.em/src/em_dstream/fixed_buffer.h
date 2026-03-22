/**
 * @file fixed_buffer.h
 * @author canrad (1517807724@qq.com)
 * @brief 固定大小的缓冲区
 * 用途：1. 包装原始u8*的缓冲区，方便解析
 *      2. 组织数据写入缓冲区
 * @version 0.2
 * @date 2026-01-29
 * @update 2026-02-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DSTREAM_FIXED_BUFFER_H
#define LIBCA_EM_DSTREAM_FIXED_BUFFER_H

#include <em_base/datatype.h>
#include <em_base/debug.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief fixed_buffer 的错误码
 */
#define FIXED_BUF_OK          0  /**< 成功 */
#define FIXED_BUF_ERR_FULL    -1 /**< 缓冲区已满 */
#define FIXED_BUF_ERR_EMPTY   -2 /**< 缓冲区为空 */
#define FIXED_BUF_ERR_INVALID -3 /**< 无效参数 */
#define FIXED_BUF_ERR_OOB     -4 /**< 越界访问 */

/**
 * @brief 固定大小的缓冲区结构
 * 
 * cursor 是读写共用的处理位置标记。
 * used 是当前缓冲区中有效数据的长度。
 */
typedef struct fixed_buffer
{
    u8*   raw;      /**< 缓冲区指针 */
    usize capacity; /**< 缓冲区的总大小 */
    usize used;     /**< 缓冲区使用了的大小 */
    usize cursor;   /**< 当前游标位置 */
} fixed_buffer_t;

/**
 * @brief 初始化 FSB 缓冲区
 * @param self FSB 对象指针
 * @param data 外部提供的内存块
 * @param capacity 内存块容量
 */
void fixed_buf_init(fixed_buffer_t* self, u8* data, usize capacity);

/**
 * @brief 获取数据指针
 * @param self FSB 指针
 * @return u8* 数据起始指针
 */
static inline u8* fixed_buf_data(fixed_buffer_t* self)
{
    param_check(self != NULL);
    return self->raw;
}

/**
 * @brief 获取缓冲区总容量
 * @param self FSB 指针
 * @return usize 容量
 */
static inline usize fixed_buf_capacity(fixed_buffer_t* self)
{
    param_check(self != NULL);
    return self->capacity;
}

/**
 * @brief 获取已存储的有效数据长度
 * @param self FSB 指针
 * @return usize 长度
 */
static inline usize fixed_buf_used(fixed_buffer_t* self)
{
    param_check(self != NULL);
    return self->used;
}

/**
 * @brief 获取缓冲区剩余可写入的空间
 * @param self FSB 指针
 * @return usize 剩余大小
 */
static inline usize fixed_buf_available(fixed_buffer_t* self)
{
    param_check(self != NULL);
    return self->capacity - self->used;
}

/**
 * @brief 获取从 cursor 到 used 之间的可读字节数
 * @param self FSB 指针
 * @return usize 可读字节数
 */
static inline usize fixed_buf_remaining_to_read(fixed_buffer_t* self)
{
    param_check(self != NULL);
    if (self->cursor >= self->used) {
        return 0;
    }
    return self->used - self->cursor;
}

// Cursor 控制

/**
 * @brief 跳过 n 个字节的 cursor
 * @param self FSB 指针
 * @param size 跳过的字节数
 */
void fixed_buf_skip(fixed_buffer_t* self, usize size);

/**
 * @brief 回退 n 个字节的 cursor
 * @param self FSB 指针
 * @param size 回退的字节数
 */
void fixed_buf_rewind(fixed_buffer_t* self, usize size);

/**
 * @brief 重置 cursor 到起始位置
 * @param self FSB 指针
 */
static inline void fixed_buf_reset_cursor(fixed_buffer_t* self)
{
    param_check(self != NULL);
    self->cursor = 0;
}

/**
 * @brief 移动数据逻辑
 * 
 * 删除 [0, cursor) 的数据，将 [cursor, used) 的数据移动到头部，
 * 更新 used 并自动将 cursor 重置为 0。
 * 
 * @param self FSB 指针
 */
void fixed_buf_flush(fixed_buffer_t* self);

/**
 * @brief 从 [cursor, used) 创建一个新的 FSB (浅拷贝)
 * @param self 源 FSB 指针
 * @param new_b 目标 FSB 指针
 */
void fixed_buf_new_from_cursor(fixed_buffer_t* self, fixed_buffer_t* new_b);

// 基于 cursor 的读取

/**
 * @brief 在 cursor 处读取一个 u8 字节并推进 cursor
 * @param self FSB 指针
 * @param value 接收值的指针
 * @return i32 FIXED_BUF_OK 成功，FIXED_BUF_ERR_EMPTY 无数据可读
 */
i32 fixed_buf_read_u8(fixed_buffer_t* self, u8* value);

/**
 * @brief 从 cursor 开始读取 n 个字节，并推进 cursor
 * @param self FSB 指针
 * @param buffer 接收缓冲区
 * @param size 读取大小
 * @return i32 实际读取的大小，或错误码
 */
i32 fixed_buf_read(fixed_buffer_t* self, u8* buffer, usize size);

/**
 * @brief 尝试查看当前游标处的字节，不移动游标
 * @param self FSB 指针
 * @param value 接收字节值的指针
 * @return i32 FIXED_BUF_OK 成功，FIXED_BUF_ERR_EMPTY 无数据可读，FIXED_BUF_ERR_INVALID 无效参数
 */
i32 fixed_buf_peek(fixed_buffer_t* self, u8* buf, usize size);

/**
 * @brief 读取 cursor + offset 处的字节，不移动游标
 * @param self FSB 指针
 * @param offset 偏移量
 * @return u8 字节值，如果越界返回 0
 */
u8 fixed_buf_peek_at(fixed_buffer_t* self, usize offset);

/**
 * @brief 从 used 结尾处追加数据（不改变 cursor）
 * 
 * 如果空间不足，允许部分写入并返回实际大小。
 * 
 * @param self FSB 指针
 * @param data 要写入的数据
 * @param size 数据长度
 * @return i32 实际追加的大小
 */
i32 fixed_buf_append(fixed_buffer_t* self, const u8* data, usize size);

/**
 * @brief 将 other 缓冲区的内容合并到 self (从 used 追加，不改变 cursor)
 * @param self 目标 FSB 指针
 * @param other 源 FSB 指针
 * @return i32 实际合并的大小
 */
i32 fixed_buf_merge(fixed_buffer_t* self, const fixed_buffer_t* other);

/**
 * @brief 在指定索引索引 index 处直接写值 (不改变 cursor 和 used)
 * @param self FSB 指针
 * @param index 索引位置
 * @param value 写入的值
 */
void fixed_buf_write_u8(fixed_buffer_t* self, usize index, u8 value);

/**
 * @brief 从 cursor 位置开始写数据，并推进 cursor
 * 
 * 如果 cursor + size > used，则更新 used。
 * 如果空间不足，执行部分写入。
 * 
 * @param self FSB 指针
 * @param data 数据指针
 * @param size 数据大小
 * @return i32 实际写入的大小
 */
i32 fixed_buf_write(fixed_buffer_t* self, const u8* data, usize size);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DSTREAM_FIXED_BUFFER_H
