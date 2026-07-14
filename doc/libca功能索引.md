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
- `libca/core/doc/core设计文档.md`

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
- `libca/str/doc/str设计文档.md`

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

## io

同步字节流协议、缓冲读写和原生 handle/fd RAII 模块。

入口头文件：
- `<libca/io/io.hpp>`
- `<libca/io/error.hpp>`
- `<libca/io/reader.hpp>`
- `<libca/io/writer.hpp>`
- `<libca/io/seek.hpp>`
- `<libca/io/buffered.hpp>`
- `<libca/io/native_handle.hpp>`
- `<libca/io/native_stream.hpp>`

功能：
- `IoError` / `IoResult`：保留稳定错误类别、原生错误码和操作上下文。
- `Reader`：统一 EOF、短读、精确读取和限长读取到末尾。
- `Writer`：统一短写、完整写入和 flush 语义。
- `Seek` / `SeekFrom`：从起点、当前位置或末尾定位字节流。
- `BufReader` / `BufWriter`：拥有底层流的固定容量缓冲适配器。
- `OwnedHandle`：Windows HANDLE 或 POSIX fd 的 move-only RAII 所有者。
- `NativeStream`：基于原生 handle/fd 的 Reader、Writer 和 Seek 实现。

设计与使用文档：
- `libca/io/doc/design.md`
- `libca/io/README.md`（快速示例）

## net

建立在 io 上的同步 TCP、UDP、DNS 与跨平台 socket RAII 模块。

入口头文件：
- `<libca/net/net.hpp>`
- `<libca/net/address.hpp>`
- `<libca/net/socket.hpp>`
- `<libca/net/dns.hpp>`
- `<libca/net/tcp.hpp>`
- `<libca/net/udp.hpp>`

功能：
- `IpAddress` / `SocketAddress`：IPv4、IPv6、端口、flow info 和 scope id 值类型。
- `OwnedSocket`：Windows SOCKET 或 POSIX socket fd 的 move-only RAII 所有者。
- `DnsResolver`：基于 getaddrinfo 的同步主机名解析和地址族筛选。
- `TcpStream`：实现 Reader / Writer 的 TCP 字节流，支持连接超时、读写超时和非阻塞。
- `TcpListener`：TCP bind、accept、临时端口、非阻塞和 clone。
- `UdpSocket`：保留数据报边界的 send/receive API、超时、非阻塞和 broadcast。

TLS 不属于基础 socket API，后续应作为包装 TcpStream 的独立扩展。

设计与使用文档：
- `libca/net/doc/design.md`
- `libca/net/README.md`（快速示例）

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
- `<libca/time/duration.hpp>`
- `<libca/time/timestamp.hpp>`
- `<libca/time/datetime.hpp>`
- `<libca/time/time_util.hpp>`

功能：
- `Duration`：纳秒精度时间间隔，纯值类型，支持 constexpr 算术与 chrono 互转。
- `Timestamp`：Unix epoch 纳秒时间戳，与 `Duration` 做加减、与 `system_clock` 互转。
- `Date` / `Time` / `DateTime`：面向日历展示与简单解析的轻量类型。
- `TimeUtil`：时钟工具，对齐 Java `currentTimeMillis` / `nanoTime` 语义。

设计文档：
- `libca/time/doc/time设计文档.md`

## collection

集合与函数式处理容器，以 Rust-like API 包装 STL 存储。

入口头文件：
- `<libca/collection/array_list.hpp>`
- `<libca/collection/hash_map.hpp>`
- `<libca/collection/hash_set.hpp>`
- `<libca/collection/immutable_list.hpp>`
- `<libca/collection/stream.hpp>`
- `<libca/collection/collection.hpp>`

功能：
- `ArrayList<T>`：基于 `std::vector` 的拥有型可变顺序容器。
- `HashMap<K, V>` / `HashSet<T>`：基于 `std::unordered_map` / `std::unordered_set` 的哈希容器。
- `ImmutableList<T>`：构造后不可修改的列表，支持范围 for 与随机访问。
- `Stream`：基于迭代器范围的惰性 `filter/map/forEach/collect`。

设计文档：
- `libca/collection/doc/collection设计文档.md`

## thread

结构化线程、协作取消、有界队列与线程池模块。

入口头文件：
- `<libca/thread/stop_token.hpp>`
- `<libca/thread/thread.hpp>`
- `<libca/thread/bounded_queue.hpp>`
- `<libca/thread/thread_pool.hpp>`

功能：
- `StopSource` / `StopToken`：共享、幂等的协作停止状态，支持等待停止请求。
- `Thread`：类似 `std::jthread` 的 move-only 结构化线程，析构时请求停止并 join。
- `BoundedQueue<T>`：多生产者、多消费者有界队列，支持阻塞、立即和限时背压。
- `ThreadPool`：固定 worker 线程池，任务返回值与异常经 future 传播，支持排空关闭和取消待执行任务。

设计与使用文档：
- `libca/thread/doc/thread设计文档.md`
- `libca/thread/README.md`（快速示例）

## 暂未作为主线使用的代码

- `libca.core/`：旧 C++ 桌面代码，作为 legacy 参考，新增 C++ 工作优先放在 `libca/`。
- `libca/log/`、`libca/utility/`：有代码但未接入 `libca/xmake.lua`，使用前先确认是否要纳入主线。
- `libca/opt/`、`libca/reflect/`、`zip` 相关内容：规划或实验性质，使用前先看当前代码状态。

## 变更策略

libca 不维护单独的接口冻结清单，也不承诺严格长期 API/ABI 兼容。项目会尽量减少无意义的破坏性改动；当接口设计、错误模型、所有权语义或模块边界需要调整时，可以进行不兼容变更，并应在 README、CHANGELOG 或相关模块文档中说明影响和迁移方式。
