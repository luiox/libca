---
version: 1.0
update: 
2026-02-28 - 完成第一版的使用文档编写
---

## em_crypto 使用文档

em_crypto 模块提供加密编码相关的工具函数，包括加密器抽象和 Base64 编解码。

---

## 模块组成

| 组件 | 文件 | 说明 |
|------|------|------|
| 加密器抽象 | `crypto.h/c` | 统一的加密器接口（null、xor） |
| Base64 编解码 | `base64.h/c` | 标准 Base64 编码/解码 |

---

## Base64 编解码

### 功能说明

Base64 是一种将二进制数据编码为可打印 ASCII 字符的编码方式，常用于：
- 邮件传输
- URL 安全传输
- 配置文件存储
- 数据序列化

**编码规则**：
- 每 3 字节编码为 4 个 Base64 字符
- 不足 3 字节时用 `=` 填充
- 字符集：`A-Z`、`a-z`、`0-9`、`+`、`/`

### API 概览

```c
usize base64_encode_len(usize input_len);                     // 计算编码后长度
usize base64_encode(const u8 *input, usize input_len, char *output);  // 编码
usize base64_decode_len(const char *input, usize input_len);  // 计算解码后长度
usize base64_decode(const char *input, usize input_len, u8 *output);   // 解码
```

### 使用示例

#### 1. 基本编码

```c
#include "base64.h"

void encode_example(void) {
    u8 data[] = "Hello, World!";
    char output[64];
    
    usize len = base64_encode(data, sizeof(data) - 1, output);
    // output = "SGVsbG8sIFdvcmxkIQ=="
    // len = 20
}
```

#### 2. 基本解码

```c
#include "base64.h"

void decode_example(void) {
    const char *encoded = "SGVsbG8sIFdvcmxkIQ==";
    u8 output[32];
    
    usize len = base64_decode(encoded, 20, output);
    // output = "Hello, World!"
    // len = 13
}
```

#### 3. 缓冲区大小计算

```c
#include "base64.h"

void buffer_size_example(void) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    // 编码：需要 base64_encode_len(input_len) + 1 字节（含 null 终止符）
    usize enc_buf_size = base64_encode_len(sizeof(data)) + 1;
    char encoded[enc_buf_size];
    base64_encode(data, sizeof(data), encoded);
    
    // 解码：需要 base64_decode_len(input, input_len) 字节
    usize dec_buf_size = base64_decode_len(encoded, strlen(encoded));
    u8 decoded[dec_buf_size];
    usize dec_len = base64_decode(encoded, strlen(encoded), decoded);
}
```

#### 4. 二进制数据编解码

```c
#include "base64.h"

void binary_example(void) {
    // 任意二进制数据
    u8 binary[] = {0x00, 0xFF, 0xAB, 0xCD, 0x12, 0x34};
    
    char encoded[32];
    usize enc_len = base64_encode(binary, sizeof(binary), encoded);
    
    u8 decoded[32];
    usize dec_len = base64_decode(encoded, enc_len, decoded);
    
    // 验证编解码一致性
    if (dec_len == sizeof(binary) && memcmp(binary, decoded, dec_len) == 0) {
        // 编解码成功
    }
}
```

### 错误处理

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `base64_encode` | `0` | 输入指针为 NULL |
| `base64_encode` | `>0` | 成功，返回编码长度 |
| `base64_decode` | `0` | 输入指针为 NULL、长度非法或包含非法字符 |
| `base64_decode` | `>0` | 成功，返回解码长度 |

### 常见错误

1. **输出缓冲区不足**

```c
// 错误：缓冲区太小
char output[4];
base64_encode(data, 10, output);  // 需要 16 字节

// 正确
usize needed = base64_encode_len(10) + 1;  // 16 + 1
char output[needed];
```

2. **输入长度非法**

```c
// 错误：Base64 编码长度必须是 4 的倍数
base64_decode("Zm9vYmF", 7, output);  // 返回 0

// 正确
base64_decode("Zm9vYmFy", 8, output);  // 返回 6
```

3. **包含非法字符**

```c
// 错误：包含非法字符
base64_decode("Zm*v", 4, output);  // 返回 0，'*' 不是有效字符
```

---

## 加密器抽象

### 设计理念

加密器采用 **操作表 + 上下文** 的设计模式：
- `crypto_ops_t`：操作函数表（单例），定义 `init/encrypt/decrypt/destroy`
- 上下文：由用户分配和持有，存储加密器状态

这种设计允许：
- 静态分配所有资源（无动态内存）
- 多实例并发使用
- 统一的加密器接口

### 可用加密器

| 加密器 | 操作表获取 | 上下文类型 | 说明 |
|--------|-----------|-----------|------|
| Null | `crypto_ops_get_null()` | `crypto_null_ctx_t` | 空加密，原样拷贝 |
| XOR | `crypto_ops_get_xor()` | `crypto_xor_ctx_t` | 简单异或加密 |

### 使用示例

#### 1. Null 加密器

```c
#include "crypto.h"

void null_crypto_example(void) {
    crypto_null_ctx_t ctx;
    crypto_null_ctx_init(&ctx);
    
    crypto_ops_t* ops = crypto_ops_get_null();
    ops->init(&ctx);
    
    u8 plaintext[] = "Hello";
    u8 ciphertext[16];
    u8 decrypted[16];
    
    // 加密（实际上是拷贝）
    i32 enc_len = ops->encrypt(&ctx, plaintext, 5, ciphertext, sizeof(ciphertext));
    // enc_len = 5, ciphertext = "Hello"
    
    // 解密
    i32 dec_len = ops->decrypt(&ctx, ciphertext, enc_len, decrypted, sizeof(decrypted));
    // dec_len = 5, decrypted = "Hello"
    
    ops->destroy(&ctx);
}
```

#### 2. XOR 加密器

```c
#include "crypto.h"

void xor_crypto_example(void) {
    u8 key[] = {0xAA, 0xBB, 0xCC};
    
    crypto_xor_ctx_t ctx;
    crypto_xor_ctx_init(&ctx, key, sizeof(key));
    
    crypto_ops_t* ops = crypto_ops_get_xor();
    ops->init(&ctx);
    
    u8 plaintext[] = "Hello World!";
    u8 ciphertext[16];
    u8 decrypted[16];
    
    // 加密
    i32 enc_len = ops->encrypt(&ctx, plaintext, 12, ciphertext, sizeof(ciphertext));
    
    // 解密
    i32 dec_len = ops->decrypt(&ctx, ciphertext, enc_len, decrypted, sizeof(decrypted));
    
    // decrypted 应该等于 plaintext
    
    ops->destroy(&ctx);
}
```

---

## 注意事项

1. **密钥生命周期**：XOR 加密器不复制密钥，用户必须保证密钥缓冲区在使用期间有效
2. **缓冲区大小**：编解码前务必计算所需缓冲区大小，避免溢出
3. **输出缓冲区**：`base64_encode` 会在输出末尾添加 null 终止符

---

## 依赖

- `em_base`（datatype.h, debug.h）
