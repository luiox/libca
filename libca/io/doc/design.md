---
version: 1.0
update:
2026-07-11 - 完成 libca_io 第一版设计
---

# libca_io 设计文档

## 1. 目标与边界

`libca_io` 为文件、进程管道和网络连接提供统一的同步字节流协议。设计参考 Rust
`std::io`，但使用 libca 的 C++17、`Result` 和命名约定。模块的核心不是重新命名
`read(2)` / `ReadFile()`，而是统一以下语义：

- EOF、短读、短写和重试规则。
- 可分类并保留原生错误码的 IO 错误。
- `Reader`、`Writer`、`Seek` 三个最小能力接口。
- 可组合的缓冲读取和缓冲写入。
- 原生 handle/fd 的唯一所有权和 RAII 关闭。

首版只提供同步阻塞 IO，不设计 reactor、异步任务、超时或 socket 创建接口。异步 IO
应在未来建立于同一能力接口和错误模型之上，而不是改变基础同步语义。

依赖方向为：

```text
libca_core <- libca_io <- libca_process
                       <- libca_fs
                       <- libca_net
```

`libca_process` 当前管道接口仍保留，后续迁移时其 `PipeReader`、`PipeWriter` 和
`NamedPipeConnection` 应实现 `Reader` / `Writer`，不再维护另一套字节流规则。

## 2. 错误模型

通用 `Status` 无法表达 `WouldBlock`、`Interrupted`、`BrokenPipe`、`UnexpectedEof` 等
IO 特有条件，也不能稳定保留 `errno` / `GetLastError()`。因此模块定义：

```cpp
enum class IoErrorKind;
class IoError;

template<typename T>
using IoResult = ca::core::Result<T, IoError>;
```

`IoError` 同时保存：

- 稳定、跨平台的 `IoErrorKind`。
- 原生错误码；合成错误的原生错误码为 0。
- 带操作上下文的可读消息。

`IoError::from_native_error()` 将 POSIX errno 或 Windows error 映射到稳定类别，同时保留
原值。`to_status()` 只用于和仍使用 `Status` 的旧模块衔接，不作为 IO 主错误接口。

首版错误类别至少包含：`NotFound`、`PermissionDenied`、`ConnectionRefused`、
`ConnectionReset`、`ConnectionAborted`、`NotConnected`、`AddrInUse`、
`AddrNotAvailable`、`BrokenPipe`、`AlreadyExists`、`WouldBlock`、`InvalidInput`、
`InvalidData`、`TimedOut`、`WriteZero`、`Interrupted`、`UnexpectedEof`、`OutOfMemory`、
`Unsupported` 和 `Other`。

## 3. Reader

`Reader` 是可读字节流的抽象基类：

```cpp
class Reader
{
public:
    virtual ~Reader() = default;
    virtual IoResult<usize> read(u8* buffer, usize capacity) = 0;

    IoResult<void> read_exact(u8* buffer, usize length);
    IoResult<ca::core::Bytes> read_to_end(usize max_length);
};
```

核心契约：

- `capacity == 0` 返回 `Ok(0)`，不要求 `buffer` 非空。
- `capacity > 0` 时空指针返回 `InvalidInput`。
- 成功返回值不得大于 `capacity`。
- `Ok(0)` 表示 EOF；非阻塞资源暂时无数据应返回 `WouldBlock`，不能伪装为 EOF。
- 一次 read 可以短读；调用方不能假设缓冲区会被填满。
- `read_exact()` 自动处理短读和 `Interrupted`，提前 EOF 返回 `UnexpectedEof`。
- `read_to_end()` 设置最大长度，避免不可信输入导致无界内存增长。

## 4. Writer

`Writer` 是可写字节流的抽象基类：

```cpp
class Writer
{
public:
    virtual ~Writer() = default;
    virtual IoResult<usize> write(const u8* data, usize length) = 0;
    virtual IoResult<void> flush() = 0;

    IoResult<void> write_all(const u8* data, usize length);
    IoResult<void> write_all(ca::core::ByteSlice data);
};
```

一次 write 可以短写。`write_all()` 自动处理短写和 `Interrupted`；非空输入返回
`Ok(0)` 时转换为 `WriteZero`，避免无限循环。`flush()` 只保证该 Writer 自己维护的缓冲
已经交给下一层，不承诺磁盘持久化；需要 `fsync` / `FlushFileBuffers` 时由文件模块提供
明确的持久化接口。

## 5. Seek

`Seek` 表达可随机定位资源的能力，不是所有 Reader/Writer 都必须实现它：

