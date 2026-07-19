# libca_http

同步 HTTP/1.0/1.1 报文、URL 与 header 基础模块，命名空间 `ca::http`。

> 当前版本提供 codec，不包含连接管理、路由和 TLS。设计边界见 `doc/design.md`，接口签名
> 以头文件 Doxygen 注释为准。

## 读写报文

`Http1Reader` 与 `Http1Writer` 只借用 `io::Reader` / `io::Writer`。同一个 `TcpStream` 可以
同时作为读写端，reader 会保留预读字节，可连续处理 keep-alive 或 pipelined 报文。

```cpp
auto connected = ca::net::TcpStream::connect("example.com", 80);
if (connected.is_err()) return;
auto stream = std::move(connected).unwrap();

ca::http::HttpRequest request;
request.method = "GET";
request.target = "/";
request.headers.append("Host", "example.com");

ca::http::Http1Writer writer(stream);
if (writer.write_request(request).is_err()) return;

ca::http::Http1Reader reader(stream);
auto response = reader.read_response("GET");
if (response.is_err() || !response.unwrap().has_value()) return;
```

body 使用 `ca::core::Bytes`，不假设文本编码。headers 保序、允许重复，并按 ASCII
大小写不敏感查询。

## 解析限制

```cpp
ca::http::HttpLimits limits;
limits.max_start_line_bytes = 8 * 1024;
limits.max_header_bytes = 64 * 1024;
limits.max_header_count = 100;
limits.max_body_bytes = 8 * 1024 * 1024;

ca::http::Http1Reader reader(stream, limits);
```

codec 支持 Content-Length、chunked、trailers 和 response close-delimited body。解析器严格
拒绝 CL/TE 并存、冲突 Content-Length、裸 LF、obs-fold、重复 Host 和超限报文。

## URL

```cpp
auto url = ca::http::HttpUrl::parse("https://[::1]:8443/mcp?q=1").unwrap();
url.host();       // ::1
url.port();       // 8443
url.target();     // /mcp?q=1
url.authority();  // [::1]:8443
```

URL parser 支持 http/https、DNS/IPv4 host、方括号 IPv6、显式端口、query 和 fragment
剥离。userinfo、未加方括号的 IPv6、控制字符和非法端口会返回 `InvalidUrl`。
