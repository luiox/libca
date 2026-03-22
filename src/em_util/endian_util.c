#include "endian_util.h"

static i8 endian_flag = -1;
// 是否是小端序
bool is_little_endian(void)
{
    if (endian_flag != -1) {
        return endian_flag == 1;
    }
    union
    {
        u8  bytes[2];
        u16 value;
    } u;
    u.value = 0x0102;
    if (u.bytes[0] == 0x01) {
        endian_flag = 1;
        return true;
    }
    endian_flag = 0;
    return false;
}

// 是否是大端序
bool is_big_endian(void)
{
    return !is_little_endian();
}

