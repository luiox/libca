---
version: 1.1
update:
2026-09-15 - 新增 AES 章节：三后端结构、Auto 顺序、GCM 取舍与性能数据
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
- 分组加密：`aes.hpp`（内置参考实现 + 多后端分发）。
- 聚合入口：`crypto.hpp`。

每个算法尽量保持小文件独立，避免把所有实现集中到一个巨型工具类中。

## 错误模型

格式类输入错误使用 `CryptoError`，并通过 `Result<T, CryptoError>` 返回。例如 Base64/Hex
严格解码会检查非法字符、padding 和尾部补零位。随机数等系统能力失败也返回模块错误码。

摘要和编码的纯计算函数在输入有效且不依赖系统资源时直接返回值。

AES 部分扩展了两个错误语义：

- `UNSUPPORTED_ALGORITHM`：显式指定的后端在当前编译配置下不可用（如 `with_openssl=n`
  时选 `OpenSsl`），或所选能力不提供该后端路径（如 GCM 走 `Builtin`）。
- `AUTHENTICATION_FAILED`：AEAD（AES-GCM）解密时认证标签校验失败。
- `BACKEND_FAILED`：参数合法的前提下，OpenSSL/CNG 系统调用失败。

## 字节接口

新接口优先使用 `ca::core::ByteSlice` 输入，返回 `Bytes` 或 `std::string`。这样调用方可以
从 `Bytes`、`BytesMut`、数组或字符串统一构造视图，而不需要在 crypto API 中重复裸指针重载。

## AES：内置参考实现与后端选择

### 三后端结构

`aes.hpp` 以同一组函数签名（`aes_ecb_*` / `aes_cbc_*` / `aes_ctr_crypt` / `aes_gcm_*`）提供
三种实现路径，由 `AesBackend` 枚举逐函数选择：

- `Builtin`：`aes.cpp` 内的纯软件参考实现，按 FIPS-197 朴素实现（S-box 查表 + 逐列
  MixColumns）。任何构建配置都可用，作为正确性基准与无外部依赖的兜底。
- `OpenSsl`：EVP 接口（`with_openssl=y` 时编译，define `LIBCA_CRYPTO_HAS_OPENSSL`，
  命名对齐 http 模块的 `LIBCA_HTTP_HAS_OPENSSL`）。条件编译隔离，未启用时零影响。
- `Cng`：Windows CNG（bcrypt.dll，ECB/CBC 走原生 chaining mode；crypto 库在 Windows
  下已链 bcrypt）。仅 `_WIN32` 编译，非 Windows 平台零影响。
- `Auto`：编译期解析顺序 **OpenSSL（若可用）> CNG（若 Windows）> Builtin**。

参数校验（密钥 16/24/32 字节、ECB/CBC 整块、IV/计数块 16 字节）在分发前统一完成，
各后端只负责计算与系统调用错误映射。

### 常量时间口径

内置参考实现为**正确性与无外部依赖优先**，未做常量时间加固（查表法存在缓存时序
侧信道）。防御时序攻击的场景应显式选择 `OpenSsl` / `Cng` 后端；头文件 Doxygen 对每个
入口都明示了这一边界。

### GCM 只走外部后端的取舍

AES-GCM 本模块只提供 `OpenSsl` / `Cng` 路径：AEAD 的安全性依赖实现侧的常量时间
GHASH 与密钥调度，内置朴素实现做 GCM 会放大时序风险且收益有限。因此
`aes_gcm_*` 走 `Builtin` 时直接返回 `UNSUPPORTED_ALGORITHM`（Auto 会解析到可用的外部
后端，通常不受影响）。接口口径：nonce 固定 12 字节、tag 固定 16 字节、密文与 tag
分离返回（`AesGcmResult`），认证失败返回 `AUTHENTICATION_FAILED`。

### CNG 的 CTR 限制

实测部分 Windows 的 AES primitive provider 会拒绝 `ChainingModeCTR` 属性
（`BCryptSetProperty` 返回 `STATUS_INVALID_PARAMETER`），且原生 CTR 只接受整块输入。
因此 CNG 后端的 CTR 不依赖原生 chaining mode：将连续计数块（128 位大端递增）组成
缓冲区后用 ECB 模式批量加密生成 keystream（每次最多 64KB），再与数据 XOR。语义与
SP800-38A 及其他后端严格一致，跨后端对拍逐字节相同。

### 向量与对拍测试

- FIPS-197 附录 C（单块 ECB，三种密钥长度）与 NIST SP800-38A 附录 F（ECB/CBC/CTR，
  各三种密钥长度）向量经 python cryptography（OpenSSL EVP）与 openssl 命令行双工具
  交叉验证后硬编码，出处写在用例注释。
- GCM 向量取 McGrew-Viega GCM spec 附录 B Test Case 3/4，与 nettle 测试套件同源比对
  （注意 TC3 明文尾部为 `1aafd255`，与 SP800-38A CTR 向量的 `1bafd9d7` 不同，易混淆）。
- 双实现对拍：固定种子确定性伪随机输入，全部可用后端（本仓库 Windows + openssl=y
  配置下为 Builtin/OpenSsl/Cng 三方）对同一输入逐字节比对，并各自解密回环；外部后端
  不可用的构建中对拍用例 `GTEST_SKIP`。
- 错误路径：非法密钥长度、非整块 ECB/CBC、错误 IV、GCM 经内置路径、显式指定不可用
  后端、GCM 篡改 tag/AAD/密文。

### 性能数据

`libca/crypto/perf/crypto_perf.cpp`（target `libca_crypto_perf`，`libs/perf` 组，默认不
构建）输出 AES-256-CTR / CBC 加密吞吐。方法：4 MiB 数据，热身 2 轮后测 5 轮取
中位数，FNV-1a checksum 防死代码消除。

本机参考值（Windows 11 x64 / MSVC release / openssl 3.x，多次运行有波动）：

| backend  | CTR MB/s   | CBC MB/s   |
|----------|------------|------------|
| Builtin  | 6 ~ 15     | 6 ~ 16     |
| OpenSsl  | 513 ~ 628  | 330 ~ 455  |
| Cng      | 271 ~ 374  | 339 ~ 486  |

内置实现比外部后端慢约 1~2 个数量级，符合"正确性优先、不做平台优化"的定位；对吞吐
有要求的调用方应选择外部后端。

## 安全边界

本模块不是完整安全协议库。它只提供基础原语：

- 不负责密钥派生、nonce 管理、认证加密组合或协议握手。
- RC4 保留给兼容和混淆用途，不建议用于安全通信。
- ChaCha20 当前是裸流加密能力，调用方必须保证 key/nonce/counter 使用策略正确。
- AES 的 ECB/CBC/CTR 均为原始模式，无填充、无认证；需要认证加密用 AES-GCM（外部
  后端）或自行组合 MAC。

## 新人阅读顺序

1. `crypto_error.hpp`：先看错误码边界。
2. `crypto_util.hpp`、`hex.hpp`、`base64.hpp`：理解字节/文本转换风格。
3. `sha256.hpp`、`hmac.hpp`：理解常用摘要和认证入口。
4. `rc4.hpp`、`chacha20.hpp`：理解流加密接口和测试向量。
5. `aes.hpp`：理解"同一签名、多后端分发"的组织方式与后端可用性边界。
