#ifndef LIBCA_EM_DSTREAM_DSTREAM_H
#define LIBCA_EM_DSTREAM_DSTREAM_H

#include "../em_base/datatype.h"
#include "../em_base/debug.h"
#include <sys/stat.h>

typedef struct dstream dstream_t;

// dstream 核心接口
typedef struct dstream_ops
{
    // 获取缓存区总容量，单位字节
    usize (*capacity)(dstream_t* self);
    // 获取缓存内已经使用的长度，单位字节
    usize (*used)(dstream_t* self);

    // 当前cursor的移动操作
    // skip跳过n个字节
    void (*skip)(dstream_t* self, usize len);
    // rewind回退n个字节
    void (*rewind)(dstream_t* self, usize len);
    // 获取当前相对于起始位置的偏移量
    usize (*offset)(dstream_t* self);
    // 设置cursor到位置 (false 表示位置非法)
    bool (*reset)(dstream_t* self, usize pos);

    // 读写操作
    // 读取数据到dest，返回实际读取的字节数，负数代表错误码
    i32 (*read)(dstream_t* self, void* dest, usize len);
    // 查看指定cursor+offset偏移的字节，但不移动cursor
    i32 (*peek)(dstream_t* self, usize offset, void* dest, usize len);
    // 写入数据，返回实际写入的字节数，如果写入失败返回负数错误码
    i32 (*write)(dstream_t* self, const void* src, usize len);
} dstream_ops;

struct dstream
{
    // 指向底层具体缓冲区对象
    void* buf_obj;
    // 操作函数指针表
    const dstream_ops* ops;
};

static inline usize dstream_capacity(dstream_t* self)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->capacity != NULL);

    return self->ops->capacity(self);
}

static inline usize dstream_used(dstream_t* self)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->used != NULL);

    return self->ops->used(self);
}

// 获取剩余长度
static inline usize dstream_available(dstream_t* self)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->capacity != NULL);
    param_check(self->ops->used != NULL);

    return self->ops->capacity(self) - self->ops->used(self);
}

static inline void dstream_skip(dstream_t* self, usize len)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->skip != NULL);

    self->ops->skip(self, len);
}

static inline void dstream_rewind(dstream_t* self, usize len)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->rewind != NULL);

    self->ops->rewind(self, len);
}

static inline usize dstream_offset(dstream_t* self)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->offset != NULL);

    return self->ops->offset(self);
}

static inline bool dstream_reset(dstream_t* self, usize pos)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->reset != NULL);

    return self->ops->reset(self, pos);
}

static inline i32 dstream_read(dstream_t* self, void* dest, usize len)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->read != NULL);

    return self->ops->read(self, dest, len);
}

static inline i32 dstream_peek(dstream_t* self, usize offset, void* dest, usize len)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->peek != NULL);

    return self->ops->peek(self, offset, dest, len);
}

static inline i32 dstream_write(dstream_t* self, const void* src, usize len)
{
    param_check(self != NULL);
    param_check(self->ops != NULL);
    param_check(self->ops->write != NULL);

    return self->ops->write(self, src, len);
}

// 辅助接口
// 裸奔不检查的版本
static inline u8 dstream_peek_u8(dstream_t* self, usize offset)
{
    u8 val = 0;
    dstream_peek(self, offset, &val, sizeof(u8));
    return val;
}

static inline u16 dstream_peek_u16_le(dstream_t* self, usize offset)
{
    return (u16)dstream_peek_u8(self, offset) | ((u16)dstream_peek_u8(self, offset + 1) << 8);
}

static inline u16 dstream_peek_u16_be(dstream_t* self, usize offset)
{
    return ((u16)dstream_peek_u8(self, offset) << 8) | (u16)dstream_peek_u8(self, offset + 1);
}

static inline u32 dstream_peek_u32_le(dstream_t* self, usize offset)
{
    return (u32)dstream_peek_u8(self, offset) | ((u32)dstream_peek_u8(self, offset + 1) << 8) |
           ((u32)dstream_peek_u8(self, offset + 2) << 16) | ((u32)dstream_peek_u8(self, offset + 3) << 24);
}

static inline u32 dstream_peek_u32_be(dstream_t* self, usize offset)
{
    return ((u32)dstream_peek_u8(self, offset) << 24) | ((u32)dstream_peek_u8(self, offset + 1) << 16) |
           ((u32)dstream_peek_u8(self, offset + 2) << 8) | (u32)dstream_peek_u8(self, offset + 3);
}

static inline i8 dstream_peek_i8(dstream_t* self, usize offset)
{
    i8 val = 0;
    dstream_peek(self, offset, &val, sizeof(i8));
    return val;
}

static inline i16 dstream_peek_i16_le(dstream_t* self, usize offset)
{
    return (i16)dstream_peek_u8(self, offset) | ((i16)dstream_peek_u8(self, offset + 1) << 8);
}

