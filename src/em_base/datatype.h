/**
 * @file datatype.h
 * @author canrad (1517807724@qq.com)
 * @brief 基础类型的定义
 * 位，字节，字节序相关的操作
 * @version 0.2
 * @date 2025-07-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef DATATYPE_H
#define DATATYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 整数
typedef uint8_t      u8;
typedef uint16_t     u16;
typedef uint32_t     u32;
typedef int8_t       i8;
typedef int16_t      i16;
typedef int32_t      i32;
#if USE_RUST_STYLE_INT
typedef s8   i8;
typedef s16  i16;
typedef s32  i32;
#endif
#ifdef HAS_INT64
typedef uint64_t     u64;
typedef int64_t      i64;
#    if USE_RUST_STYLE_INT
typedef i64  i64;
#    endif
#endif
// 浮点数
typedef float        f32;
typedef double       f64;
// size
typedef size_t          usize;

// 对于仅需要存位级别的数据，但是不需要很精准控制高低位的情况下可以使用下面这个
// 8个位
typedef union bits8
{
    struct bits8_u
    {
        u8 b0 : 1;   // 位0
        u8 b1 : 1;   // 位1
        u8 b2 : 1;   // 位2
        u8 b3 : 1;   // 位3
        u8 b4 : 1;   // 位4
        u8 b5 : 1;   // 位5
        u8 b6 : 1;   // 位6
        u8 b7 : 1;   // 位7
    } u;
    u8 val;   // 整体值
} bits8_t;

// 16个位
typedef union bits16
{
    struct bits16_u
    {
        u8 b0 : 1;    // 位0
        u8 b1 : 1;    // 位1
        u8 b2 : 1;    // 位2
        u8 b3 : 1;    // 位3
        u8 b4 : 1;    // 位4
        u8 b5 : 1;    // 位5
        u8 b6 : 1;    // 位6
        u8 b7 : 1;    // 位7
        u8 b8 : 1;    // 位8
        u8 b9 : 1;    // 位9
        u8 b10 : 1;   // 位10
        u8 b11 : 1;   // 位11
        u8 b12 : 1;   // 位12
        u8 b13 : 1;   // 位13
        u8 b14 : 1;   // 位14
        u8 b15 : 1;   // 位15
    } u;
    u16 val;   // 整体值
} bits16_t;

// 常用函数指针
typedef void(*runnable_fn_t)(void);
// 暂时不开放谓词
//typedef bool(*predicate_fn_t)(void* arg);


// 对于需要精准控制高低位的情况下使用下面的宏
// 其中bits是一个整数类型的变量，一般是u8、u16、u32等
// n是位的索引，从0开始
// val是要设置的值，0或1
// 取一个位上的值
#define bits_get(bits, n) (((bits) >> (n)) & 0x01)
// 设置一个位上的值
#define bits_set(bits, n, val) ((bits) = ((bits) & ~(1 << (n))) | ((val) << (n)))
// 反转一个位上的值
#define bits_toggle(bits, n) ((bits) ^= (1 << (n)))
// 获取第n位的掩码
#define bits_mask(n) (1U << (n))
// 获取低n位的掩码
#define bits_mask_low(n) ((1U << (n)) - 1)
// 检查某一位是否为1
#define bits_check_bit(bits, n) (((bits) & (1U << (n))) != 0)

// 获取数组元素个数
#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]))

// 判断一个变量是否为无符号类型
#define is_unsigned_v(a) (a >= 0 && ~a >= 0)

// 判断一个类型是否为无符号类型
#define is_unsigned_t(type) ((type)0 - 1 > 0)

// 标记未使用的参数
// 例: unused_param(a);
#define unused_param(param) (void)(param)

// 是否是小端序
bool is_little_endian(void);
// 是否是大端序
bool is_big_endian(void);

// 以大端的方式解释一个数组

// 大端方式解释字节数组到u16
static inline u16 big_endian_read_u16(const u8* bytes)
{
    return (u16)((bytes[0] << 8) | bytes[1]);
}

// 大端方式解释字节数组到s16
static inline i16 big_endian_read_s16(const u8* bytes)
{
    return (i16)((bytes[0] << 8) | bytes[1]);
}

// 大端方式解释字节数组到u32
static inline u32 big_endian_read_u32(const u8* bytes)
{
    return (u32)((bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3]);
}

// 大端方式解释字节数组到s32
static inline i32 big_endian_read_s32(const u8* bytes)
{
    return (i32)((bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3]);
}

// 以小端的方式解释一个数组

// 小端方式解释字节数组到u16
static inline u16 little_endian_read_u16(const u8* bytes)
{
    return (u16)(bytes[0] | (bytes[1] << 8));
}

// 小端方式解释字节数组到s16
static inline i16 little_endian_read_s16(const u8* bytes)
{
    return (i16)(bytes[0] | (bytes[1] << 8));
}

// 小端方式解释字节数组到u32
static inline u32 little_endian_read_u32(const u8* bytes)
{
    return (u32)((bytes[0]) | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

// 小端方式解释字节数组到s32
static inline i32 little_endian_read_s32(const u8* bytes)
{
    return (i32)((bytes[0]) | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

// 以大端方式写入u16到字节数组
static inline void big_endian_write_u16(u8* bytes, u16 value)
{
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
}

// 以大端方式写入s16到字节数组
static inline void big_endian_write_s16(u8* bytes, i16 value)
{
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
}

// 以大端方式写入u32到字节数组
static inline void big_endian_write_u32(u8* bytes, u32 value)
{
    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)(value);
}

// 以大端方式写入s32到字节数组
static inline void big_endian_write_s32(u8* bytes, i32 value)
{
    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)(value);
}

// 以小端方式写入u16到字节数组
static inline void little_endian_write_u16(u8* bytes, u16 value)
{
    bytes[0] = (u8)(value);
    bytes[1] = (u8)(value >> 8);
}

// 以小端方式写入s16到字节数组
static inline void little_endian_write_s16(u8* bytes, i16 value)
{
    bytes[0] = (u8)(value);
    bytes[1] = (u8)(value >> 8);
}

// 以小端方式写入u32到字节数组
static inline void little_endian_write_u32(u8* bytes, u32 value)
{
    bytes[0] = (u8)(value);
    bytes[1] = (u8)(value >> 8);
    bytes[2] = (u8)(value >> 16);
    bytes[3] = (u8)(value >> 24);
}

// 以小端方式写入s32到字节数组
static inline void little_endian_write_s32(u8* bytes, i32 value)
{
    bytes[0] = (u8)(value);
    bytes[1] = (u8)(value >> 8);
    bytes[2] = (u8)(value >> 16);
    bytes[3] = (u8)(value >> 24);
}

// 获取一个结构体的成员指针
// 例如
// struct list_head {
//     struct list_head *next, *prev;
// };
// struct task {
//     int id;
//     struct list_head node;
// };
// int main() {
//     struct task t = { .id = 42 };
//     struct list_head *nodeptr = &t.node;
//     struct task *tp = container_of(nodeptr, struct task, node);
//     printf("Task ID: %d\n", tp->id); // 输出: Task ID: 42
//     return 0;
// }
// 
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif   // !DATATYPE_H
