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

#include "../em_base/datatype.h"

/*
 * 最大允许的 XOR 密钥长度（固定缓冲区），实现中不使用动态内存分配
 */
#define CA_CRYPTO_MAX_KEY_LEN 256

typedef struct crypto {
    void (*init)(void* context);
    i32 (*encrypt)(void* context, u8* in, usize in_size, u8* out, usize out_size);
    i32 (*decrypt)(void* context, u8* in, usize in_size, u8* out, usize out_size);
    void (*destroy)(void* context);
}crypto_t;

/**
 * @brief 获取一个空加密器（no-op）单例
 *
 * 该实例无需释放（实现为静态单例），调用者可直接使用返回的 `crypto_t` 接口。
 *
 * @return 指向可用的 `crypto_t`，失败时返回 NULL（理论上不会失败）
 */
crypto_t* crypto_get_null(void);

/**
 * @brief 获取一个 XOR 加密器单例，密钥被复制到内部固定缓冲区
 *
 * 该实现不使用动态内存分配；密钥长度必须大于 0 且小于等于 `CA_CRYPTO_MAX_KEY_LEN`。
 * 返回的实例为静态单例（同一进程仅有一个有效实例），因此**非线程安全**，
 * 后续调用可能覆盖先前的密钥。
 *
 * @param key 指向密钥数据的指针（不可为 NULL）
 * @param key_len 密钥长度（>0 && <= CA_CRYPTO_MAX_KEY_LEN）
 * @return 指向可用的 `crypto_t`，参数不合法或超长时返回 NULL
 */
crypto_t* crypto_get_xor(const u8* key, usize key_len);

#endif // !LIBCA_EM_CRYPTO_CRYPTO_H
