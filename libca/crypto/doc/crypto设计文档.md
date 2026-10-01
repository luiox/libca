---
version: 1.2
update:
2026-10-01 - 新增 DES 章节：单实现取舍、弱密钥不校验语义与 JCE 黄金组锚点
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
- 分组加密：`aes.hpp`（内置参考实现 + 多后端分发）、`des.hpp`（纯内置单实现）。
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

## DES：标准块密码原语与 JCE 对齐

### 单实现、无后端分发

`des.hpp` 提供纯内置的 FIPS 46-3 实现（IP/FP + 16 轮 Feistel、E 扩展、8 个 S-box、
P 置换、PC-1/PC-2 密钥调度 + 循环左移表），与 AES 的多后端结构刻意不同：**不设
后端枚举、不接 OpenSSL/CNG**。原因：DES 已被现代提供方弃用（OpenSSL 3.x 移入
legacy provider、CNG 逐步收紧），为它维护多条后端路径只有成本没有收益；内置查表
实现即为唯一事实来源。

API 分三层，全部 snake_case 自由函数（不引入 `Des` 类型，与模块既有风格一致）：

- 单块：`des_encrypt_block` / `des_decrypt_block`（8 字节进、8 字节出）；
- 模式：`des_ecb_encrypt/decrypt`、`des_cbc_encrypt/decrypt`（NoPadding，IV 显式
  传入，零 IV 也由调用方显式给；块对齐由调用方保证，非 8 倍数返回
  `INVALID_ARGUMENT`）；
- 填充：`pkcs5_pad` / `pkcs5_unpad`（固定按 DES 块大小 8；unpad 对 pad 值越界
  （0 或 > 8）或不一致返回 `INVALID_ARGUMENT`，对齐 JCE BadPaddingException 口径）。

错误通道沿用 `Result<Bytes, CryptoError>`，只用到 `INVALID_ARGUMENT`，不新增错误码。

### 弱密钥不校验（与 JCE 对齐的硬要求）

JCE 的 `DESKeySpec` 只读取 8 字节原始密钥，**不调整奇偶位、不拒绝弱密钥/半弱密钥**；
奇偶位经 PC-1 置换自然丢弃。本实现同样不做任何密钥校验：任意 8 字节都是合法密钥。
这不只是宽容——morpher 侧既有数据（混淆产物中的密钥常带非法奇偶位）依赖该语义，
逐位对齐是硬要求。推论：全零密钥 `0000000000000000` 与弱密钥 `0101010101010101` 经
PC-1 后完全同键，两者 `E(0) = 8CA64DE9C1B123A7`（单测钉死）。

### 测试向量来源

- FIPS 46-3 教科书向量（key `133457799BBCDFF1`：
  `E(0123456789ABCDEF)=85E813540F0AB405`、`D(0123456789ABCDEF)=EE0F7C12E0B09338`）
  与附录 B、经典例题（"Now is t"）。
- **JCE 黄金组**移植自 luiox/morpher `mjt-deobf/test/des_eval_test.cpp`（SunJCE
  JDK17 实测产出）：单块解密 `D(133457799BBCDFF1, FEDCBA9876543210)=7D4D8B4E525E14ED`、
  `D(133457799BBCDFF1, FFFFFFFFFFFFFFFF)=D85B9AE1CCD81834`；零 IV 三块 CBC 链、
  非零 IV CBC 单块、7 组 CBC-PKCS5 明文长（0/1/7/8/9/16/23）与 4 组非法填充拒绝。
  morpher 的 DES 双实现下沉本模块后，这批数字是双仓共用的逐位兼容锚点。
- 回环与链接性：确定性 LCG 伪随机输入双向回环；IV 变化改变输出、错 IV 解密仅首块
  不同、逐块核对 `C_i = E(P_i xor C_{i-1})`；弱/半弱密钥的 FIPS 46-3 附录 A 对合性质
  （弱密钥 `E(E(P))=P`、半弱对 `E_K2(E_K1(P))=P`）兼作"未拒绝弱密钥"的行为证明。

### 安全口径

DES 56 位有效密钥强度早已不足，本原语仅供 legacy 协议与混淆场景兼容，不用于新安全
设计；PKCS#5 填充校验只保证协议正确性，不具备认证能力（安全场景用 AES-GCM）。

## 安全边界

本模块不是完整安全协议库。它只提供基础原语：

- 不负责密钥派生、nonce 管理、认证加密组合或协议握手。
- RC4 保留给兼容和混淆用途，不建议用于安全通信。
- ChaCha20 当前是裸流加密能力，调用方必须保证 key/nonce/counter 使用策略正确。
- AES 的 ECB/CBC/CTR 均为原始模式，无填充、无认证；需要认证加密用 AES-GCM（外部
  后端）或自行组合 MAC。
- DES 为 legacy 兼容原语（56 位有效密钥，强度不足），仅用于旧协议与混淆场景；
  按设计不校验弱密钥（对齐 JCE `DESKeySpec` 语义）。

## 新人阅读顺序

1. `crypto_error.hpp`：先看错误码边界。
2. `crypto_util.hpp`、`hex.hpp`、`base64.hpp`：理解字节/文本转换风格。
3. `sha256.hpp`、`hmac.hpp`：理解常用摘要和认证入口。
4. `rc4.hpp`、`chacha20.hpp`：理解流加密接口和测试向量。
5. `aes.hpp`：理解"同一签名、多后端分发"的组织方式与后端可用性边界。
6. `des.hpp`：理解"单实现 + JCE 逐位对齐"的兼容原语取舍。
