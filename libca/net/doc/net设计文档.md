---
version: 1.0
update:
2026-07-11 - 完成 libca_net 第一版设计
2026-07-19 - 加固 socket 继承边界并增加 TCP listener/连接期限配置
---

# libca_net 设计文档

## 1. 目标与边界

`libca_net` 建立在 `libca_io` 之上，为 C++17 提供跨平台同步网络基础设施。第一版覆盖：

- IPv4 / IPv6 地址与 socket 地址。
- TCP 客户端流、TCP 监听器和 UDP socket。
- 主机名 DNS 解析。
- 连接超时、读写超时和非阻塞模式。
- Windows `SOCKET` / POSIX socket fd 的唯一所有权与 RAII。

依赖方向为：

```text
libca_core <- libca_io <- libca_net
```

首版不包含 HTTP、WebSocket、异步 reactor 或事件循环。TLS 以独立的 `TlsStream` 适配层
接入（见第 11 节）：包装任意双向 `Reader` / `Writer`（不限于 `TcpStream`），不把证书、
握手和加密状态塞进基础 socket API。

## 2. 错误模型

网络操作统一返回 `ca::io::IoResult<T>`，复用 `IoErrorKind` 中已有的网络类别：

- `ConnectionRefused`
- `ConnectionReset`
- `ConnectionAborted`
- `NotConnected`
- `AddrInUse`
- `AddrNotAvailable`
- `WouldBlock`
- `TimedOut`
- `Interrupted`

Windows socket 错误必须从 `WSAGetLastError()` 读取，再通过
`IoError::from_native_error()` 保留 WSA 原生码。不能使用 `GetLastError()`。
POSIX socket 错误从 `errno` 读取。

DNS 的 `getaddrinfo()` 错误不是 errno/WSA socket 错误，按其稳定语义映射为
`NotFound`、`WouldBlock`、`OutOfMemory` 或 `Other`，消息中保留 `gai_strerror()` 文本。

## 3. 地址类型

### 3.1 IpAddress

`IpAddress` 是值类型，保存 IPv4 的 4 字节或 IPv6 的 16 字节：

```cpp
enum class IpVersion { V4, V6 };

class IpAddress
{
public:
    static IpAddress v4(u8 a, u8 b, u8 c, u8 d) noexcept;
    static IpAddress v6(const std::array<u8, 16>& octets) noexcept;
    static IpAddress localhost_v4() noexcept;
    static IpAddress localhost_v6() noexcept;
    static IpAddress unspecified_v4() noexcept;
    static IpAddress unspecified_v6() noexcept;
    static io::IoResult<IpAddress> parse(const std::string& value);

    IpVersion version() const noexcept;
    bool is_ipv4() const noexcept;
    bool is_ipv6() const noexcept;
    const std::array<u8, 16>& octets() const noexcept;
    std::string to_string() const;
};
```

文本解析和格式化使用 `inet_pton()` / `inet_ntop()`，不手写 IPv6 压缩规则。
`octets()` 对 IPv4 只使用前 4 字节，其余字节为 0。

### 3.2 SocketAddress

`SocketAddress` 组合 IP 地址和主机字节序端口：

```cpp
class SocketAddress
{
public:
    SocketAddress(IpAddress ip, u16 port, u32 flow_info = 0, u32 scope_id = 0) noexcept;
    static io::IoResult<SocketAddress> parse(const std::string& value);

    const IpAddress& ip() const noexcept;
    u16 port() const noexcept;
    u32 flow_info() const noexcept;
    u32 scope_id() const noexcept;
    std::string to_string() const;
};
```

IPv4 文本格式为 `127.0.0.1:8080`，IPv6 必须使用 `[::1]:8080`，避免地址冒号和端口
分隔符产生歧义。IPv6 scope 使用数字格式 `[fe80::1%3]:8080`；DNS 返回的 flow info 和
scope id 必须保留。地址类型不携带 DNS 主机名；主机名必须先经过解析。

## 4. socket 所有权

`OwnedSocket` 是 move-only RAII 类型：

