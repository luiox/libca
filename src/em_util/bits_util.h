#ifndef LIBCA_EM_UTIL_BITS_UTIL_H
#define LIBCA_EM_UTIL_BITS_UTIL_H

#include "../em_base/datatype.h"

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

#endif // !LIBCA_EM_UTIL_BITS_UTIL_H