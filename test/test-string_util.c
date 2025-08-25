#include "base/string_util.h"
#include "test_helper.h"
#include <string.h>

TEST_CASE_BEGIN(test_str_len_case)
    ASSERT_EQ(str_len("abc"), 3);
    ASSERT_EQ(str_len(""), 0);
TEST_CASE_END(test_str_len_case)

TEST_CASE_BEGIN(test_str_cpy_case)
    char buf[10];
    str_cpy(buf, "hello", sizeof(buf));
    ASSERT_STR_EQ(buf, "hello");
TEST_CASE_END(test_str_cpy_case)

TEST_CASE_BEGIN(test_str_cmp_case)
    ASSERT_EQ(str_cmp("abc", "abc", 3), 0);
    ASSERT_TRUE(str_cmp("abc", "abd", 3) < 0);
TEST_CASE_END(test_str_cmp_case)

TEST_CASE_BEGIN(test_str_chr_case)
    ASSERT_TRUE(str_chr("hello", 'e') != NULL);
    ASSERT_TRUE(str_chr("hello", 'z') == NULL);
TEST_CASE_END(test_str_chr_case)

TEST_CASE_BEGIN(test_str_str_case)
    ASSERT_TRUE(str_str("abcdef", "cd") != NULL);
    ASSERT_TRUE(str_str("abcdef", "gh") == NULL);
TEST_CASE_END(test_str_str_case)

TEST_CASE_BEGIN(test_str_tok_case)
    char buf2[32] = "a,b,c";
    char* token = str_tok(buf2, ",");
    ASSERT_STR_EQ(token, "a");
    token = str_tok(NULL, ",");
    ASSERT_STR_EQ(token, "b");
    token = str_tok(NULL, ",");
    ASSERT_STR_EQ(token, "c");
    token = str_tok(NULL, ",");
    ASSERT_TRUE(token == NULL);
TEST_CASE_END(test_str_tok_case)

TEST_CASE_BEGIN(test_str_to_upper_case)
    char buf3[16] = "abcDEF";
    str_to_upper(buf3);
    ASSERT_STR_EQ(buf3, "ABCDEF");
TEST_CASE_END(test_str_to_upper_case)

TEST_CASE_BEGIN(test_str_to_lower_case)
    char buf4[16] = "ABCdef";
    str_to_lower(buf4);
    ASSERT_STR_EQ(buf4, "abcdef");
TEST_CASE_END(test_str_to_lower_case)

TEST_CASE_BEGIN(test_str_reverse_case)
    char buf5[16] = "abcdef";
    str_reverse(buf5);
    ASSERT_STR_EQ(buf5, "fedcba");
TEST_CASE_END(test_str_reverse_case)

TEST_CASE_BEGIN(test_hex_str_to_uint_case)
    u32 val = 0;
    ASSERT_TRUE(hex_str_to_uint("1A3F", &val));
    ASSERT_EQ(val, 0x1A3F);
    ASSERT_TRUE(!hex_str_to_uint("xyz", &val));
TEST_CASE_END(test_hex_str_to_uint_case)

TEST_CASE_BEGIN(test_uint_to_hex_str_case)
    char buf6[9];
    uint_to_hex_str(0x1A3F, buf6, sizeof(buf6));
    ASSERT_STR_EQ(buf6, "1A3F");
TEST_CASE_END(test_uint_to_hex_str_case)

int main() {
    test_str_len_case();
    test_str_cpy_case();
    test_str_cmp_case();
    test_str_chr_case();
    test_str_str_case();
    test_str_tok_case();
    test_str_to_upper_case();
    test_str_to_lower_case();
    test_str_reverse_case();
    test_hex_str_to_uint_case();
    test_uint_to_hex_str_case();
    return 0;
}
