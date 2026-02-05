#include "crypto.h"
#include "../em_base/macro_util.h"
#include "../em_base/debug.h"
#include <string.h>

// ---- null crypto implementation (no dynamic allocation) ----
typedef struct crypto_null {
    crypto_t api;
    // no state
}crypto_null_t;

static void crypto_null_init(void* context)
{
    unused_param(context);
}

static i32 crypto_null_encrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    crypto_null_t* c = (crypto_null_t*)context;
    param_check(c != NULL);
    if (out_size < in_size) return -1;
    if (in_size == 0) return 0;
    memcpy(out, in, in_size);
    return (i32)in_size;
}

static i32 crypto_null_decrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    return crypto_null_encrypt(context, in, in_size, out, out_size);
}

static void crypto_null_destroy(void* context)
{
    unused_param(context);
}

/*
 * 使用静态单例代替动态分配：
 * - 不使用 malloc/free
 * - 返回的指针有效且可用，但为进程内唯一实例（可根据需要扩展）
 */
static crypto_null_t g_crypto_null = {
    .api = {
        .init = crypto_null_init,
        .encrypt = crypto_null_encrypt,
        .decrypt = crypto_null_decrypt,
        .destroy = crypto_null_destroy
    }
};

crypto_t* crypto_get_null(void)
{
    return &g_crypto_null.api;
}

// ---- xor crypto implementation (no dynamic allocation) ----
typedef struct crypto_xor {
    crypto_t api;
    u8 key[CA_CRYPTO_MAX_KEY_LEN];
    usize key_len;
}crypto_xor_t;

static void crypto_xor_init(void* context)
{
    unused_param(context);
}

static i32 crypto_xor_encrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    param_check(context != NULL);
    crypto_xor_t* c = (crypto_xor_t*)context;
    if (c->key_len == 0) return -1;
    if (out_size < in_size) return -1;
    for (usize i = 0; i < in_size; ++i) {
        out[i] = in[i] ^ c->key[i % c->key_len];
    }
    return (i32)in_size;
}

static i32 crypto_xor_decrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    // XOR is对称的
    return crypto_xor_encrypt(context, in, in_size, out, out_size);
}

static void crypto_xor_destroy(void* context)
{
    crypto_xor_t* c = (crypto_xor_t*)context;
    if (!c) return;
    // 清除密钥并复位长度；不释放内存（静态实例）
    memset(c->key, 0, sizeof(c->key));
    c->key_len = 0;
}

static crypto_xor_t g_crypto_xor = {
    .api = {
        .init = crypto_xor_init,
        .encrypt = crypto_xor_encrypt,
        .decrypt = crypto_xor_decrypt,
        .destroy = crypto_xor_destroy
    },
    .key = {0},
    .key_len = 0
};

crypto_t* crypto_get_xor(const u8* key, usize key_len)
{
    if (!key || key_len == 0) return NULL;
    if (key_len > CA_CRYPTO_MAX_KEY_LEN) return NULL;
    memcpy(g_crypto_xor.key, key, key_len);
    g_crypto_xor.key_len = key_len;
    return &g_crypto_xor.api;
}

// ---- unit tests ----
#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(crypto_null_basic) {
    crypto_t* c = crypto_get_null();
    TEST_ASSERT_NOT_NULL(c);
    c->init(c);

    u8 in[] = {1,2,3,4,5};
    u8 out[5] = {0};
    i32 ret = c->encrypt(c, in, sizeof(in), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(in), ret);
    TEST_ASSERT_EQUAL_MEMORY(in, out, sizeof(in));

    // decrypt should be same
    u8 out2[5] = {0};
    ret = c->decrypt(c, out, sizeof(out), out2, sizeof(out2));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(out), ret);
    TEST_ASSERT_EQUAL_MEMORY(in, out2, sizeof(in));

    // insufficient out buffer
    ret = c->encrypt(c, in, sizeof(in), out, 2);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    c->destroy(c);
}

TEST_CASE(crypto_xor_basic) {
    u8 key[] = {0xFF, 0x01};
    crypto_t* c = crypto_get_xor(key, sizeof(key));
    TEST_ASSERT_NOT_NULL(c);
    c->init(c);

    u8 plain[] = { 'h', 'e', 'l', 'l', 'o' };
    u8 expected[sizeof(plain)];
    for (usize i = 0; i < sizeof(plain); ++i) expected[i] = plain[i] ^ key[i % sizeof(key)];

    u8 out[sizeof(plain)];
    i32 ret = c->encrypt(c, plain, sizeof(plain), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(plain), ret);
    TEST_ASSERT_EQUAL_MEMORY(expected, out, sizeof(plain));

    // decrypt back
    u8 decrypted[sizeof(plain)];
    ret = c->decrypt(c, out, sizeof(out), decrypted, sizeof(decrypted));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(plain), ret);
    TEST_ASSERT_EQUAL_MEMORY(plain, decrypted, sizeof(plain));

    // insufficient out
    ret = c->encrypt(c, plain, sizeof(plain), out, 2);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    c->destroy(c);
}

TEST_CASE(crypto_xor_bad_key) {
    crypto_t* c = crypto_get_xor(NULL, 0);
    TEST_ASSERT_NULL(c);
}

#endif // TEST_ENABLE