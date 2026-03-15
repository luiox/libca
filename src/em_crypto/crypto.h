/**
 * @file crypto.h
 * @author canrad (1517807724@qq.com)
 * @brief 加密接口定义
 * @version 0.1
 * @date 2026-02-05
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_CRYPTO_CRYPTO_H
#define LIBCA_EM_CRYPTO_CRYPTO_H

#include "em_base/datatype.h"

typedef struct crypto_ops {
    void (*init)(void* context);
    i32 (*encrypt)(void* context, u8* in, usize in_size, u8* out, usize out_size);
    i32 (*decrypt)(void* context, u8* in, usize in_size, u8* out, usize out_size);
    void (*destroy)(void* context);
}crypto_ops_t; 

/**
 * @brief 空加密器上下文类型（无状态）
 *
 * 该上下文由使用者分配和持有；无需释放。
 */
typedef struct crypto_null_ctx {
    /* 无状态，占位以便未来扩展 */
    u8 _reserved;
}crypto_null_ctx_t;

/**
 * @brief XOR 加密器上下文类型
 *
 * 用户需要提供该上下文实例并通过 `crypto_xor_ctx_init` 初始化密钥后
 * 与 `crypto_ops_get_xor` 返回的操作表配合使用。
 */
typedef struct crypto_xor_ctx {
    /**
     * @brief 指向密钥数据的指针
     *
     * 该上下文不会复制密钥；调用者必须保证传入的密钥缓冲在该上下文
     * 使用期间保持有效且不可被修改（如果需要请传入只读或在外部保护）。
     */
    const u8* key;
    /** 密钥长度 */
    usize key_len;
}crypto_xor_ctx_t;

/**
 * @brief 获取空加密器操作表（单例）
 *
 * 返回的操作表为单例，使用者需提供上下文（`crypto_null_ctx_t`）用于调用函数。
 *
 * @return 指向 `crypto_ops_t` 单例
 */
crypto_ops_t* crypto_ops_get_null(void);

/**
 * @brief 初始化空加密器上下文
 *
 * @param ctx 要初始化的上下文指针，不能为空
 */
void crypto_null_ctx_init(crypto_null_ctx_t* ctx);

/**
 * @brief 获取 XOR 加密器操作表（单例）
 *
 * 返回的操作表为单例，使用者需提供通过 `crypto_xor_ctx_init` 初始化的
 * `crypto_xor_ctx_t` 上下文来调用加解密函数。
 *
 * @return 指向 `crypto_ops_t` 单例
 */
crypto_ops_t* crypto_ops_get_xor(void);

/**
 * @brief 初始化 XOR 加密器上下文并设置密钥（不复制）
 *
 * 注意：该函数**不**会复制密钥，而是仅保存密钥指针到上下文中，
 * 调用者必须保证传入的密钥缓冲在上下文使用期间保持有效。
 *
 * @param ctx 要初始化的上下文指针，不能为空
 * @param key 密钥指针，不能为空
 * @param key_len 密钥长度（>0 && <= CA_CRYPTO_MAX_KEY_LEN）
 * @return 0 成功；-1 参数不合法或密钥过长
 */
i32 crypto_xor_ctx_init(crypto_xor_ctx_t* ctx, const u8* key, usize key_len);

#endif // !LIBCA_EM_CRYPTO_CRYPTO_H
