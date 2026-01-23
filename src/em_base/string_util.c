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

// 字符串拼接
char* str_cat(char* dest, const char* src, usize dest_max_size)
{
    // 先找得到dest的\0
    while(*dest !='\0'){
        dest++;
    }
    // 拼接
    return str_cpy(dest, src, dest_max_size - (dest - dest));
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

char* str_find_ch(const char* str, char c)
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

char* str_find_str(const char* haystack, const char* needle)
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

// 字符串分割（类似 strtok）
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

///////////////////////////////////////////////////////////////////////////////

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





#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(test_str_len_case)
{
    TEST_ASSERT_EQUAL_INT(str_len("abc"), 3);
    TEST_ASSERT_EQUAL_INT(str_len(""), 0);
}

TEST_CASE(test_str_cpy_case)
{
    char buf[10];
    str_cpy(buf, "hello", sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(buf, "hello");
}

TEST_CASE(test_str_cmp_case)
{
    TEST_ASSERT_EQUAL_INT(str_cmp("abc", "abc", 3), 0);
    TEST_ASSERT(str_cmp("abc", "abd", 3) < 0);
}

TEST_CASE(test_str_chr_case)
{
    TEST_ASSERT(str_chr("hello", 'e') != NULL);
    TEST_ASSERT(str_chr("hello", 'z') == NULL);
}

TEST_CASE(test_str_str_case)
{
    TEST_ASSERT(str_str("abcdef", "cd") != NULL);
    TEST_ASSERT(str_str("abcdef", "gh") == NULL);
}

TEST_CASE(test_str_tok_case)
{
    char  buf2[32] = "a,b,c";
    char* token    = str_tok(buf2, ",");
    TEST_ASSERT_EQUAL_STRING(token, "a");
    token = str_tok(NULL, ",");
    TEST_ASSERT_EQUAL_STRING(token, "b");
    token = str_tok(NULL, ",");
    TEST_ASSERT_EQUAL_STRING(token, "c");
    token = str_tok(NULL, ",");
    TEST_ASSERT(token == NULL);
}

TEST_CASE(test_str_to_upper_case)
{
    char buf3[16] = "abcDEF";
    str_to_upper(buf3);
    TEST_ASSERT_EQUAL_STRING(buf3, "ABCDEF");
}

TEST_CASE(test_str_to_lower_case)
{
    char buf4[16] = "ABCdef";
    str_to_lower(buf4);
    TEST_ASSERT_EQUAL_STRING(buf4, "abcdef");
}

TEST_CASE(test_str_reverse_case)
{
    char buf5[16] = "abcdef";
    str_reverse(buf5);
    TEST_ASSERT_EQUAL_STRING(buf5, "fedcba");
}

TEST_CASE(test_hex_str_to_uint_case)
{
    u32 val = 0;
    TEST_ASSERT(hex_str_to_uint("1A3F", &val));
    TEST_ASSERT_EQUAL_INT(val, 0x1A3F);
    TEST_ASSERT(!hex_str_to_uint("xyz", &val));
}

TEST_CASE(test_uint_to_hex_str_case)
{
    char buf6[9];
    uint_to_hex_str(0x1A3F, buf6, sizeof(buf6));
    TEST_ASSERT_EQUAL_STRING(buf6, "1A3F");
}

#endif


// #include <libca/core/string.h>
// #include <stdio.h>
// #include <libca/core/test.h>

// TEST_CASE(str_trim)
// {
//     char buf[512] = "  \n \t 21345\n\t ";
//     printf("%s1", str_trim(buf));
// }

// // int main(int argc, char* argv[])
// // {
// //     test1();
// //     return 0;
// // }
