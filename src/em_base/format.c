#include "format.h"

#define FMT_SPRINTF_MAX ((usize)(~(usize)0))

static void fmt_buf_putc(char *buf, usize buf_size, usize *pos, char ch)
{
    if (buf_size > 0U && *pos + 1U < buf_size) {
        buf[*pos] = ch;
    }
    *pos = *pos + 1U;
}

static void fmt_buf_puts(char *buf, usize buf_size, usize *pos, const char *str)
{
    if (str == NULL) {
        str = "(null)";
    }

    while (*str != '\0') {
        fmt_buf_putc(buf, buf_size, pos, *str);
        str++;
    }
}

static void fmt_buf_put_u32_base(char *buf, usize buf_size, usize *pos, u32 value, u32 base, bool upper)
{
    char digits_lower[] = "0123456789abcdef";
    char digits_upper[] = "0123456789ABCDEF";
    char tmp[16];
    usize i = 0U;

    if (value == 0U) {
        fmt_buf_putc(buf, buf_size, pos, '0');
        return;
    }

    while (value != 0U && i < (usize)sizeof(tmp)) {
        u32 d = value % base;
        tmp[i++] = upper ? digits_upper[d] : digits_lower[d];
        value /= base;
    }

    while (i > 0U) {
        i--;
        fmt_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

static void fmt_buf_put_u32_width(char *buf, usize buf_size, usize *pos, u32 value, u32 min_width, char pad_char)
{
    char tmp[16];
    usize digits_len = 0U;
    usize pad_count;
    usize i;

    if (value == 0U) {
        tmp[digits_len++] = '0';
    }
    else {
        while (value != 0U && digits_len < (usize)sizeof(tmp)) {
            u32 d = value % 10U;
            tmp[digits_len++] = (char)('0' + d);
            value /= 10U;
        }
    }

    pad_count = min_width > digits_len ? (min_width - digits_len) : 0U;
    for (i = 0U; i < pad_count; i++) {
        fmt_buf_putc(buf, buf_size, pos, pad_char);
    }

    while (digits_len > 0U) {
        digits_len--;
        fmt_buf_putc(buf, buf_size, pos, tmp[digits_len]);
    }
}

static void fmt_buf_put_i32_width(char *buf, usize buf_size, usize *pos, i32 value, u32 min_width, char pad_char)
{
    bool negative = false;
    u32 abs_value;
    usize digits_len = 0U;
    usize total_len;
    usize pad_count;
    char tmp[16];
    usize i;

    if (value < 0) {
        negative = true;
        abs_value = (u32)(-(value + 1)) + 1U;
    }
    else {
        abs_value = (u32)value;
    }

    if (abs_value == 0U) {
        tmp[digits_len++] = '0';
    }
    else {
        while (abs_value != 0U && digits_len < (usize)sizeof(tmp)) {
            u32 d = abs_value % 10U;
            tmp[digits_len++] = (char)('0' + d);
            abs_value /= 10U;
        }
    }

    total_len = digits_len + (negative ? 1U : 0U);
    pad_count = min_width > total_len ? (min_width - total_len) : 0U;

    if (pad_char == '0') {
        if (negative) {
            fmt_buf_putc(buf, buf_size, pos, '-');
        }
        for (i = 0U; i < pad_count; i++) {
            fmt_buf_putc(buf, buf_size, pos, '0');
        }
    }
    else {
        for (i = 0U; i < pad_count; i++) {
            fmt_buf_putc(buf, buf_size, pos, pad_char);
        }
        if (negative) {
            fmt_buf_putc(buf, buf_size, pos, '-');
        }
    }

    while (digits_len > 0U) {
        digits_len--;
        fmt_buf_putc(buf, buf_size, pos, tmp[digits_len]);
    }
}

static void fmt_buf_put_i32(char *buf, usize buf_size, usize *pos, i32 value)
{
    fmt_buf_put_i32_width(buf, buf_size, pos, value, 0U, ' ');
}

static void fmt_buf_put_f64_trunc(char *buf, usize buf_size, usize *pos, f64 value, u32 precision)
{
    f64 abs_value = value;
    u32 int_part;
    f64 frac_part;

    if (abs_value < 0.0) {
        fmt_buf_putc(buf, buf_size, pos, '-');
        abs_value = -abs_value;
    }

    int_part = (u32)abs_value;
    frac_part = abs_value - (f64)int_part;

    fmt_buf_put_u32_base(buf, buf_size, pos, int_part, 10U, false);

    if (precision == 0U) {
        return;
    }

    fmt_buf_putc(buf, buf_size, pos, '.');
    while (precision > 0U) {
        u32 digit;
        frac_part *= 10.0;
        digit = (u32)frac_part;
        fmt_buf_putc(buf, buf_size, pos, (char)('0' + digit));
        frac_part -= (f64)digit;
        precision--;
    }
}

// 辅助函数：将无符号整数转换为字符串，返回写入的字符长度
// buf: 目标缓冲区
// val: 待转换的无符号整数
// return: 转换后的字符长度
usize u32_to_str(char *buf, u32 val) {
    char tmp[12]; // 32位整数最大10位，预留空间
    int i = 0;
    usize len = 0;

    // 特殊情况处理：0
    if (val == 0) {
        *buf = '0';
        return 1;
    }

    // 逆序生成数字字符
    while (val > 0) {
        tmp[i++] = (val % 10) + '0';
        val /= 10;
    }

    // 翻转回正序写入buf
    len = i;
    while (i > 0) {
        *buf++ = tmp[--i];
    }

    return len;
}

// 主函数：浮点转字符串
// buf: 目标缓冲区（需保证足够空间，建议至少20字节）
// val: 浮点数值
// decimal_num: 保留小数位数
void f32_to_str(char *buf, float val, u32 decimal_num) {
    char *p = buf;
    u32 int_part;
    u32 frac_part;
    u32 pow_ten = 1;
    int i;

    // 1. 处理符号
    if (val < 0) {
        *p++ = '-';
        val = -val; // 取绝对值进行后续处理
    }

    // 2. 分离整数部分
    // 强制转换直接截断小数
    int_part = (u32)val;

    // 3. 分离小数部分
    float frac_val = val - (float)int_part;

    // 4. 计算10的N次方
    // 这是一个简单的整数幂运算，仅用于小数位数较少的情况
    for (i = 0; i < (int)decimal_num; i++) {
        pow_ten *= 10;
    }

    // 5. 提取小数部分的整数值 (截断法)
    // 例如：0.5678 * 100 = 56.78 -> int转换为 56
    // 注意：此处存在浮点精度误差，例如可能得到 55 或 56，符合“无需特别精准”的要求
    if (pow_ten > 0) {
        frac_part = (u32)(frac_val * pow_ten);
    } else {
        frac_part = 0;
    }

    // 6. 转换整数部分
    p += u32_to_str(p, int_part);

    // 7. 处理小数部分
    if (decimal_num > 0) {
        *p++ = '.'; // 添加小数点

        // 关键点：处理小数部分的前导零
        // 例如：整数部分是1，小数部分计算结果为 5 (期望是 1.05)
        // 如果直接转换 "5"，结果会是 "1.5"，漏掉了一个0。
        // 我们需要根据位数补0。
        u32 temp_pow = pow_ten / 10;
        while (temp_pow > 0 && frac_part < temp_pow) {
            *p++ = '0';
            temp_pow /= 10;
        }

        // 转换小数部分的数字
        // 如果小数部分为0，也要确保打印出 ".00" 这样的格式
        if (frac_part > 0) {
            p += u32_to_str(p, frac_part);
        } else {
            // 如果计算出的小数部分是0，补齐所需的0个数
            for (i = 0; i < (int)decimal_num; i++) {
                *p++ = '0';
            }
        }
    }

    // 8. 结束符
    *p = '\0';
}

i32 fmt_vsnprintf(char *buf, usize buf_size, const char *fmt, va_list args)
{
    usize pos = 0U;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    while (*fmt != '\0') {
        if (*fmt != '%') {
            fmt_buf_putc(buf, buf_size, &pos, *fmt);
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '%') {
            fmt_buf_putc(buf, buf_size, &pos, '%');
            fmt++;
            continue;
        }

        bool zero_pad = false;
        u32 width = 0U;
        u32 precision = 6U;
        bool has_precision = false;

        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10U + (u32)(*fmt - '0');
            fmt++;
        }

        if (*fmt == '.') {
            has_precision = true;
            precision = 0U;
            fmt++;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10U + (u32)(*fmt - '0');
                fmt++;
            }
        }

        switch (*fmt) {
        case 'd': {
            i32 value = (i32)va_arg(args, int);
            if (width > 0U) {
                fmt_buf_put_i32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                fmt_buf_put_i32(buf, buf_size, &pos, value);
            }
            break;
        }
        case 'u': {
            u32 value = (u32)va_arg(args, unsigned int);
            if (width > 0U) {
                fmt_buf_put_u32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                fmt_buf_put_u32_base(buf, buf_size, &pos, value, 10U, false);
            }
            break;
        }
        case 'x': {
            u32 value = (u32)va_arg(args, unsigned int);
            fmt_buf_put_u32_base(buf, buf_size, &pos, value, 16U, false);
            break;
        }
        case 'X': {
            u32 value = (u32)va_arg(args, unsigned int);
            fmt_buf_put_u32_base(buf, buf_size, &pos, value, 16U, true);
            break;
        }
        case 's': {
            const char *value = va_arg(args, const char *);
            fmt_buf_puts(buf, buf_size, &pos, value);
            break;
        }
        case 'f': {
            f64 value = (f64)va_arg(args, double);
            fmt_buf_put_f64_trunc(buf, buf_size, &pos, value, precision);
            break;
        }
        case '\0':
            fmt--;
            break;
        default:
            fmt_buf_putc(buf, buf_size, &pos, '%');
            if (has_precision) {
                fmt_buf_putc(buf, buf_size, &pos, '.');
            }
            if (*fmt != '\0') {
                fmt_buf_putc(buf, buf_size, &pos, *fmt);
            }
            break;
        }

        if (*fmt != '\0') {
            fmt++;
        }
    }

    if (pos >= buf_size) {
        buf[buf_size - 1U] = '\0';
        return (i32)(buf_size - 1U);
    }

    buf[pos] = '\0';
    return (i32)pos;
}

i32 fmt_sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    i32 len;

    if (buf == NULL || fmt == NULL) {
        return 0;
    }

    va_start(args, fmt);
    len = fmt_vsnprintf(buf, FMT_SPRINTF_MAX, fmt, args);
    va_end(args);

    return len;
}
