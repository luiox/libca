#include "base64.h"

// clang-format off
static const char base64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const u8 base64_decode_table[256] = {
    /* 0x00-0x0F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x10-0x1F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x20-0x2F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   62, 0xFF, 0xFF, 0xFF,   63,
    /* 0x30-0x3F */   52,   53,   54,   55,   56,   57,   58,   59,   60,   61, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x40-0x4F */ 0xFF,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
    /* 0x50-0x5F */   15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x60-0x6F */ 0xFF,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
    /* 0x70-0x7F */   41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x80-0xFF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// clang-format on

usize base64_encode_len(usize input_len)
{
    return ((input_len + 2) / 3) * 4;
}

usize base64_encode(const u8* input, usize input_len, char* output)
{
    if (!input || !output)
        return 0;

    usize j = 0;
    for (usize i = 0; i < input_len; i += 3) {
        u32 octet_a = i < input_len ? input[i] : 0;
        u32 octet_b = i + 1 < input_len ? input[i + 1] : 0;
        u32 octet_c = i + 2 < input_len ? input[i + 2] : 0;

        u32 triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = (i + 1 < input_len) ? base64_table[(triple >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < input_len) ? base64_table[triple & 0x3F] : '=';
    }
    output[j] = '\0';
    return j;
}

usize base64_decode_len(const char* input, usize input_len)
{
    if (!input || input_len == 0)
        return 0;

    /* 检查填充字符 */
    usize padding = 0;
    if (input_len >= 1 && input[input_len - 1] == '=')
        padding++;
    if (input_len >= 2 && input[input_len - 2] == '=')
        padding++;

    return (input_len / 4) * 3 - padding;
}

usize base64_decode(const char* input, usize input_len, u8* output)
{
    if (!input || !output || input_len == 0)
        return 0;
    if (input_len % 4 != 0)
        return 0; /* Base64 编码长度必须是4的倍数 */
    
    usize j          = 0;

    for (usize i = 0; i < input_len; i += 4) {
        u8 a = base64_decode_table[(u8)input[i]];
        u8 b = base64_decode_table[(u8)input[i + 1]];
        u8 c = base64_decode_table[(u8)input[i + 2]];
        u8 d = base64_decode_table[(u8)input[i + 3]];

        /* 检查非法字符 */
        if (a == 0xFF || b == 0xFF)
            return 0;
        /* c 和 d 可以是 '=' (填充)，对应的值是 0xFF，但我们需要区分 */
        bool c_is_padding = (input[i + 2] == '=');
        bool d_is_padding = (input[i + 3] == '=');

        if (c_is_padding && !d_is_padding)
            return 0; /* 填充格式错误 */
        if (c == 0xFF && !c_is_padding)
            return 0;
        if (d == 0xFF && !d_is_padding)
            return 0;

        u32 triple = ((u32)a << 18) | ((u32)b << 12);

        if (!c_is_padding) {
            triple |= ((u32)c << 6);
            output[j++] = (triple >> 16) & 0xFF;
            output[j++] = (triple >> 8) & 0xFF;

            if (!d_is_padding) {
                triple |= (u32)d;
                output[j++] = triple & 0xFF;
            }
        }
        else {
            /* 只有1个输出字节 */
            output[j++] = (triple >> 16) & 0xFF;
        }
    }

    return j;
}

