/* Auto-migrated from src/em_crypto/base64.c test blocks */
#include "base64.h"


#include <em_test/test.h>


/* ============================================================================
 * Base64 单元测试
 * ============================================================================ */

TEST_CASE(base64_encode_len_test)
{
    /* 每3字节编码为4字符 */
    TEST_ASSERT_EQUAL_UINT(0, base64_encode_len(0));  /* 0 -> 0 */
    TEST_ASSERT_EQUAL_UINT(4, base64_encode_len(1));  /* 1 -> 4 */
    TEST_ASSERT_EQUAL_UINT(4, base64_encode_len(2));  /* 2 -> 4 */
    TEST_ASSERT_EQUAL_UINT(4, base64_encode_len(3));  /* 3 -> 4 */
    TEST_ASSERT_EQUAL_UINT(8, base64_encode_len(4));  /* 4 -> 8 */
    TEST_ASSERT_EQUAL_UINT(8, base64_encode_len(5));  /* 5 -> 8 */
    TEST_ASSERT_EQUAL_UINT(8, base64_encode_len(6));  /* 6 -> 8 */
    TEST_ASSERT_EQUAL_UINT(12, base64_encode_len(9)); /* 9 -> 12 */
}

TEST_CASE(base64_encode_basic)
{
    /* 空输入 */
    char out[32];
    TEST_ASSERT_EQUAL_UINT(0, base64_encode((u8*)"", 0, out));
    TEST_ASSERT_EQUAL_STRING("", out);

    /* "f" -> "Zg==" */
    u8 in1[] = {'f'};
    TEST_ASSERT_EQUAL_UINT(4, base64_encode(in1, 1, out));
    TEST_ASSERT_EQUAL_STRING("Zg==", out);

    /* "fo" -> "Zm8=" */
    u8 in2[] = {'f', 'o'};
    TEST_ASSERT_EQUAL_UINT(4, base64_encode(in2, 2, out));
    TEST_ASSERT_EQUAL_STRING("Zm8=", out);

    /* "foo" -> "Zm9v" */
    u8 in3[] = {'f', 'o', 'o'};
    TEST_ASSERT_EQUAL_UINT(4, base64_encode(in3, 3, out));
    TEST_ASSERT_EQUAL_STRING("Zm9v", out);

    /* "foob" -> "Zm9vYg==" */
    u8 in4[] = {'f', 'o', 'o', 'b'};
    TEST_ASSERT_EQUAL_UINT(8, base64_encode(in4, 4, out));
    TEST_ASSERT_EQUAL_STRING("Zm9vYg==", out);

    /* "fooba" -> "Zm9vYmE=" */
    u8 in5[] = {'f', 'o', 'o', 'b', 'a'};
    TEST_ASSERT_EQUAL_UINT(8, base64_encode(in5, 5, out));
    TEST_ASSERT_EQUAL_STRING("Zm9vYmE=", out);

    /* "foobar" -> "Zm9vYmFy" */
    u8 in6[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    TEST_ASSERT_EQUAL_UINT(8, base64_encode(in6, 6, out));
    TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", out);
}

TEST_CASE(base64_encode_all_bytes)
{
    /* 测试所有字节值 0-255 */
    u8 input[256];
    for (usize i = 0; i < 256; i++) {
        input[i] = (u8)i;
    }

    usize enc_len = base64_encode_len(256);
    TEST_ASSERT_EQUAL_UINT(344, enc_len); /* (256+2)/3*4 = 344 */

    char  output[400];
    usize len = base64_encode(input, 256, output);
    TEST_ASSERT_EQUAL_UINT(enc_len, len);

    /* 验证编码后可以正确解码 */
    u8    decoded[256];
    usize dec_len = base64_decode(output, len, decoded);
    TEST_ASSERT_EQUAL_UINT(256, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(input, decoded, 256);
}

TEST_CASE(base64_decode_len_test)
{
    TEST_ASSERT_EQUAL_UINT(0, base64_decode_len("", 0));
    TEST_ASSERT_EQUAL_UINT(1, base64_decode_len("Zg==", 4)); /* 2个填充 */
    TEST_ASSERT_EQUAL_UINT(2, base64_decode_len("Zm8=", 4)); /* 1个填充 */
    TEST_ASSERT_EQUAL_UINT(3, base64_decode_len("Zm9v", 4)); /* 无填充 */
    TEST_ASSERT_EQUAL_UINT(6, base64_decode_len("Zm9vYmFy", 8));
}

TEST_CASE(base64_decode_basic)
{
    u8 out[32];

    /* "Zg==" -> "f" */
    TEST_ASSERT_EQUAL_UINT(1, base64_decode("Zg==", 4, out));
    TEST_ASSERT_EQUAL_UINT('f', out[0]);

    /* "Zm8=" -> "fo" */
    TEST_ASSERT_EQUAL_UINT(2, base64_decode("Zm8=", 4, out));
    TEST_ASSERT_EQUAL_UINT('f', out[0]);
    TEST_ASSERT_EQUAL_UINT('o', out[1]);

    /* "Zm9v" -> "foo" */
    TEST_ASSERT_EQUAL_UINT(3, base64_decode("Zm9v", 4, out));
    TEST_ASSERT_EQUAL_MEMORY("foo", out, 3);

    /* "Zm9vYmFy" -> "foobar" */
    TEST_ASSERT_EQUAL_UINT(6, base64_decode("Zm9vYmFy", 8, out));
    TEST_ASSERT_EQUAL_MEMORY("foobar", out, 6);
}

TEST_CASE(base64_decode_invalid)
{
    u8 out[32];

    /* 空指针 */
    TEST_ASSERT_EQUAL_UINT(0, base64_decode(NULL, 4, out));
    TEST_ASSERT_EQUAL_UINT(0, base64_decode("Zm9v", 4, NULL));

    /* 长度不是4的倍数 */
    TEST_ASSERT_EQUAL_UINT(0, base64_decode("Zm9v", 3, out));
    TEST_ASSERT_EQUAL_UINT(0, base64_decode("Zm9vYmF", 7, out));

    /* 非法字符 */
    TEST_ASSERT_EQUAL_UINT(0, base64_decode("Zm*v", 4, out)); /* '*' 不是有效字符 */

    /* 验证正确的编码格式 */
    TEST_ASSERT_EQUAL_UINT(1, base64_decode("Zg==", 4, out)); /* 解码为 'f' */
    TEST_ASSERT_EQUAL_UINT('f', out[0]);
}

TEST_CASE(base64_roundtrip)
{
    /* 编码后解码应得到原始数据 */
    u8   original[] = {0x00, 0x01, 0x02, 0x7F, 0x80, 0xFE, 0xFF, 0xAB, 0xCD, 0xEF};
    char encoded[32];
    u8   decoded[32];

    usize enc_len = base64_encode(original, sizeof(original), encoded);
    TEST_ASSERT(enc_len > 0);

    usize dec_len = base64_decode(encoded, enc_len, decoded);
    TEST_ASSERT_EQUAL_UINT(sizeof(original), dec_len);
    TEST_ASSERT_EQUAL_MEMORY(original, decoded, sizeof(original));
}

TEST_CASE(base64_encode_null_check)
{
    char out[32];
    u8   in[] = {'t', 'e', 's', 't'};

    TEST_ASSERT_EQUAL_UINT(0, base64_encode(NULL, 4, out));
    TEST_ASSERT_EQUAL_UINT(0, base64_encode(in, 4, NULL));
}


