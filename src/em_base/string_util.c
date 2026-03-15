#include "string_util.h"

/**
 * @brief 字符转小写（私有函数）
 */
static char private_char_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

/**
 * @brief 判断字符是否为空白字符（空格、制表、换行、回车）
 */
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

#if TEST_ENABLE
#include <em_test/test.h>

TEST_CASE(test_char_to_lower)
{
    TEST_ASSERT_EQUAL_INT('a', char_to_lower('A'));
    TEST_ASSERT_EQUAL_INT('a', char_to_lower('a'));
    TEST_ASSERT_EQUAL_INT('1', char_to_lower('1'));
}

TEST_CASE(test_char_to_upper)
{
    TEST_ASSERT_EQUAL_INT('A', char_to_upper('a'));
    TEST_ASSERT_EQUAL_INT('A', char_to_upper('A'));
    TEST_ASSERT_EQUAL_INT('1', char_to_upper('1'));
}

TEST_CASE(test_str_len)
{
    TEST_ASSERT_EQUAL_UINT(0, str_len(NULL));
    TEST_ASSERT_EQUAL_UINT(0, str_len(""));
    TEST_ASSERT_EQUAL_UINT(5, str_len("hello"));
}

TEST_CASE(test_str_nlen)
{
    TEST_ASSERT_EQUAL_UINT(0, str_nlen(NULL, 10));
    TEST_ASSERT_EQUAL_UINT(5, str_nlen("hello", 10));
    TEST_ASSERT_EQUAL_UINT(3, str_nlen("hello", 3));
    TEST_ASSERT_EQUAL_UINT(0, str_nlen("hello", 0));
    
    // 边界情况：max_len 正好在字符串中间截断
    TEST_ASSERT_EQUAL_UINT(3, str_nlen("hello", 3));
}

TEST_CASE(test_str_cpy)
{
    char buf[10];
    // 正常拷贝
    TEST_ASSERT(str_cpy(buf, "hello", 10) == 5);
    TEST_ASSERT_EQUAL_STRING("hello", buf);
    
    // 拷贝空字符串
    TEST_ASSERT_EQUAL_INT(0, str_cpy(buf, "", 10));
    TEST_ASSERT_EQUAL_STRING("", buf);
    
    // 溢出检查
    TEST_ASSERT_EQUAL_INT(STR_ERR_SIZE, str_cpy(buf, "hello world", 5));
    
    // 边界：刚好装满 (4字符 + \0)
    TEST_ASSERT(str_cpy(buf, "1234", 5) == 4);
    
    // 异常输入
    TEST_ASSERT_EQUAL_INT(STR_ERR_NULL, str_cpy(NULL, "test", 10));
    TEST_ASSERT_EQUAL_INT(STR_ERR_NULL, str_cpy(buf, NULL, 10));
    TEST_ASSERT_EQUAL_INT(STR_ERR_INVALID, str_cpy(buf, "test", 0));
}

TEST_CASE(test_str_cat)
{
    char buf[20] = "hello";
    // 正常拼接
    TEST_ASSERT(str_cat(buf, " world", 20) == 11);
    TEST_ASSERT_EQUAL_STRING("hello world", buf);
    
    // 溢出检查
    TEST_ASSERT_EQUAL_INT(STR_ERR_SIZE, str_cat(buf, " again", 12));
    
    // 异常输入
    TEST_ASSERT_EQUAL_INT(STR_ERR_NULL, str_cat(NULL, "test", 10));
    TEST_ASSERT_EQUAL_INT(STR_ERR_NULL, str_cat(buf, NULL, 10));
    TEST_ASSERT_EQUAL_INT(STR_ERR_INVALID, str_cat(buf, "test", 0));
    
    // 目标缓冲区无空间或无终止符
    char full_buf[5] = "12345"; 
    TEST_ASSERT_EQUAL_INT(STR_ERR_SIZE, str_cat(full_buf, "test", 5));
}