```cpp
using RawSocket = std::uintptr_t;

class OwnedSocket
{
public:
    static io::IoResult<OwnedSocket> adopt(RawSocket socket);
    io::IoResult<OwnedSocket> duplicate() const;
    bool is_valid() const noexcept;
    RawSocket get() const noexcept;
    io::IoResult<void> close();
    RawSocket release() noexcept;
};
```

- Windows 使用 `closesocket()`，无效值为 `INVALID_SOCKET`。
- POSIX 使用 `close()`，fd 0 是有效 socket。
- Windows 创建和复制 socket 时禁用 handle 继承；POSIX 原子设置 `CLOEXEC`，避免子进程
  意外延长连接或监听端口生命周期。
- `adopt()` 明确接管关闭责任。
- `duplicate()` 在 Windows 使用 `WSADuplicateSocket()` / `WSASocket()`，在 POSIX 优先使用
  `F_DUPFD_CLOEXEC`。
- `close()` 返回可观察错误；析构执行 noexcept best-effort close。

`OwnedSocket` 不能复用 `io::OwnedHandle`，因为 Windows `SOCKET` 不能由 `CloseHandle()`
关闭。

## 5. DNS

```cpp
enum class AddressFamily { Unspecified, Ipv4, Ipv6 };
enum class SocketKind { Stream, Datagram };

class DnsResolver
{
public:
    static io::IoResult<std::vector<SocketAddress>> resolve(
        const std::string& host,
        u16 port,
        AddressFamily family = AddressFamily::Unspecified,
        SocketKind kind = SocketKind::Stream);
};
```

解析结果保持系统返回顺序并去重。空主机名返回 `InvalidInput`。DNS API 只负责同步解析，
不引入缓存线程、TTL 管理或异步回调。

## 6. TCP

### 6.1 TcpStream

`TcpStream` 拥有 `OwnedSocket`，并实现 `io::Reader` 和 `io::Writer`：

```cpp
enum class Shutdown { Read, Write, Both };

class TcpStream final : public io::Reader, public io::Writer
{
public:
    static io::IoResult<TcpStream> from_socket(OwnedSocket socket);
    static io::IoResult<TcpStream> connect(const SocketAddress& address);
    static io::IoResult<TcpStream> connect(const std::string& host, u16 port);
    static io::IoResult<TcpStream> connect_timeout(
        const SocketAddress& address,
        std::chrono::milliseconds timeout);
    static io::IoResult<TcpStream> connect_timeout(
        const std::string& host,
        u16 port,
        std::chrono::milliseconds timeout);

    io::IoResult<usize> read(u8* buffer, usize capacity) override;
    io::IoResult<usize> write(const u8* data, usize length) override;
    io::IoResult<void> flush() override;

    io::IoResult<SocketAddress> local_address() const;
    io::IoResult<SocketAddress> peer_address() const;
    io::IoResult<void> shutdown(Shutdown direction);
    io::IoResult<void> set_nonblocking(bool enabled);
    io::IoResult<void> set_read_timeout(
        std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<void> set_write_timeout(
        std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<std::optional<std::chrono::milliseconds>> read_timeout() const;
    io::IoResult<std::optional<std::chrono::milliseconds>> write_timeout() const;
    io::IoResult<void> set_nodelay(bool enabled);
    io::IoResult<bool> nodelay() const;
    io::IoResult<TcpStream> try_clone() const;
    bool is_open() const noexcept;
    RawSocket native_socket() const noexcept;
    OwnedSocket& socket() noexcept;
    const OwnedSocket& socket() const noexcept;
    OwnedSocket into_socket() noexcept;
};
```

`read()` 返回 0 表示 TCP EOF。短读和短写是正常结果。`flush()` 为 no-op，因为 socket
自身没有用户态缓冲；`BufWriter` 等适配器仍可包装它。

`connect(host, port)` 使用 `DnsResolver` 并按系统顺序尝试全部地址，返回最后一次连接错误。
`connect_timeout()` 接受已经解析的单个 `SocketAddress`，使用非阻塞 connect + select，
完成后恢复阻塞模式。timeout 必须大于 0。

