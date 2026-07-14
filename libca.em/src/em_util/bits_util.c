#include "bits_util.h"

/*
 * MSB-first bitstream 索引:
 * start_bit=0 -> data[0] bit7, start_bit=7 -> data[0] bit0, start_bit=8 -> data[1] bit7.
 * 示例: data[0]=0xB3(1011 0011), data[1]=0x5C(0101 1100)
 * start_bit=4, length=8 -> bitstream 为 0011 0101 (0x35)
 * le/be 仅影响 value 的装载顺序: le 从 LSB 先写/读, be 从 MSB 先写/读。
 */

/// @brief 从 MSB-first bitstream 读取单个位
/// @param data 源缓冲区
/// @param bit_index bitstream 索引
/// @return 0 或 1
static u8 bits_read_bit_msb(const u8* data, usize bit_index)
{
    usize byte_index = bit_index >> 3;
    u8    bit_in_byte = (u8)(7u - (bit_index & 0x7u));   // MSB-first: 索引0对应bit7
    return (u8)((data[byte_index] >> bit_in_byte) & 0x01u);
}

/// @brief 写入单个位到 MSB-first bitstream
/// @param data 目标缓冲区
/// @param bit_index bitstream 索引
/// @param bit_value 0 或 1
static void bits_write_bit_msb(u8* data, usize bit_index, u8 bit_value)
{
    usize byte_index = bit_index >> 3;
    u8    bit_in_byte = (u8)(7u - (bit_index & 0x7u));   // MSB-first: 索引0对应bit7
    u8    mask = (u8)(1u << bit_in_byte);

    if (bit_value != 0u) {
        data[byte_index] |= mask;
    } else {
        data[byte_index] &= (u8)~mask;
    }
}

/// @brief 校验 bit 读写的公共参数
/// @param data 源/目标缓冲区
/// @param data_size 缓冲区长度 (字节)
/// @param start_bit 起始 bit 索引
/// @param length bit 长度
/// @param max_len 支持的最大 bit 长度
/// @return 0 成功, 负数错误码
static i32 bits_validate_params(const u8* data, usize data_size, usize start_bit, u8 length, usize max_len)
{
    if (data == NULL) {
        return BITS_UTIL_EINVAL;
    }
    if (data_size == 0) {
        return BITS_UTIL_EINVAL;
    }
    if (length == 0 || (usize)length > max_len) {
        return BITS_UTIL_EINVAL;
    }

    usize total_bits = data_size * 8u;
    if (start_bit >= total_bits) {
        return BITS_UTIL_ERANGE;
    }
    if ((usize)length > (total_bits - start_bit)) {
        return BITS_UTIL_ERANGE;
    }

    return BITS_UTIL_OK;
}

/// @brief 生成 u32 低位掩码
/// @param length 掩码位数
/// @return 低 length 位为 1 的掩码
static u32 bits_mask_low_u32(u8 length)
{
    if (length >= 32) {
        return 0xFFFFFFFFu;
    }
    return (u32)((1u << length) - 1u);
}

#ifdef HAS_INT64
/// @brief 生成 u64 低位掩码
/// @param length 掩码位数
/// @return 低 length 位为 1 的掩码
static u64 bits_mask_low_u64(u8 length)
{
    if (length >= 64) {
        return (u64)~0ULL;
    }
    return (u64)((1ULL << length) - 1ULL);
}
#endif

i32 bits_write_le(u8* data, usize data_size, usize start_bit, u8 length, u32 value)
{
    i32 ret = bits_validate_params(data, data_size, start_bit, length, 32);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u32   masked = value & bits_mask_low_u32(length);
    usize len = (usize)length;

    // le: value 低位先进入 bitstream
    for (usize i = 0; i < len; i++) {
        u8 bit_value = (u8)((masked >> i) & 0x01u);
        bits_write_bit_msb(data, start_bit + i, bit_value);
    }

    return BITS_UTIL_OK;
}

