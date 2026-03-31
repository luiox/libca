#include "bits_util.h"

/**
 * @brief Read a single bit from MSB-first bitstream.
 * @param data Source buffer.
 * @param bit_index Bit index in MSB-first stream.
 * @return 0 or 1.
 */
static u8 bits_read_bit_msb(const u8* data, usize bit_index)
{
    usize byte_index = bit_index >> 3;
    u8    bit_in_byte = (u8)(7u - (bit_index & 0x7u));
    return (u8)((data[byte_index] >> bit_in_byte) & 0x01u);
}

/**
 * @brief Write a single bit into MSB-first bitstream.
 * @param data Target buffer.
 * @param bit_index Bit index in MSB-first stream.
 * @param bit_value 0 or 1.
 */
static void bits_write_bit_msb(u8* data, usize bit_index, u8 bit_value)
{
    usize byte_index = bit_index >> 3;
    u8    bit_in_byte = (u8)(7u - (bit_index & 0x7u));
    u8    mask = (u8)(1u << bit_in_byte);

    if (bit_value != 0u) {
        data[byte_index] |= mask;
    } else {
        data[byte_index] &= (u8)~mask;
    }
}

/**
 * @brief Validate common parameters for bit read/write.
 * @param data Source or target buffer.
 * @param data_size Buffer size in bytes.
 * @param start_bit Start bit index.
 * @param length Bit length.
 * @param max_len Maximum supported bit length.
 * @return 0 on success, negative on error.
 */
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

/**
 * @brief Mask helper for u32.
 * @param length Bit length.
 * @return Mask with low length bits set.
 */
static u32 bits_mask_low_u32(u8 length)
{
    if (length >= 32) {
        return 0xFFFFFFFFu;
    }
    return (u32)((1u << length) - 1u);
}

#ifdef HAS_INT64
/**
 * @brief Mask helper for u64.
 * @param length Bit length.
 * @return Mask with low length bits set.
 */
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

    for (usize i = 0; i < len; i++) {
        u8 bit_value = bits_read_bit_msb(data, start_bit + i);
        result = (result << 1) | (u64)bit_value;
    }

    *value = result;
    return BITS_UTIL_OK;
}

#endif
