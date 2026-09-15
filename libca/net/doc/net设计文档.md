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

首版不包含 TLS、HTTP、WebSocket、异步 reactor 或事件循环。TLS 应作为独立扩展模块，
通过包装 `TcpStream` 并实现 `Reader` / `Writer` 接入，不能把证书、握手和加密状态塞进
基础 socket API。

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
- Windows 本地测试和 Linux Core CI 使用同一套 Google Test。
