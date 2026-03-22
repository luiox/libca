/* Auto-migrated from src/em_base/string_util.c test blocks */
#include "string_util.h"

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
