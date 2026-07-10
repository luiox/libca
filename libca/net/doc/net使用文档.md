---
version: 1.0
update:
2026-07-11 - 完成 libca_net 第一版使用文档
---

# libca_net 使用文档

## 地址与 DNS

```cpp
auto ip = ca::net::IpAddress::parse("127.0.0.1").unwrap();
ca::net::SocketAddress address(ip, 8080);

auto resolved = ca::net::DnsResolver::resolve("localhost", 8080);
```

IPv6 socket 地址文本必须带方括号，例如 `[::1]:8080`。link-local scope 使用数字形式，
例如 `[fe80::1%3]:8080`。

## TCP 客户端

```cpp
auto connected = ca::net::TcpStream::connect("example.com", 80);
if (connected.is_err()) {
    handle_error(connected.unwrap_err());
    return;
}
auto stream = std::move(connected).unwrap();

const std::string request = "GET / HTTP/1.0\r\n\r\n";
stream.write_all(reinterpret_cast<const u8*>(request.data()), request.size());
auto response = stream.read_to_end(1024 * 1024);
```

`TcpStream` 实现 `io::Reader` / `io::Writer`，可以直接使用 `read_exact()`、`write_all()`、
`BufReader` 和 `BufWriter`。

需要和外部网络库互操作时，可通过 `native_socket()` 借用原生值，或通过 `into_socket()`
取回 `OwnedSocket` 所有权。外部 socket 可先由 `OwnedSocket::adopt()` 接管，再通过对应类型的
`from_socket()` 包装；调用方负责保证它确实是匹配的 TCP stream、listener 或 UDP socket。

## TCP 服务端

```cpp
ca::net::SocketAddress bind_address(
    ca::net::IpAddress::unspecified_v4(), 8080);
auto listener = ca::net::TcpListener::bind(bind_address).unwrap();

auto accepted = listener.accept().unwrap();
auto peer = accepted.peer_address;
auto stream = std::move(accepted.stream);
```

端口设为 0 时可通过 `local_address()` 读取操作系统分配的实际端口。

## 连接超时与非阻塞

```cpp
auto stream = ca::net::TcpStream::connect_timeout(
    address, std::chrono::milliseconds(500));

socket.set_read_timeout(std::chrono::milliseconds(200));
socket.set_write_timeout(std::chrono::milliseconds(200));
socket.set_nonblocking(true);
```

清除 timeout 时传入 `std::nullopt`。零或负 timeout 返回 `InvalidInput`。非阻塞操作暂时
无法完成时返回 `IoErrorKind::WouldBlock`。POSIX socket 的读写 timeout 到期也可能由内核
报告为 `WouldBlock`，跨平台代码应同时处理 `WouldBlock` 和 `TimedOut`。

## UDP

```cpp
auto socket = ca::net::UdpSocket::bind(
    ca::net::SocketAddress(ca::net::IpAddress::unspecified_v4(), 0)).unwrap();

socket.send_to(payload.data(), payload.size(), destination);

u8 buffer[2048]{};
auto received = socket.receive_from(buffer, sizeof(buffer)).unwrap();
process_datagram(buffer, received.length, received.peer_address);
```

UDP 保留数据报边界，不实现 `Reader` / `Writer`。零长度 UDP 数据报是合法消息，不表示 EOF。

## TLS 边界

基础 `TcpStream` 不提供证书、握手或加密配置。未来 TLS 扩展应包装 `TcpStream`：

```cpp
class TlsStream final : public ca::io::Reader, public ca::io::Writer
{
    ca::net::TcpStream transport_;
    // TLS provider state
};
```

这样 socket 生命周期、DNS 和 TCP 连接仍由 `libca_net` 管理，TLS provider 可以独立选择，
基础网络 API 不绑定 OpenSSL、mbedTLS 或平台专用实现。