`connect_timeout(host, port, timeout)` 在同步 DNS 完成后建立统一 deadline，并让系统返回的
全部地址共享这一个连接期限。同步 DNS 本身不计入 timeout。POSIX 使用 `select()` 前必须
检查 fd 小于 `FD_SETSIZE`，超过时返回 `Unsupported`，不能调用 `FD_SET()`。

### 6.2 TcpListener

```cpp
struct TcpAcceptResult
{
    TcpStream stream;
    SocketAddress peer_address;
};

class TcpListener
{
public:
    static io::IoResult<TcpListener> from_socket(OwnedSocket socket);
    static io::IoResult<TcpListener> bind(
        const SocketAddress& address, const TcpListenerOptions& options = {});
    io::IoResult<TcpAcceptResult> accept();
    io::IoResult<SocketAddress> local_address() const;
    io::IoResult<void> set_nonblocking(bool enabled);
    io::IoResult<TcpListener> try_clone() const;
    io::IoResult<void> close();
    bool is_open() const noexcept;
    RawSocket native_socket() const noexcept;
    OwnedSocket into_socket() noexcept;
};
```

端口 0 允许操作系统分配临时端口。非阻塞 listener 没有待处理连接时返回 `WouldBlock`。
`TcpListenerOptions` 在 bind 前配置正数 backlog、显式 `SO_REUSEADDR` 和可选
`IPV6_V6ONLY`；IPv4 listener 指定 `ipv6_only` 返回 `InvalidInput`。默认不启用地址复用，
避免不同平台下隐式放宽端口独占语义。

Linux 优先用 `accept4(SOCK_CLOEXEC)` 原子禁止继承；不支持 accept4 的平台在 accept 后
立即设置 `FD_CLOEXEC`。Windows accept 后清除 `HANDLE_FLAG_INHERIT`。

## 7. UDP

UDP 保留消息边界，因此不能实现面向连续字节流的 `Reader` / `Writer`：

```cpp
struct UdpReceiveResult
{
    usize length;
    SocketAddress peer_address;
};

class UdpSocket
{
public:
    static io::IoResult<UdpSocket> from_socket(OwnedSocket socket);
    static io::IoResult<UdpSocket> bind(const SocketAddress& address);
    io::IoResult<void> connect(const SocketAddress& address);
    io::IoResult<usize> send(const u8* data, usize length);
    io::IoResult<usize> receive(u8* buffer, usize capacity);
    io::IoResult<usize> send_to(
        const u8* data, usize length, const SocketAddress& address);
    io::IoResult<UdpReceiveResult> receive_from(u8* buffer, usize capacity);

    io::IoResult<SocketAddress> local_address() const;
    io::IoResult<SocketAddress> peer_address() const;
    io::IoResult<void> set_nonblocking(bool enabled);
    io::IoResult<void> set_read_timeout(
        std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<void> set_write_timeout(
        std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<std::optional<std::chrono::milliseconds>> read_timeout() const;
    io::IoResult<std::optional<std::chrono::milliseconds>> write_timeout() const;
    io::IoResult<void> set_broadcast(bool enabled);
    io::IoResult<bool> broadcast() const;
    io::IoResult<UdpSocket> try_clone() const;
    bool is_open() const noexcept;
    RawSocket native_socket() const noexcept;
    OwnedSocket into_socket() noexcept;
};
```

零长度 UDP 数据报是合法消息，`receive_from()` 的 `length == 0` 不能解释为 EOF。
若数据报大于调用方缓冲区，平台可能截断消息；第一版返回实际复制长度，不提供完整原始长度。

## 8. 超时与非阻塞