TEST_CASE(test_str_cmp)
{
    TEST_ASSERT_EQUAL_INT(0, str_cmp("abc", "abc", 3));
    TEST_ASSERT(str_cmp("abc", "abd", 3) < 0);
    TEST_ASSERT(str_cmp("abd", "abc", 3) > 0);
    TEST_ASSERT_EQUAL_INT(0, str_cmp("abc", "abcd", 3));
    
    // NULL 比较逻辑
    TEST_ASSERT_EQUAL_INT(0, str_cmp(NULL, NULL, 3));
    TEST_ASSERT(str_cmp(NULL, "abc", 3) < 0);
    TEST_ASSERT(str_cmp("abc", NULL, 3) > 0);
    
    TEST_ASSERT_EQUAL_INT(0, str_cmp("abc", "abc", 0));
}

TEST_CASE(test_str_is_equal)
{
    TEST_ASSERT_TRUE(str_is_equal("abc", "abc"));
    TEST_ASSERT_FALSE(str_is_equal("abc", "abd"));
    TEST_ASSERT_FALSE(str_is_equal("abc", "abcd"));
    
    // 指针相等
    const char* s = "ptr";
    TEST_ASSERT_TRUE(str_is_equal(s, s));
    
    // NULL 检查
    TEST_ASSERT_FALSE(str_is_equal(NULL, "abc"));
    TEST_ASSERT_FALSE(str_is_equal("abc", NULL));
}

TEST_CASE(test_str_find_ch)
{
    const char* s = "hello";
    TEST_ASSERT(str_find_ch(s, 'e') == s + 1);
    TEST_ASSERT(str_find_ch(s, 'z') == NULL);
    TEST_ASSERT(str_find_ch(s, '\0') == s + 5);
    TEST_ASSERT(str_find_ch(NULL, 'a') == NULL);
}

TEST_CASE(test_str_find_str)
{
    const char* s = "hello world";
    TEST_ASSERT(str_find_str(s, "world") == s + 6);
    TEST_ASSERT(str_find_str(s, "hello") == s);
    TEST_ASSERT(str_find_str(s, "earth") == NULL);
    
    // 边缘情况
    TEST_ASSERT(str_find_str(s, "") == s);
    TEST_ASSERT(str_find_str(NULL, "test") == NULL);
    TEST_ASSERT(str_find_str("test", NULL) == NULL);
}

TEST_CASE(test_str_trim)
{
    char s1[] = "  hello  ";
    TEST_ASSERT(str_trim(s1) == 5);
    TEST_ASSERT_EQUAL_STRING("hello", s1);

    char s2[] = "\t\n world \r";
    TEST_ASSERT(str_trim(s2) == 5);
    TEST_ASSERT_EQUAL_STRING("world", s2);
    
    // 无需 trim 的字符串
    char s_no_trim[] = "hello";
    TEST_ASSERT_EQUAL_INT(5, str_trim(s_no_trim));
    TEST_ASSERT_EQUAL_STRING("hello", s_no_trim);
    
    // 全空格/空字符串
    char s3[] = "   ";
    TEST_ASSERT(str_trim(s3) == 0);
    TEST_ASSERT_EQUAL_STRING("", s3);
    
    char s4[] = "";
    TEST_ASSERT(str_trim(s4) == 0);
    
    TEST_ASSERT(str_trim(NULL) == 0);
}

TEST_CASE(test_str_ltrim)
{
    char s1[] = "  hello  ";
    TEST_ASSERT_EQUAL_INT(7, str_ltrim(s1));
    TEST_ASSERT_EQUAL_STRING("hello  ", s1);

    char s2[] = "\t\n world";
    TEST_ASSERT_EQUAL_INT(5, str_ltrim(s2));
    TEST_ASSERT_EQUAL_STRING("world", s2);

    char s3[] = "hello";
    TEST_ASSERT_EQUAL_INT(5, str_ltrim(s3));
    TEST_ASSERT_EQUAL_STRING("hello", s3);

    char s4[] = "   ";
    TEST_ASSERT_EQUAL_INT(0, str_ltrim(s4));
    TEST_ASSERT_EQUAL_STRING("", s4);

    TEST_ASSERT_EQUAL_INT(0, str_ltrim(NULL));
}

