#include "crypto.h"
#include <em_base/debug.h>
#include <string.h>

// ---- null crypto implementation ----
// 操作表为单例，具体上下文由用户提供

/* forward declarations */
static void crypto_null_init(void* context);
static i32 crypto_null_encrypt(void* context, u8* in, usize in_size, u8* out, usize out_size);
static i32 crypto_null_decrypt(void* context, u8* in, usize in_size, u8* out, usize out_size);
static void crypto_null_destroy(void* context);

static const crypto_ops_t g_crypto_null_ops = {
    .init = crypto_null_init,
    .encrypt = crypto_null_encrypt,
    .decrypt = crypto_null_decrypt,
    .destroy = crypto_null_destroy
};

static void crypto_null_init(void* context)
{
    crypto_null_ctx_t* ctx = (crypto_null_ctx_t*)context;
    param_check(ctx != NULL);
    // 无状态， nothing to do
    unused_param(ctx);
}

static i32 crypto_null_encrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    param_check(context != NULL);
    param_check(in != NULL);
    param_check(out != NULL);
    
    // 边界检查：输出缓冲区大小不足
    if (out_size < in_size) return -1;
    
    // 空数据：合法情况，直接返回成功
    if (in_size == 0) return 0;
    
    // 执行拷贝（null加密即原样拷贝）
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

crypto_ops_t* crypto_ops_get_null(void)
{
    return (crypto_ops_t*)&g_crypto_null_ops;
}

void crypto_null_ctx_init(crypto_null_ctx_t* ctx)
{
    param_check(ctx != NULL);
    ctx->_reserved = 0;
}

// ---- xor crypto implementation ----
// 操作表为单例，具体上下文由用户提供和初始化

/* forward declarations */
static void crypto_xor_init(void* context);
static i32 crypto_xor_encrypt(void* context, u8* in, usize in_size, u8* out, usize out_size);
static i32 crypto_xor_decrypt(void* context, u8* in, usize in_size, u8* out, usize out_size);
static void crypto_xor_destroy(void* context);

static const crypto_ops_t g_crypto_xor_ops = {
    .init = crypto_xor_init,
    .encrypt = crypto_xor_encrypt,
    .decrypt = crypto_xor_decrypt,
    .destroy = crypto_xor_destroy
};

static void crypto_xor_init(void* context)
{
    param_check(context != NULL);
    // context 已由用户通过 crypto_xor_ctx_init 初始化
}

static i32 crypto_xor_encrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    crypto_xor_ctx_t* c = (crypto_xor_ctx_t*)context;
    param_check(c != NULL);
    if (c->key == NULL || c->key_len == 0) return -1;
    if (out_size < in_size) return -1;
    for (usize i = 0; i < in_size; ++i) {
        out[i] = in[i] ^ c->key[i % c->key_len];
    }
    return (i32)in_size;
}

static i32 crypto_xor_decrypt(void* context, u8* in, usize in_size, u8* out, usize out_size)
{
    // XOR 是对称的
    return crypto_xor_encrypt(context, in, in_size, out, out_size);
}

static void crypto_xor_destroy(void* context)
{
    crypto_xor_ctx_t* c = (crypto_xor_ctx_t*)context;
    if (!c) return;
    /* 不拥有密钥内存，只清除引用信息 */
    c->key = NULL;
    c->key_len = 0;
}

crypto_ops_t* crypto_ops_get_xor(void)
{
    return (crypto_ops_t*)&g_crypto_xor_ops;
}

i32 crypto_xor_ctx_init(crypto_xor_ctx_t* ctx, const u8* key, usize key_len)
{
    param_check(ctx != NULL);
    if (!key || key_len == 0) return -1;
    /* 不复制密钥，仅保存指针（调用者需保证 key 在使用期间有效） */
    ctx->key = key;
    ctx->key_len = key_len;
    return 0;
}

// ---- unit tests ----
#if TEST_ENABLE

#include <em_test/test.h>

TEST_CASE(crypto_null_basic) {
    crypto_null_ctx_t ctx;
    crypto_null_ctx_init(&ctx);
    crypto_ops_t* ops = crypto_ops_get_null();
    TEST_ASSERT_NOT_NULL(ops);
    ops->init(&ctx);

    u8 in[] = {1,2,3,4,5};
    u8 out[5] = {0};
    i32 ret = ops->encrypt(&ctx, in, sizeof(in), out, sizeof(out));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(in), ret);
    TEST_ASSERT_EQUAL_MEMORY(in, out, sizeof(in));

    // decrypt should be same
    u8 out2[5] = {0};
    ret = ops->decrypt(&ctx, out, sizeof(out), out2, sizeof(out2));
    TEST_ASSERT_EQUAL_INT((i32)sizeof(out), ret);
    TEST_ASSERT_EQUAL_MEMORY(in, out2, sizeof(in));

    // insufficient out buffer
    ret = ops->encrypt(&ctx, in, sizeof(in), out, 2);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    ops->destroy(&ctx);
}

TEST_CASE(crypto_xor_basic) {
    u8 key[] = {0xFF, 0x01};
    crypto_xor_ctx_t ctx;
    i32 r = crypto_xor_ctx_init(&ctx, key, sizeof(key));
    TEST_ASSERT_EQUAL_INT(0, r);
    crypto_ops_t* ops = crypto_ops_get_xor();
    TEST_ASSERT_NOT_NULL(ops);
    ops->init(&ctx);

    u8 plain[] = { 'h', 'e', 'l', 'l', 'o' };
    u8 expected[sizeof(plain)];
    for (usize i = 0; i < sizeof(plain); ++i) expected[i] = plain[i] ^ key[i % sizeof(key)];

    u8 out[sizeof(plain)];
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

TEST_CASE(crypto_xor_bad_key) {
    crypto_xor_ctx_t ctx;
    i32 r = crypto_xor_ctx_init(&ctx, NULL, 0);
    TEST_ASSERT_NOT_EQUAL_INT(0, r);
}

#endif // TEST_ENABLE