---
version: 1.0
update:
2026-07-06 - 首版，补充 crypto 模块职责、算法分层与使用边界
---

# libca/crypto 设计文档

## 定位

`libca_crypto` 提供基础编码、摘要、校验、HMAC、随机数和轻量流加密算法。它面向基础库和
工具链场景，既覆盖常用安全摘要，也保留 RC4、ChaCha20 这类在混淆器或兼容场景中有用的
算法。

crypto 依赖 `libca_core`，主要复用 `ByteSlice`、`Bytes`、`Result` 和 `usize`。

## 模块结构

模块按能力拆分头文件：

- 编码：`base64.hpp`、`hex.hpp`。
- 校验：`crc.hpp`。
- 摘要：`md5.hpp`、`sha1.hpp`、`sha256.hpp`、`sha3.h`。
- 认证：`hmac.hpp`。
- 随机数：`random.hpp`。
- 流加密：`rc4.hpp`、`chacha20.hpp`。
- 聚合入口：`crypto.hpp`。

每个算法尽量保持小文件独立，避免把所有实现集中到一个巨型工具类中。

## 错误模型

格式类输入错误使用 `CryptoError`，并通过 `Result<T, CryptoError>` 返回。例如 Base64/Hex
严格解码会检查非法字符、padding 和尾部补零位。随机数等系统能力失败也返回模块错误码。

摘要和编码的纯计算函数在输入有效且不依赖系统资源时直接返回值。

## 字节接口

新接口优先使用 `ca::core::ByteSlice` 输入，返回 `Bytes` 或 `std::string`。这样调用方可以
从 `Bytes`、`BytesMut`、数组或字符串统一构造视图，而不需要在 crypto API 中重复裸指针重载。

## 安全边界

本模块不是完整安全协议库。它只提供基础原语：

- 不负责密钥派生、nonce 管理、认证加密组合或协议握手。
- RC4 保留给兼容和混淆用途，不建议用于安全通信。
- ChaCha20 当前是裸流加密能力，调用方必须保证 key/nonce/counter 使用策略正确。

## 新人阅读顺序

1. `crypto_error.hpp`：先看错误码边界。
2. `crypto_util.hpp`、`hex.hpp`、`base64.hpp`：理解字节/文本转换风格。
3. `sha256.hpp`、`hmac.hpp`：理解常用摘要和认证入口。
4. `rc4.hpp`、`chacha20.hpp`：理解流加密接口和测试向量。
