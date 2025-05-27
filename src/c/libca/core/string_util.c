#include "string_util.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>

// 字符串拷贝
char* str_copy(char* dest, const char* src)
{
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    // 补一个\0
    *dest = '\0';
    return dest;
}

// 字符串拼接
char* str_concat(char* dest, const char* src)
{
    // 先找得到dest的\0
    while(*dest !='\0'){
        dest++;
    }
    // 拼接
    return str_copy(dest, src);
}

// 字符串长度
u32_t str_length(const char* str)
{
    u32_t len = 0;
    while (*str != '\0') {
        len++;
        str++;
    }
    return len;
}

// 字符串比较
i32_t str_compare(const char* str1, const char* str2)
{
    while (*str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2)
            return *str1 - *str2;
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

// 字符串搜索字串
char* str_find(const char* str, const char* substr)
{
    // TODO
    return NULL;
}

// 字符串替换
char* str_replace(char* str, const char* old, const char* new)
{
    // TODO
    return NULL;
}

// 去除字符串首尾的空格、\t、\n等空白字符
char* str_trim(char* str)
{
    if (str == NULL)
        return NULL;
    // 找到字符串开始的非空白字符
    char* p_start = str;
    while (*p_start != '\0' && (*p_start == ' ' || *p_start == '\t' || *p_start == '\n')) {
        p_start++;
    }

    // 找到字符串末尾的非空白字符
    char* p_end = str + strlen(str) - 1;
    while (p_end > p_start && (*p_end == ' ' || *p_end == '\t' || *p_end == '\n')) {
        p_end--;
    }

    // 将非空白字符移动到字符串的开始，并在末尾添加字符串终止符
    memmove(str, p_start, p_end - p_start + 1);
    str[p_end - p_start + 1] = '\0';

    return str;
}
