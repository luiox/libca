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

## 流式读取 body

下载、上传与长连接协议不应先把整个 body 放入内存。读取 head 后，可重复调用
`read_body()` 消费解码后的 Content-Length、chunked 或 close-delimited body：

```cpp
auto head = reader.read_response_head("GET");
if (head.is_err() || !head.unwrap().has_value()) return;

std::array<u8, 8192> buffer{};
while (!reader.body_finished()) {
    auto read = reader.read_body(buffer.data(), buffer.size());
    if (read.is_err()) return;
    consume(buffer.data(), read.unwrap());
}
auto trailers = reader.finish_body();
if (trailers.is_err()) return;
```

读取下一条 keep-alive/pipelined 报文前必须调用 `finish_body()`；不需要内容时用
`discard_body()` 读到消息边界并丢弃。即使 `body_info().kind` 是 `None`，也必须显式
finish。close-delimited body 以连接 EOF 结束，因此完成后不能再复用该连接。

## 流式写入 chunked body

HTTP/1.1 的未知长度 request/response 使用 `begin_chunked_request()` 或
`begin_chunked_response()`。返回对象独占当前 `Http1Writer`，适合逐条发送 SSE event：

```cpp
ca::http::HttpResponseHead head;
head.headers.append("Content-Type", "text/event-stream");

auto begun = writer.begin_chunked_response(head, "GET");
if (begun.is_err()) return;
auto body = std::move(begun).unwrap();

if (body.write_chunk("event: message\ndata: {}\n\n").is_err()) return;
if (body.flush().is_err()) return;
if (body.finish().is_err()) return;
```

`finish()` 会写 final chunk 与 trailers，但不会隐式 flush。析构也不会补 final chunk；若
放弃未完成的 body，wire 报文保持不完整，所属 writer 不能继续复用，应关闭连接。

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