TEST_CASE(test_str_rtrim)
{
    char s1[] = "  hello  ";
    TEST_ASSERT_EQUAL_INT(7, str_rtrim(s1));
    TEST_ASSERT_EQUAL_STRING("  hello", s1);

    char s2[] = "world\t\n ";
    TEST_ASSERT_EQUAL_INT(5, str_rtrim(s2));
    TEST_ASSERT_EQUAL_STRING("world", s2);

    char s3[] = "hello";
    TEST_ASSERT_EQUAL_INT(5, str_rtrim(s3));
    TEST_ASSERT_EQUAL_STRING("hello", s3);

    char s4[] = "   ";
    TEST_ASSERT_EQUAL_INT(0, str_rtrim(s4));
    TEST_ASSERT_EQUAL_STRING("", s4);

    TEST_ASSERT_EQUAL_INT(0, str_rtrim(NULL));
}

TEST_CASE(test_str_to_upper)
{
    char s[] = "hello123";
    str_to_upper(s);
    TEST_ASSERT_EQUAL_STRING("HELLO123", s);
    
    // 异常输入覆盖
    str_to_upper(NULL);
}

TEST_CASE(test_str_to_lower)
{
    char s[] = "HELLO123";
    str_to_lower(s);
    TEST_ASSERT_EQUAL_STRING("hello123", s);

    // 异常输入覆盖
    str_to_lower(NULL);
}

TEST_CASE(test_str_reverse)
{
    char s1[] = "hello";
    str_reverse(s1);
    TEST_ASSERT_EQUAL_STRING("olleh", s1);
    
    char s2[] = "a";
    str_reverse(s2);
    TEST_ASSERT_EQUAL_STRING("a", s2);

    char s3[] = "";
    str_reverse(s3);
    TEST_ASSERT_EQUAL_STRING("", s3);

    // 异常输入覆盖
    str_reverse(NULL);
}

TEST_CASE(test_str_starts_with)
{
    TEST_ASSERT_TRUE(str_starts_with("hello", "he"));
    TEST_ASSERT_FALSE(str_starts_with("hello", "ha"));
    
    // 边缘情况
    TEST_ASSERT_FALSE(str_starts_with("hi", "hello")); // 前缀比主串长
    TEST_ASSERT_TRUE(str_starts_with("hello", ""));    // 空前缀
    TEST_ASSERT_TRUE(str_starts_with("", ""));         // 双方都为空
    
    // 异常输入
    TEST_ASSERT_FALSE(str_starts_with(NULL, "he"));
    TEST_ASSERT_FALSE(str_starts_with("hello", NULL));
}

TEST_CASE(test_str_starts_with_i)
{
    TEST_ASSERT_TRUE(str_starts_with_i("hello", "HE"));
    TEST_ASSERT_FALSE(str_starts_with_i("hello", "HA"));

    // 异常输入
    TEST_ASSERT_FALSE(str_starts_with_i(NULL, "HE"));
    TEST_ASSERT_FALSE(str_starts_with_i("hello", NULL));
}

TEST_CASE(test_str_ends_with)
{
    TEST_ASSERT_TRUE(str_ends_with("hello", "lo"));
    TEST_ASSERT_FALSE(str_ends_with("hello", "la"));

    // 边缘情况
    TEST_ASSERT_FALSE(str_ends_with("hi", "hello")); // 后缀比主串长
    TEST_ASSERT_TRUE(str_ends_with("hello", ""));    // 空后缀
    TEST_ASSERT_TRUE(str_ends_with("", ""));         // 双方都为空

    // 异常输入
    TEST_ASSERT_FALSE(str_ends_with(NULL, "lo"));
    TEST_ASSERT_FALSE(str_ends_with("hello", NULL));
}

TEST_CASE(test_str_ends_with_i)
{
    TEST_ASSERT_TRUE(str_ends_with_i("hello", "LO"));
    TEST_ASSERT_FALSE(str_ends_with_i("hello", "LA"));

    // 边缘情况
    TEST_ASSERT_FALSE(str_ends_with_i("hi", "hello")); // 后缀比主串长
    TEST_ASSERT_TRUE(str_ends_with_i("hello", ""));    // 空后缀
    TEST_ASSERT_TRUE(str_ends_with_i("", ""));         // 双方都为空

    // 异常输入
    TEST_ASSERT_FALSE(str_ends_with_i(NULL, "LO"));
    TEST_ASSERT_FALSE(str_ends_with_i("hello", NULL));
}
#endif



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
