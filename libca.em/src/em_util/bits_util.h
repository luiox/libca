/// @file bits_util.h
/// @author Canrad
/// @brief 位操作工具
/// @version 0.1
/// @date 2026-03-31
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_UTIL_BITS_UTIL_H
#define LIBCA_EM_UTIL_BITS_UTIL_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

// 位序与端序说明
// - 位序统一采用 MSB-first bitstream：start_bit = 0 对应 data[0] 的 bit7。
// - le/be 仅影响读取/写入 value 的字节序，不改变 bitstream 的位序。

// 返回码
#define BITS_UTIL_OK       0
#define BITS_UTIL_EINVAL   (-1)
#define BITS_UTIL_ERANGE   (-2)

// 对于需要精准控制高低位的情况下使用下面的宏
// 其中bits是一个整数类型的变量，一般是u8、u16、u32、u64等
// n是位的索引，从0开始
// val是要设置的值，0或1
// 取一个位上的值
#define bits_get(bits, n) (((bits) >> (n)) & 0x01u)
// 设置一个位上的值
#define bits_set(bits, n, val) ((bits) = ((bits) & ~(1ULL << (n))) | (((val) & 0x01u) ? (1ULL << (n)) : 0ULL))
// 反转一个位上的值
#define bits_toggle(bits, n) ((bits) ^= (1ULL << (n)))
// 获取第n位的掩码
#define bits_mask(n) (1ULL << (n))
// 获取低n位的掩码
#define bits_mask_low(n) (((n) >= 64) ? (~0ULL) : ((1ULL << (n)) - 1ULL))
// 检查某一位是否为1
#define bits_check_bit(bits, n) (((bits) & (1ULL << (n))) != 0)

/// @brief 按 bitstream 顺序写入小端 value
/// @param data 目标缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 写入的 bit 数
/// @param value 写入的值（低 length 位有效）
/// @return 0 成功，负数错误码
i32 bits_write_le(u8* data, usize data_size, usize start_bit, u8 length, u32 value);

/// @brief 按 bitstream 顺序写入大端 value
/// @param data 目标缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 写入的 bit 数
/// @param value 写入的值（低 length 位有效）
/// @return 0 成功，负数错误码
i32 bits_write_be(u8* data, usize data_size, usize start_bit, u8 length, u32 value);

/// @brief 按 bitstream 顺序读取小端 value
/// @param data 源缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 读取的 bit 数
/// @param value 输出指针
/// @return 0 成功，负数错误码
i32 bits_read_le(const u8* data, usize data_size, usize start_bit, u8 length, u32* value);

/// @brief 按 bitstream 顺序读取大端 value
/// @param data 源缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 读取的 bit 数
/// @param value 输出指针
/// @return 0 成功，负数错误码
i32 bits_read_be(const u8* data, usize data_size, usize start_bit, u8 length, u32* value);

#ifdef HAS_INT64
/// @brief 按 bitstream 顺序写入小端 64 位 value
/// @param data 目标缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 写入的 bit 数 (1~64)
/// @param value 写入的值（低 length 位有效）
/// @return 0 成功，负数错误码
i32 bits_write64_le(u8* data, usize data_size, usize start_bit, u8 length, u64 value);

/// @brief 按 bitstream 顺序写入大端 64 位 value
/// @param data 目标缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 写入的 bit 数 (1~64)
/// @param value 写入的值（低 length 位有效）
/// @return 0 成功，负数错误码
i32 bits_write64_be(u8* data, usize data_size, usize start_bit, u8 length, u64 value);

/// @brief 按 bitstream 顺序读取小端 64 位 value
/// @param data 源缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 读取的 bit 数 (1~64)
/// @param value 输出指针
/// @return 0 成功，负数错误码
i32 bits_read64_le(const u8* data, usize data_size, usize start_bit, u8 length, u64* value);

/// @brief 按 bitstream 顺序读取大端 64 位 value
/// @param data 源缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引（从0开始，指向 data[0] 的 bit7）
/// @param length 读取的 bit 数 (1~64)
/// @param value 输出指针
/// @return 0 成功，负数错误码
i32 bits_read64_be(const u8* data, usize data_size, usize start_bit, u8 length, u64* value);
#endif

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_UTIL_BITS_UTIL_H