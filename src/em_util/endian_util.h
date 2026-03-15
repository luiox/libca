#ifndef LIBCA_EM_UTIL_ENDIAN_UTIL_H
#define LIBCA_EM_UTIL_ENDIAN_UTIL_H

#include <em_base/datatype.h>

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


#endif // !LIBCA_EM_UTIL_ENDIAN_UTIL_H
