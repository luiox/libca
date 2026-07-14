#include "string_util.h"

// 字符转小写（私有函数）
static char private_char_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

// 判断字符是否为空白字符（空格、制表、换行、回车）
static inline bool private_char_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char char_to_lower(char c)
{
    return private_char_to_lower(c);
}

char char_to_upper(char c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

#if USE_CUSTOM_STRING_UTIL_IMPL

usize str_len(const char* str)
{
    if (!str) {
        return 0;
    }

    usize len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

usize str_nlen(const char* str, usize max_len)
{
    if (!str) {
        return 0;
    }

    usize len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }
    return len;
}

i32 str_cmp(const char* s1, const char* s2, usize size)
{
    if (s1 == s2 || size == 0) {
        return 0;
    }
    if (!s1) {
        return -1;
    }
    if (!s2) {
        return 1;
    }

    for (usize i = 0; i < size; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (i32)((u8)s1[i] - (u8)s2[i]);
        }
    }

    return 0;
}

char* str_find_ch(const char* str, char c)
{
    if (!str) {
        return NULL;
    }

    while (*str) {
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }

    // find \0
    if (c == '\0') {
        return (char*)str;
    }

    return NULL;
}

char* str_find_str(const char* haystack, const char* needle)
{
    if (!haystack || !needle) {
        return NULL;
    }

    if (*needle == '\0') {
        return (char*)haystack;
    }

    for (const char* h = haystack; *h != '\0'; h++) {
        if (*h == *needle) {
            const char* h_sub = h;
            const char* n_sub = needle;
            while (*h_sub != '\0' && *n_sub != '\0' && *h_sub == *n_sub) {
                h_sub++;
                n_sub++;
            }
            if (*n_sub == '\0') {
                return (char*)h;
            }
        }
    }

    return NULL;
}

#endif /* USE_CUSTOM_STRING_UTIL_IMPL */

/* ==================== 始终使用自定义实现的函数 ==================== */

i32 str_cpy(char* dest, const char* src, usize size)
{
    if (!dest || !src) {
        return STR_ERR_NULL;
    }
    if (size == 0) {
        return STR_ERR_INVALID;
    }

    usize i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';

    // 如果源字符串没拷完，说明空间不足
    if (src[i] != '\0') {
        return STR_ERR_SIZE;
    }

    return (i32)i;
}

i32 str_cat(char* dest, const char* src, usize dest_max_size)
{
    if (!dest || !src) {
        return STR_ERR_NULL;
    }
    if (dest_max_size == 0) {
        return STR_ERR_INVALID;
    }

    usize dest_len = str_nlen(dest, dest_max_size);
    if (dest_len >= dest_max_size) {
        return STR_ERR_SIZE;
    }

    usize i;
    usize remaining = dest_max_size - dest_len;
    
    for (i = 0; i < remaining - 1 && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';

    if (src[i] != '\0') {
        return STR_ERR_SIZE;
    }

    return (i32)(dest_len + i);
}

bool str_is_equal(const char* s1, const char* s2)
{
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return (*s1 == *s2);
}

i32 str_trim(char* str)
{
    if (!str) {
        return 0;
    }

    str_ltrim(str);
    return str_rtrim(str);
}

i32 str_ltrim(char* str)
{
    if (!str) {
        return 0;
    }

    usize start = 0;
    while (str[start] != '\0' && private_char_is_space(str[start])) {
        start++;
    }

    if (start > 0) {
        usize new_len = 0;
        usize i = start;
        while (str[i] != '\0') {
            str[new_len++] = str[i++];
        }
        str[new_len] = '\0';
        return (i32)new_len;
    }

    return (i32)str_len(str);
}

i32 str_rtrim(char* str)
{
    if (!str) {
        return 0;
    }

    usize len = str_len(str);
    if (len == 0) {
        return 0;
    }

    i32 end = (i32)len - 1;
    while (end >= 0 && private_char_is_space(str[end])) {
        str[end] = '\0';
        end--;
    }

    return (i32)(end + 1);
}

void str_to_upper(char* str)
{
    if (!str) {
        return;
    }

    while (*str) {
        if (*str >= 'a' && *str <= 'z') {
            *str -= 'a' - 'A';
        }
        str++;
    }
}

void str_to_lower(char* str)
{
    if (!str) {
        return;
    }

    while (*str) {
        if (*str >= 'A' && *str <= 'Z') {
            *str += 'a' - 'A';
        }
        str++;
    }
}

void str_reverse(char* str)
{
    if (!str) {
        return;
    }

    usize len = str_len(str);
    if (len <= 1) {
        return;
    }

    usize i = 0;
    usize j = len - 1;
    while (i < j) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

bool str_starts_with(const char* str, const char* prefix)
{
    if (!str || !prefix) {
        return false;
    }

    while (*prefix) {
        if (*str != *prefix) {
            return false;
        }
        str++;
        prefix++;
    }

    return true;
}

bool str_starts_with_i(const char* str, const char* prefix)
{
    if (!str || !prefix) {
        return false;
    }

    while (*prefix) {
        if (private_char_to_lower(*str) != private_char_to_lower(*prefix)) {
            return false;
        }
        str++;
        prefix++;
    }

    return true;
}

bool str_ends_with(const char* str, const char* suffix)
{
    if (!str || !suffix) {
        return false;
    }

    usize str_len_val = str_len(str);
    usize suffix_len = str_len(suffix);

    if (suffix_len > str_len_val) {
        return false;
    }

    const char* str_end = str + str_len_val - suffix_len;
    while (*suffix) {
        if (*str_end != *suffix) {
            return false;
        }
        str_end++;
        suffix++;
    }

    return true;
}

bool str_ends_with_i(const char* str, const char* suffix)
{
    if (!str || !suffix) {
        return false;
    }

    usize str_len_val = str_len(str);
    usize suffix_len = str_len(suffix);

    if (suffix_len > str_len_val) {
        return false;
    }

    const char* str_end = str + str_len_val - suffix_len;
    while (*suffix) {
        if (private_char_to_lower(*str_end) != private_char_to_lower(*suffix)) {
            return false;
        }
        str_end++;
        suffix++;
    }

    return true;
}




///////////////////////////////////////////////////////////////////////////////
// 暂时先不加入，还没有定义好标准
#if 0

// 十六进制转换
char* to_hex(const void* data, u32 data_len, char* buf, u32 buf_len);

/**
 * @brief 十六进制字符串转整数
 * 
 * @param str 字符串
 * @param out_value 输出值
 * @return bool 成功返回 true
 */
bool hex_str_to_uint(const char* str, u32* out_value);

/**
 * @brief 整数转十六进制字符串
 * 
 * @param value 整数
 * @param out_str 输出缓冲区
 * @param out_size 缓冲区大小
 */
void uint_to_hex_str(u32 value, char* out_str, usize out_size);

#endif