- `std::nullopt` 清除读写 timeout。
- 显式的 0ms 或负 timeout 返回 `InvalidInput`，因为系统的零值通常表示无限等待。
- Windows 使用 `SO_RCVTIMEO` / `SO_SNDTIMEO` 的毫秒值。
- POSIX 使用 `timeval`。
- Windows 使用 `ioctlsocket(FIONBIO)` 切换非阻塞。
- POSIX 使用 `fcntl(F_GETFL/F_SETFL)` 切换 `O_NONBLOCK`。
- 非阻塞操作暂时无法完成时返回 `WouldBlock`，不能伪装成 EOF。
- POSIX 的 `SO_RCVTIMEO` / `SO_SNDTIMEO` 到期通常由内核报告 `EAGAIN`，因此读写 timeout
  可能表现为 `WouldBlock`；Windows 通常映射为 `TimedOut`。调用方应同时处理这两类。

同一个 socket 的 timeout 与 nonblocking 状态属于底层 OS 对象；`try_clone()` 得到的对象共享
这些状态，但拥有独立关闭责任。

`from_socket()` 用于接入外部创建的 socket，并接管 `OwnedSocket`。调用方必须保证 socket
类型和状态匹配目标类型，例如传给 `TcpListener` 的 socket 已经完成 bind/listen。

## 9. 并发与生命周期

- 类型本身不承诺同一对象可被多线程无锁并发修改。
- `try_clone()` 可创建独立所有者，用于读写线程分别持有 socket。
- 关闭一个 clone 不关闭其它 clone；shutdown 会影响同一连接的所有 clone。
- 移动后对象为空，继续操作返回 `InvalidInput`。
- Windows Winsock 初始化由模块内部按进程执行一次，调用方不需要手工调用 `WSAStartup()`。

## 10. 测试策略

测试覆盖：

- IPv4 / IPv6 解析、格式化和 socket 地址文本。
- DNS localhost 解析与地址族过滤。
- `OwnedSocket` 的 move、duplicate、release、close 和析构。
- loopback TCP bind/connect/accept、Reader/Writer、EOF、地址查询和 clone。
- TCP 连接超时参数校验、读写 timeout 配置、TCP_NODELAY 和非阻塞 listener。
- loopback UDP send_to/receive_from、connected send/receive、零长度数据报和 broadcast 配置。
- TlsStream：内存自环真实握手（见第 11 节），单测不起网络。
- Windows 本地测试和 Linux Core CI 使用同一套 Google Test。CI 另有独立 job
  "build & test HTTP (linux, OpenSSL 3)" 作为 TLS/HTTPS 回归兜底。

## 11. TLS（TlsStream）

### 11.1 定位与结构

`TlsStream` 是 net 内的可选 TLS 适配层（`with_openssl=y` 才有实质实现，OpenSSL 3.x
为主、兼容 1.1.1 的点用条件编译最小化）。核心是一对 OpenSSL memory BIO：

```text
明文侧（io::Reader / io::Writer）
    SSL_read / SSL_write
        rbio（mem） ←── 底层流 io::Reader（对端密文灌入）
        wbio（mem） ──→ 底层流 io::Writer（密文刷出，write_all）
对端密文方向
```

- 读：`SSL_read` 返回 `WANT_READ` 时从底层流读一块密文（≤16KB）灌入 rbio 后重试；
  底层流返回 EOF 且 SSL 仍在等数据 = 对端未发 close_notify 即断开。
- 写：`SSL_write` 成功后立即把 wbio 密文 `write_all` 到底层流；`WANT_READ` 出现在
  renegotiation，同样透明处理。
- 握手（`connect` / `accept`）与 renegotiation 的 `WANT_READ` / `WANT_WRITE` 重试
  循环全部封闭在 TlsStream 内部，调用方只见阻塞语义。
- TlsStream **不拥有**底层流（借用 `io::Reader&` + `io::Writer&`，需保证存活期），
  这样 http 层可以继续持有 `TcpStream` 并暴露 `tcp_stream()`（socket 级超时、
  连接池探针都依赖它）；实现细节经 PIMPL 隐藏，公开头文件不出现 OpenSSL 类型。
- OpenSSL 句柄只有一个 `SSL*`（`SSL_set_bio` 后两个 mem BIO 由 SSL 接管），
  `SSL_CTX` 归 `TlsServerContext` 持有，多连接共享只读。
