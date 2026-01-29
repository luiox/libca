#ifndef LIBCA_EM_UTIL_FIXED_SIZE_BUFFER_H
#define LIBCA_EM_UTIL_FIXED_SIZE_BUFFER_H

#include "../em_base/datatype.h"

typedef struct fixed_size_buffer
{
    // 缓冲区指针
    u8* raw;
    // 缓冲区的总大小
    usize capacity;
    // 缓冲区使用了的大小
    usize len;
    // 当前游标位置
    usize cursor;
} fixed_size_buffer_t;


void fsb_init(fixed_size_buffer_t* self, u8* data, usize capacity);

static inline u8* fsb_data(fixed_size_buffer_t* self)
{
    return self->raw;
}

static inline usize fsb_capacity(fixed_size_buffer_t* self)
{
    return self->capacity;
}

static inline usize fsb_used(fixed_size_buffer_t* self)
{
    return self->len;
}

static inline usize fsb_available(fixed_size_buffer_t* self)
{
    return self->capacity - self->len;
}

// Cursor控制
// 跳过n个字节的cursor
void fsb_skip(fixed_size_buffer_t* self, usize size);
// 回退n个字节的cursor
void fsb_rewind(fixed_size_buffer_t* self, usize size);
// 重置cursor
static inline void fsb_reset_cursor(fixed_size_buffer_t* self)
{
    self->cursor = 0;
}
// 删除[0, cursor)的数据，将[cursor, len)的数据移动到头部
void fsb_flush(fixed_size_buffer_t* self);
// 创建一个新的fsb从[cursor,len)
void fsb_new_from_cursor(fixed_size_buffer_t* self, fixed_size_buffer_t* new_b);

// 基于cursor的读取
// 读取u8
i32 fsb_read_u8(fixed_size_buffer_t* self, u8* value);
// 读取n个字节的数据
i32 fsb_read(fixed_size_buffer_t* self, u8* buffer, usize size);
// 读取 Cursor 处的字节，不移动游标
u8 fsb_peek(fixed_size_buffer_t* self);
// 读取 Cursor + offset 处的字节，不移动游标
u8 fsb_peek_at(fixed_size_buffer_t* self, usize offset);

// 返回实际写入的大小
i32 fsb_append(fixed_size_buffer_t* self, const u8* data, usize size);
// 合并other到self，返回实际写入的大小
i32 fsb_merge(fixed_size_buffer_t* self, const fixed_size_buffer_t* other);
// 直接设置某个值
void fsb_write_u8(fixed_size_buffer_t* self, usize index, u8 value);
// 在cusor位置开始写，并且推进cursor，返回实际写入大小
i32 fsb_write(fixed_size_buffer_t* self, const u8* data, usize size);

#endif   // !LIBCA_EM_UTIL_FIXED_SIZE_BUFFER_H
