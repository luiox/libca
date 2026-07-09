# libca 功能索引

本文只做功能导航：遇到需求时先看这里判断该去哪个模块、哪个头文件。具体 API 参数、返回值、错误语义和示例以头文件 Doxygen 注释为准。

设计文档只说明模块思想、类型组织和重要取舍，不维护接口清单，也不做兼容承诺。

## core

基础设施模块，不依赖其他 libca 模块。

入口头文件：
- `<libca/core/datatype.hpp>`
- `<libca/core/result.hpp>`
- `<libca/core/bytes.hpp>`
- `<libca/core/cast.hpp>`
- `<libca/core/any.hpp>`
- `<libca/core/platform.hpp>`
- `<libca/core/stacktrace.hpp>`

功能：
- 定长类型与大小语义类型：`u8`、`i32`、`usize` 等。
- `Result<T, E>`：用返回值表达成功/失败，提供 `Ok`、`Err`、链式处理和错误传播辅助。
- `Bytes` / `BytesMut` / `ByteSlice`：字节缓冲、字节视图和协议解析辅助。
- 类型转换：精确动态类型匹配、类型判断和安全转换辅助。
- `Any`：轻量类型擦除，适合需要运行时保存少量异构值的边界。
- 平台检测、导出宏、栈追踪等基础工具。

设计文档：
- `libca/core/doc/result-spec.md`
- `libca/core/doc/bytes-spec.md`
- `libca/core/doc/cast_design.md`

## str

UTF-8 字符串与所有权模型模块。

入口头文件：
- `<libca/str/utf8_string.hpp>`
- `<libca/str/utf8_string_arena.hpp>`
- `<libca/str/utf8_string_pool.hpp>`
- `<libca/str/cstring.hpp>`
- `<libca/str/wstring.hpp>`
- `<libca/str/conversion.hpp>`
- `<libca/str/string_util.hpp>`
- `<libca/str/char_util.hpp>`

功能：
- `Utf8String`：拥有所有权的 UTF-8 字符串，移动语义，显式 `clone()`。
- `Utf8StringRef`：非拥有 UTF-8 字符串视图，用于参数、切片和临时引用。
- `Utf8Iterator`：按码点遍历 UTF-8 数据。
- `Utf8StringArena`：批量分配、整体释放的字符串 arena。
- `Utf8StringPool`：引用计数式字符串池。
- `Utf8StringBuilder`：可变构建器，用于多次追加后生成字符串。
- C 字符串、宽字符串、编码转换、字符分类和字符串工具函数。

设计文档：
- `libca/str/doc/str-spec.md`
- `libca/str/doc/utf8_string_design.md`

## fs

文件与路径工具模块，封装常见 `std::filesystem` 操作。

入口头文件：
- `<libca/fs/file_util.hpp>`
- `<libca/fs/path_util.hpp>`
- `<libca/fs/fs_error.hpp>`

功能：
- `PathUtil`：路径拼接、规范化、扩展名、文件名、父目录等纯字符串路径操作。
- `FileUtil`：读写文本、读写字节、文件/目录创建、删除、复制、移动和查询。
- `FileMode`：写入模式控制。
- `FsError`：文件操作错误码与可读文本转换。

设计文档：
- `libca/fs/doc/fs设计文档.md`

## crypto

哈希、编码、校验和基础密码学工具。

入口头文件：
- `<libca/crypto/crypto.hpp>`
- `<libca/crypto/hash.hpp>`
- `<libca/crypto/sha256.hpp>`
- `<libca/crypto/sha1.hpp>`
- `<libca/crypto/md5.hpp>`
- `<libca/crypto/sha3.h>`
- `<libca/crypto/hmac.hpp>`
- `<libca/crypto/crc.hpp>`
- `<libca/crypto/base64.hpp>`
- `<libca/crypto/hex.hpp>`
- `<libca/crypto/random.hpp>`
- `<libca/crypto/chacha20.hpp>`
- `<libca/crypto/rc4.hpp>`

功能：
- SHA-1、SHA-256、SHA-3、MD5 等 hash。
- HMAC。
- CRC、Base64、Hex 编解码。
- 随机数辅助。
- ChaCha20、RC4 等流式算法。

## time

日期时间工具模块。

入口头文件：
- `<libca/time/datetime.hpp>`

功能：
- `DateTime`：日期时间表示、解析、格式化和基础计算。

## collection

集合与函数式处理雏形。

入口头文件：
- `<libca/collection/immutable_list.hpp>`
- `<libca/collection/stream.hpp>`
- `<libca/collection/collection.hpp>`

功能：
- `ImmutableList`：不可变列表。
- `Stream`：链式数据处理接口。

## 暂未作为主线使用的代码

- `libca.core/`：旧 C++ 桌面代码，作为 legacy 参考，新增 C++ 工作优先放在 `libca/`。
- `libca/log/`、`libca/utility/`：有代码但未接入 `libca/xmake.lua`，使用前先确认是否要纳入主线。
- `libca/opt/`、`libca/reflect/`、`zip` 相关内容：规划或实验性质，使用前先看当前代码状态。

## 变更策略

libca 不维护单独的接口冻结清单，也不承诺严格长期 API/ABI 兼容。项目会尽量减少无意义的破坏性改动；当接口设计、错误模型、所有权语义或模块边界需要调整时，可以进行不兼容变更，并应在 README、CHANGELOG 或相关模块文档中说明影响和迁移方式。
