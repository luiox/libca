/* Auto-migrated from src/em_crypto/crypto.c test blocks */
#include "crypto.h"
#include <em_base/debug.h>
#include <string.h>


#include <em_test/test.h>

TEST_CASE(crypto_null_basic)
{
    crypto_null_ctx_t ctx;
    crypto_null_ctx_init(&ctx);
    crypto_ops_t* ops = crypto_ops_get_null();
    TEST_ASSERT_NOT_NULL(ops);
    ops->init(&ctx);

    u8  in[]   = {1, 2, 3, 4, 5};
    u8  out[5] = {0};
    i32 ret    = ops->encrypt(&ctx, in, sizeof(in), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(in), ret);
    TEST_ASSERT_EQUAL_MEMORY(in, out, sizeof(in));

    // decrypt should be same
    u8 out2[5] = {0};
    ret        = ops->decrypt(&ctx, out, sizeof(out), out2, sizeof(out2));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(out), ret);
    TEST_ASSERT_EQUAL_MEMORY(in, out2, sizeof(in));

    // insufficient out buffer
    ret = ops->encrypt(&ctx, in, sizeof(in), out, 2);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    ops->destroy(&ctx);
}

TEST_CASE(crypto_xor_basic)
{
    u8               key[] = {0xFF, 0x01};
    crypto_xor_ctx_t ctx;
    i32              r = crypto_xor_ctx_init(&ctx, key, sizeof(key));
    TEST_ASSERT_EQUAL_INT(0, r);
    crypto_ops_t* ops = crypto_ops_get_xor();
    TEST_ASSERT_NOT_NULL(ops);
    ops->init(&ctx);

    u8 plain[] = {'h', 'e', 'l', 'l', 'o'};
    u8 expected[sizeof(plain)];
    for (usize i = 0; i < sizeof(plain); ++i)
        expected[i] = plain[i] ^ key[i % sizeof(key)];

    u8  out[sizeof(plain)];
    i32 ret = ops->encrypt(&ctx, plain, sizeof(plain), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(plain), ret);
    TEST_ASSERT_EQUAL_MEMORY(expected, out, sizeof(plain));

    // decrypt back
    u8 decrypted[sizeof(plain)];
    ret = ops->decrypt(&ctx, out, sizeof(out), decrypted, sizeof(decrypted));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(plain), ret);
    TEST_ASSERT_EQUAL_MEMORY(plain, decrypted, sizeof(plain));

    // insufficient out
    ret = ops->encrypt(&ctx, plain, sizeof(plain), out, 2);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    ops->destroy(&ctx);
}

TEST_CASE(crypto_xor_bad_key)
{
    crypto_xor_ctx_t ctx;
    i32              r = crypto_xor_ctx_init(&ctx, NULL, 0);
    TEST_ASSERT_NOT_EQUAL_INT(0, r);
}