i32 bits_write_be(u8* data, usize data_size, usize start_bit, u8 length, u32 value)
{
    i32 ret = bits_validate_params(data, data_size, start_bit, length, 32);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u32   masked = value & bits_mask_low_u32(length);
    usize len = (usize)length;

    // be: value 高位先进入 bitstream
    for (usize i = 0; i < len; i++) {
        usize shift = len - 1u - i;
        u8    bit_value = (u8)((masked >> shift) & 0x01u);
        bits_write_bit_msb(data, start_bit + i, bit_value);
    }

    return BITS_UTIL_OK;
}

i32 bits_read_le(const u8* data, usize data_size, usize start_bit, u8 length, u32* value)
{
    if (value == NULL) {
        return BITS_UTIL_EINVAL;
    }

    i32 ret = bits_validate_params(data, data_size, start_bit, length, 32);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u32   result = 0u;
    usize len = (usize)length;

    // le: bitstream 低位映射到 value 低位
    for (usize i = 0; i < len; i++) {
        u8 bit_value = bits_read_bit_msb(data, start_bit + i);
        result |= ((u32)bit_value << i);
    }

    *value = result;
    return BITS_UTIL_OK;
}

i32 bits_read_be(const u8* data, usize data_size, usize start_bit, u8 length, u32* value)
{
    if (value == NULL) {
        return BITS_UTIL_EINVAL;
    }

    i32 ret = bits_validate_params(data, data_size, start_bit, length, 32);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u32   result = 0u;
    usize len = (usize)length;

    // be: bitstream 先读到 value 高位
    for (usize i = 0; i < len; i++) {
        u8 bit_value = bits_read_bit_msb(data, start_bit + i);
        result = (result << 1) | (u32)bit_value;
    }

    *value = result;
    return BITS_UTIL_OK;
}

#ifdef HAS_INT64

i32 bits_write64_le(u8* data, usize data_size, usize start_bit, u8 length, u64 value)
{
    i32 ret = bits_validate_params(data, data_size, start_bit, length, 64);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u64   masked = value & bits_mask_low_u64(length);
    usize len = (usize)length;

    // le: value 低位先进入 bitstream
    for (usize i = 0; i < len; i++) {
        u8 bit_value = (u8)((masked >> i) & 0x01u);
        bits_write_bit_msb(data, start_bit + i, bit_value);
    }

    return BITS_UTIL_OK;
}

i32 bits_write64_be(u8* data, usize data_size, usize start_bit, u8 length, u64 value)
{
    i32 ret = bits_validate_params(data, data_size, start_bit, length, 64);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u64   masked = value & bits_mask_low_u64(length);
    usize len = (usize)length;

    // be: value 高位先进入 bitstream
    for (usize i = 0; i < len; i++) {
        usize shift = len - 1u - i;
        u8    bit_value = (u8)((masked >> shift) & 0x01u);
        bits_write_bit_msb(data, start_bit + i, bit_value);
    }

    return BITS_UTIL_OK;
}

i32 bits_read64_le(const u8* data, usize data_size, usize start_bit, u8 length, u64* value)
{
    if (value == NULL) {
        return BITS_UTIL_EINVAL;
    }

    i32 ret = bits_validate_params(data, data_size, start_bit, length, 64);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u64   result = 0ULL;
    usize len = (usize)length;

    // le: bitstream 低位映射到 value 低位
    for (usize i = 0; i < len; i++) {
        u8 bit_value = bits_read_bit_msb(data, start_bit + i);
        result |= ((u64)bit_value << i);
    }

    *value = result;
    return BITS_UTIL_OK;
}

i32 bits_read64_be(const u8* data, usize data_size, usize start_bit, u8 length, u64* value)
{
    if (value == NULL) {
        return BITS_UTIL_EINVAL;
    }

    i32 ret = bits_validate_params(data, data_size, start_bit, length, 64);
    if (ret != BITS_UTIL_OK) {
        return ret;
    }

    u64   result = 0ULL;
    usize len = (usize)length;

    // be: bitstream 先读到 value 高位
    for (usize i = 0; i < len; i++) {
        u8 bit_value = bits_read_bit_msb(data, start_bit + i);
        result = (result << 1) | (u64)bit_value;
    }

    *value = result;
    return BITS_UTIL_OK;
}

#endif