static inline i16 dstream_peek_i16_be(dstream_t* self, usize offset)
{
    return ((i16)dstream_peek_u8(self, offset) << 8) | (i16)dstream_peek_u8(self, offset + 1);
}

static inline i32 dstream_peek_i32_le(dstream_t* self, usize offset)
{
    return (i32)dstream_peek_u8(self, offset) | ((i32)dstream_peek_u8(self, offset + 1) << 8) |
           ((i32)dstream_peek_u8(self, offset + 2) << 16) | ((i32)dstream_peek_u8(self, offset + 3) << 24);
}

static inline i32 dstream_peek_i32_be(dstream_t* self, usize offset)
{
    return ((i32)dstream_peek_u8(self, offset) << 24) | ((i32)dstream_peek_u8(self, offset + 1) << 16) |
           ((i32)dstream_peek_u8(self, offset + 2) << 8) | (i32)dstream_peek_u8(self, offset + 3);
}

static inline u8 dstream_read_u8(dstream_t* self)
{
    u8 val = 0;
    dstream_read(self, &val, sizeof(u8));
    return val;
}

static inline u16 dstream_read_u16_le(dstream_t* self)
{
    return (u16)dstream_read_u8(self) | ((u16)dstream_read_u8(self) << 8);
}

static inline u16 dstream_read_u16_be(dstream_t* self)
{
    return ((u16)dstream_read_u8(self) << 8) | (u16)dstream_read_u8(self);
}

static inline u32 dstream_read_u32_le(dstream_t* self)
{
    return (u32)dstream_read_u8(self) | ((u32)dstream_read_u8(self) << 8) |
           ((u32)dstream_read_u8(self) << 16) | ((u32)dstream_read_u8(self) << 24);
}

static inline u32 dstream_read_u32_be(dstream_t* self)
{
    return ((u32)dstream_read_u8(self) << 24) | ((u32)dstream_read_u8(self) << 16) |
           ((u32)dstream_read_u8(self) << 8) | (u32)dstream_read_u8(self);
}

static inline i8 dstream_read_i8(dstream_t* self)
{
    i8 val = 0;
    dstream_read(self, &val, sizeof(i8));
    return val;
}

static inline i16 dstream_read_i16_le(dstream_t* self)
{
    return (i16)dstream_read_u8(self) | ((i16)dstream_read_u8(self) << 8);
}

static inline i16 dstream_read_i16_be(dstream_t* self)
{
    return ((i16)dstream_read_u8(self) << 8) | (i16)dstream_read_u8(self);
}

static inline i32 dstream_read_i32_le(dstream_t* self)
{
    return (i32)dstream_read_u8(self) | ((i32)dstream_read_u8(self) << 8) |
           ((i32)dstream_read_u8(self) << 16) | ((i32)dstream_read_u8(self) << 24);
}

static inline i32 dstream_read_i32_be(dstream_t* self)
{
    return ((i32)dstream_read_u8(self) << 24) | ((i32)dstream_read_u8(self) << 16) |
           ((i32)dstream_read_u8(self) << 8) | (i32)dstream_read_u8(self);
}

static inline i32 dstream_write_u8(dstream_t* self, u8 value)
{
    return dstream_write(self, &value, sizeof(u8));
}

static inline i32 dstream_write_u16_le(dstream_t* self, u16 value)
{
    u8 bytes[2] = {value & 0xFF, (value >> 8) & 0xFF};
    return dstream_write(self, bytes, sizeof(bytes));
}

static inline i32 dstream_write_u16_be(dstream_t* self, u16 value)
{
    u8 bytes[2] = {(value >> 8) & 0xFF, value & 0xFF};
    return dstream_write(self, bytes, sizeof(bytes));
}

static inline i32 dstream_write_u32_le(dstream_t* self, u32 value)
{
    u8 bytes[4] = {value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF, (value >> 24) & 0xFF};
    return dstream_write(self, bytes, sizeof(bytes));
}

static inline i32 dstream_write_u32_be(dstream_t* self, u32 value)
{
    u8 bytes[4] = {(value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF};
    return dstream_write(self, bytes, sizeof(bytes));
}

static inline i32 dstream_write_i8(dstream_t* self, i8 value)
{
    return dstream_write(self, &value, sizeof(i8));
}

static inline i32 dstream_write_i16_le(dstream_t* self, i16 value)
{
    return dstream_write_u16_le(self, (u16)value);
}

static inline i32 dstream_write_i16_be(dstream_t* self, i16 value)
{
    return dstream_write_u16_be(self, (u16)value);
}

static inline i32 dstream_write_i32_le(dstream_t* self, i32 value)
{
    return dstream_write_u32_le(self, (u32)value);
}

static inline i32 dstream_write_i32_be(dstream_t* self, i32 value)
{
    return dstream_write_u32_be(self, (u32)value);
}





#endif   // !LIBCA_EM_DSTREAM_DSTREAM_H
