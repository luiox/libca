#include "string_util.h"
#include <string.h>

// 添加静态变量用于str_tok函数的状态保持
static char* strtok_pos = NULL;

char* str_cpy(char* dest, const char* src, usize size)
{
    if (size == 0)
        return dest;

    char* ret = dest;
    usize i;

    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';   // 确保终止符

    return ret;
}

bool hex_str_to_uint(const char* str, u32* out_value)
{
    if (!str || !out_value)
        return false;

    u32 value = 0;
    while (*str) {
        char c = *str;
        u32  digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        }
        else if (c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        }
        else if (c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        }
        else {
            return false;   // 非法字符
        }

        if (value > (UINT32_MAX - digit) / 16) {
            return false;   // 溢出
        }

        value = value * 16 + digit;
        str++;
    }

    *out_value = value;
    return true;
}


void uint_to_hex_str(u32 value, char* out_str, size_t out_size)
{
    if (!out_str || out_size == 0)
        return;

    const char* digits = "0123456789ABCDEF";
    usize       pos    = out_size - 1;
    out_str[pos]       = '\0';   // 终止符

    do {
        pos--;
        out_str[pos] = digits[value & 0xF];
        value >>= 4;
    } while (pos > 0 && value != 0);

    // 如果还有空间，左移字符串
    if (pos > 0) {
        memmove(out_str, &out_str[pos], out_size - pos);
    }
}

usize str_len(const char* str)
{
    if (!str) return 0;
    
    usize len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int str_cmp(const char* s1, const char* s2, usize size)
{
    if (!s1 || !s2) return -1;
    
    for (usize i = 0; i < size; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            break;
        }
    }
    return 0;
}

char* str_chr(const char* str, char c)
{
    if (!str) return NULL;
    
    while (*str) {
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

char* str_str(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    
    usize needle_len = str_len(needle);
    if (needle_len == 0) return (char*)haystack;
    
    while (*haystack) {
        if (str_cmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    return NULL;
}

void str_to_upper(char* str)
{
    if (!str) return;
    
    while (*str) {
        if (*str >= 'a' && *str <= 'z') {
            *str = *str - 'a' + 'A';
        }
        str++;
    }
}

void str_to_lower(char* str)
{
    if (!str) return;
    
    while (*str) {
        if (*str >= 'A' && *str <= 'Z') {
            *str = *str - 'A' + 'a';
        }
        str++;
    }
}

void str_reverse(char* str)
{
    if (!str) return;
    
    usize len = str_len(str);
    if (len <= 1) return;
    
    usize i, j;
    for (i = 0, j = len - 1; i < j; i++, j--) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

char* str_tok(char* str, const char* delim)
{
    // 如果str不为NULL，则开始新的字符串解析
    if (str) {
        strtok_pos = str;
    } 
    // 如果str为NULL且strtok_pos为NULL，则返回NULL
    else if (!strtok_pos) {
        return NULL;
    }
    
    // 跳过起始的分隔符
    while (*strtok_pos) {
        if (!str_chr(delim, *strtok_pos)) {
            break;
        }
        strtok_pos++;
    }
    
    // 如果到达字符串末尾，返回NULL
    if (*strtok_pos == '\0') {
        strtok_pos = NULL;
        return NULL;
    }
    
    // 找到token的起始位置
    char* token_start = strtok_pos;
    
    // 查找下一个分隔符
    while (*strtok_pos) {
        if (str_chr(delim, *strtok_pos)) {
            *strtok_pos = '\0';  // 将分隔符替换为字符串终止符
            strtok_pos++;        // 移动到下一个字符
            return token_start;
        }
        strtok_pos++;
    }
    
    // 到达字符串末尾，没有更多分隔符
    strtok_pos = NULL;
    return token_start;
}
