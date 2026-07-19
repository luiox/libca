# libca_http

同步 HTTP/1.0/1.1 codec、client 与 server 模块，命名空间 `ca::http`。

> 当前版本只连接明文 http。https 需要后续可选 TLS stream；设计边界见 `doc/design.md`，
> 接口签名以头文件 Doxygen 注释为准。

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

## Client

`HttpClient` 完整缓冲 response，并在相同 http origin 上复用一条 keep-alive 连接。URL
始终覆盖 request 的 target 与 Host，避免实际连接地址和 wire authority 分歧：

```cpp
auto url = ca::http::HttpUrl::parse("http://127.0.0.1:8080/data").unwrap();
auto client = ca::http::HttpClient::create().unwrap();

auto response = client.get(url);
if (response.is_err()) return;
if (response.unwrap().status != 200) return;
```

`HttpClientOptions` 分别控制 connect、request write、response head/body 总期限与解析限制。
client 会跳过有限数量的 1xx response，正确处理 close-delimited body，并在 framing 或
`Connection` 不允许复用时关闭连接。当前不做 redirect、cookie、代理、压缩和 https。

## Server

`HttpServer::bind()` 在调用线程外不启动后台任务；注册路由后，`serve()` 在当前线程进入
accept loop，另一个线程可调用 `stop()`：

```cpp
auto address = ca::net::SocketAddress(ca::net::IpAddress::localhost_v4(), 8080);
auto server = ca::http::HttpServer::bind(address).unwrap();

server.route("POST", "/mcp", [](const ca::http::HttpServerRequestContext& context) {
    ca::http::HttpResponse response;
    response.status = 200;
    response.body = context.request().body;
    return ca::core::Ok(ca::http::HttpServerResponse::buffered(std::move(response)));
});

auto served = server.serve();
```

路由按区分大小写的 method 与不含 query 的 origin-form path 精确匹配；path 匹配而 method
不匹配返回 405，其它未匹配请求返回 404。`HttpServerResponse::chunked()` 接受同步 producer，
producer 可逐块写入并 flush SSE event；producer 成功后 server 显式写 final chunk，失败则直接
关闭不完整连接。

server 使用固定 worker 数和有界 pending connection 队列，过载连接返回 503。idle、request
head/body、response write 均有独立期限；accept loop 发送 503 并关闭连接使用更短的
`overload_response_timeout`，避免慢连接长时间阻塞后续 accept。`stop()` 请求协作取消，活动 IO
最多一个 `stop_poll_interval` 后退出。单连接达到 `max_requests_per_connection` 后会发送
`Connection: close`。
