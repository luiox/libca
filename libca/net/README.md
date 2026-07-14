# libca_net

跨平台网络子库：地址、DNS、TCP、UDP。命名空间 `ca::net`。

> 设计与选型见 `doc/design.md`；以下为快速示例。接口签名见头文件 Doxygen 注释。

## 地址与 DNS

```cpp
auto ip = ca::net::IpAddress::parse("127.0.0.1").unwrap();
ca::net::SocketAddress address(ip, 8080);

auto resolved = ca::net::DnsResolver::resolve("localhost", 8080);
```

IPv6 socket 地址文本必须带方括号（`[::1]:8080`）；link-link scope 用数字形式（`[fe80::1%3]:8080`）。

## TCP 客户端

```cpp
auto connected = ca::net::TcpStream::connect("example.com", 80);
if (connected.is_err()) { /* ... */ return; }
auto stream = std::move(connected).unwrap();

const std::string request = "GET / HTTP/1.0\r\n\r\n";
stream.write_all(reinterpret_cast<const u8*>(request.data()), request.size());
auto response = stream.read_to_end(1024 * 1024);
```

`TcpStream` 实现 `io::Reader` / `io::Writer`，可直接配合 `read_exact()`、`write_all()`、`BufReader`、`BufWriter`。

## TCP 服务端

```cpp
ca::net::SocketAddress bind_address(ca::net::IpAddress::unspecified_v4(), 8080);
auto listener = ca::net::TcpListener::bind(bind_address).unwrap();

auto accepted = listener.accept().unwrap();
auto stream = std::move(accepted.stream);
```

端口设为 0 时可经 `local_address()` 读取系统分配的实际端口。

## 超时与非阻塞

```cpp
auto stream = ca::net::TcpStream::connect_timeout(address, std::chrono::milliseconds(500));
socket.set_nonblocking(true);
socket.set_read_timeout(std::chrono::milliseconds(200));
```

清除 timeout 传 `std::nullopt`。非阻塞暂时不可完成时返回 `WouldBlock`；POSIX 读写 timeout 到期内核也可能报告为 `WouldBlock`，跨平台代码应同时处理 `WouldBlock` 和 `TimedOut`。

## UDP

```cpp
auto socket = ca::net::UdpSocket::bind(
    ca::net::SocketAddress(ca::net::IpAddress::unspecified_v4(), 0)).unwrap();

socket.send_to(payload.data(), payload.size(), destination);
u8 buffer[2048]{};
auto received = socket.receive_from(buffer, sizeof(buffer)).unwrap();
```

UDP 保留数据报边界，不实现 `Reader` / `Writer`。零长度数据报是合法消息，不表示 EOF。
