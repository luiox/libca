#ifndef BITMAP_H
#define BITMAP_H 

#include "../base/datatype.h"

#define BITS_PER_BYTE  8
#define bitmap_size(n) (((n) + BITS_PER_BYTE - 1) / BITS_PER_BYTE)

// 设置某一位为1（标记为空闲）
static inline void bitmap_set(uint8_t* bitmap, size_t index)
{
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;
    bitmap[byte_idx] |= (1u << bit_idx);
}

// 清除某一位为0（标记为已分配）
static inline void bitmap_clear(uint8_t* bitmap, size_t index)
{
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;
    bitmap[byte_idx] &= ~(1u << bit_idx);
}

// 测试某一位的状态
static inline int bitmap_test(const uint8_t* bitmap, size_t index)
{
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;
    return (bitmap[byte_idx] & (1u << bit_idx)) != 0;
}

// 查找第一个空闲位
static int bitmap_find_first_set(const uint8_t* bitmap, size_t total_bits)
{
    size_t bytes = bitmap_size(total_bits);
    for (size_t i = 0; i < bytes; i++) {
        if (bitmap[i] != 0) {
            // 使用__builtin_ctz查找最低位的1（GCC内建函数）
            // 如果编译器不支持，可以用循环代替
            uint8_t byte = bitmap[i];
            for (int j = 0; j < 8; j++) {
                if (byte & (1u << j)) {
                    size_t index = i * 8 + j;
                    if (index < total_bits) {
                        return (int)index;
                    }
                }
            }
        }
    }
    return-1;  // 没有找到空闲块
}

#endif // !BITMAP_H