- 握手总期限在每个重试迭代边界检查；`TlsHandshakeControl::before_retry` 钩子供
  调用方在每次重试前收紧底层 socket 超时或检查协作停止标记（http server 的
  stop 轮询、client 的剩余期限收紧都走它）。没有超时能力的底层流（内存流）
  只能受迭代边界约束。

### 11.2 安全默认值

- **证书校验默认开启**（`verify_peer=true`）：`SSL_VERIFY_PEER` + `SSL_set1_host` /
  IP SAN 校验（`X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS`）。显式置 `false` 同时关闭
  证书链与主机名校验，Doxygen 以 @warning 高亮中间人风险。
- **SNI 默认 = host**；host 为 IP 字面量时不发 SNI（对齐主流 client 语义）。
- **最低版本 TLS 1.2** 硬编码（与原 http 实现一致，无收紧项遗留）。
- **信任库**：`ca_file` / `ca_directory` 任一非空走自定义信任，否则 OpenSSL 默认
  trust paths。
- **mTLS**：`TlsServerOptions::verify_client=true` 时要求并校验客户端证书
  （`SSL_VERIFY_FAIL_IF_NO_PEER_CERT`）。
- **ALPN**：client 侧可选提供协议列表，`alpn_selected()` 返回协商结果；server 侧
  暂不配置选择回调（保持 http 行为等价）。

### 11.3 错误分类

统一映射为 `IoErrorKind`，调用方可判别：

| 场景 | kind | 说明 |
|------|------|------|
| 证书校验失败 | `InvalidData` | 消息含 "certificate verification failed" 与 X509 原因串 |
| TLS 协议错误（alert/记录层） | `InvalidData` | 消息含 OpenSSL 错误文本 |
| 对端未发 close_notify 即断开 | `UnexpectedEof` | 含握手阶段 |
| 干净关闭（close_notify） | read 返回 0 | Rust EOF 语义 |
| 底层流错误 | 原样透传 | 保留 kind 与原生错误码（`TimedOut`/`WouldBlock` 等） |
| 配置/加载失败（证书文件等） | `InvalidInput` | |
| 未启用 with_openssl | `Unsupported` | stub 降级 |

`shutdown()` 发送 close_notify 后立即刷出，单向不等待对端；析构不隐式关闭
（对齐原 http 行为，由调用方决定是否干净关闭）。

### 11.4 无 OpenSSL 降级

`with_openssl=n`（默认）时整个实现编译为 stub：`tls_stream_supported()` 返回
false，`connect` / `accept` / `load` 返回 `Unsupported`，公开头文件不引入任何
OpenSSL 依赖，net 默认构建不拉取 openssl3 包（学 core/minidump 的平台 stub 模式）。

### 11.5 测试与性能参考

单测不起网络：一对内存双工流（`libca/net/test/mem_duplex.hpp`，阻塞/非阻塞两种
读模式）让 client / server 两个 TlsStream 经 mem-BIO 自环完成真实 TLS 握手；
`libca/net/test/` 内预置自签测试证书（CA + CN=localhost/SAN=DNS:localhost 的
服务端证书与私钥 + 一个不受信任对照 CA，文件头注明仅供测试），使证书校验、
主机名验证、SNI 被真实 exercised。用例覆盖：握手、双向 echo、4MiB 分片传输
（FNV-1a 校验防损坏）、对端提前断开（握手中/握手后）、主机名不匹配默认拒绝、
显式关闭校验放行、错误分类断言；无 OpenSSL 构建验证 stub 降级。

性能基准 target `libca_net_perf`（`set_group("libs/perf")`、默认不构建、不注册
add_tests；Stopwatch 计时、热身 + 多轮取中位数、FNV-1a 校验和防空转）：

| 指标 | 参考值 | 环境 |
|------|--------|------|
| mem-BIO 自环明文吞吐 | ~40 MiB/s | x64 MSVC 2022 release，OpenSSL 3.6.3，8MiB/轮 |
| 完整 TLS 握手 | ~390 次/秒 | 同上（client/server 双线程并发驱动） |