```cpp
enum class SeekOrigin { Start, Current, End };

class SeekFrom
{
public:
    static SeekFrom start(u64 position);
    static SeekFrom current(i64 offset);
    static SeekFrom end(i64 offset);
};

class Seek
{
public:
    virtual ~Seek() = default;
    virtual IoResult<u64> seek(const SeekFrom& position) = 0;

    IoResult<u64> stream_position();
    IoResult<void> rewind();
};
```

`Start` 使用无符号绝对位置，`Current` / `End` 使用有符号相对偏移。管道和 socket 不应
假装支持定位，应返回 `Unsupported`。

## 6. 缓冲读写

### 6.1 BufReader

`BufReader` 拥有一个 `std::unique_ptr<Reader>` 和固定容量缓冲区，并自身实现 `Reader`。
它提供：

- `read()`：优先消费已缓冲数据；大块读取在缓冲为空时直接透传到底层。
- `fill_buf()`：返回在下一次可变操作前有效的 `ByteSlice`。
- `consume()`：显式推进缓冲区，越界返回 `InvalidInput`。
- `read_until()`：读取到指定分隔符或 EOF，适合协议行和帧头解析。

底层对象由缓冲器独占，避免非缓冲读取绕过缓冲区后破坏顺序。需要共享底层资源时，
调用方应在更高层提供同步代理，而不是让 `BufReader` 保存裸引用。

### 6.2 BufWriter

`BufWriter` 拥有一个 `std::unique_ptr<Writer>` 和固定容量缓冲区，并自身实现 `Writer`。

- 小写入先进入缓冲区。
- 缓冲为空时，大于等于缓冲容量的写入直接透传。
- 缓冲写出发生部分成功后又报错时，只移除已经成功写出的前缀，未写数据保留以供重试。
- `flush()` 先完整写出本地缓冲，再调用底层 `flush()`。
- 析构只做 best-effort 缓冲写出，错误无法从析构返回，因此业务代码必须显式 flush。

缓冲器为 move-only。移动后对象不再拥有底层流，继续操作返回 `InvalidInput`。

## 7. 原生 handle/fd RAII

### 7.1 OwnedHandle

`OwnedHandle` 是 move-only 的唯一所有权包装：

- Windows 保存可由 `CloseHandle()` 关闭的 `HANDLE`。
- POSIX 保存可由 `close()` 关闭的 fd，fd 0 是有效值。
- `adopt()` 明确表示接管外部原生资源。
- `duplicate()` 使用 `DuplicateHandle()` / `dup()` 创建独立所有权。
- `close()` 显式关闭并返回 `IoError`；析构执行 noexcept best-effort close。
- `release()` 放弃 RAII 并把原生值交还调用方。

Windows `SOCKET` 必须调用 `closesocket()`，不属于 `OwnedHandle`。未来 `libca_net` 应定义
自己的 `OwnedSocket`，但 socket 流仍实现相同 `Reader` / `Writer`。

### 7.2 NativeStream

`NativeStream` 拥有 `OwnedHandle`，并实现 `Reader`、`Writer` 和 `Seek`：

- Windows 使用 `ReadFile()`、`WriteFile()`、`SetFilePointerEx()`。
- POSIX 使用 `read()`、`write()`、`lseek()`。
- 平台调用长度被限制到各自单次调用上限，返回短读/短写而不是截断总请求语义。
- Windows 管道的 `ERROR_BROKEN_PIPE` 在读取侧转换为 EOF。
- 对管道执行 seek 返回 `Unsupported`。

文件和进程管道可直接使用或委托给 `NativeStream`。socket 因 Windows 关闭与 IO API 不同，
由 net 模块实现同一抽象而不复用 `NativeStream`。

## 8. 并发与生命周期

- 接口本身不承诺同一对象可被多个线程无锁并发调用。
- 不同对象可以并发使用；共享对象由调用方串行化。
- `Reader` / `Writer` 的虚析构保证通过接口指针安全释放实现对象。
- 缓冲区返回的 `ByteSlice` 不拥有内存，任何后续读取、consume、移动或析构都会使其失效。
- `OwnedHandle` 和 `NativeStream` 移动后原对象为空，不会重复关闭。

## 9. 测试策略

测试必须覆盖：

- IO 错误类别、原生错误码保留和 Status 转换。
- Reader 的短读、精确读取、EOF、Interrupted 重试和最大长度限制。
- Writer 的短写、write zero、Interrupted 重试和 flush。
- SeekFrom 三种来源、rewind 和当前位置。
- BufReader 的跨缓冲读取、直通读取、fill/consume、分隔符与 EOF。
- BufWriter 的聚合写、容量触发、直通写、部分写后错误保留和显式 flush。
- OwnedHandle 的移动、release、duplicate、显式 close 和析构关闭。
- NativeStream 在真实匿名管道上的读写 EOF，以及真实临时文件上的 seek。

Windows 在本地测试，Linux 平台由 Core CI 构建并运行同一测试目标。
