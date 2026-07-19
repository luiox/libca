---
version: 1.0
update:
2026-07-19 - 完成 HTTP/1 数据模型、URL、headers 与同步 codec
---

# libca_http 设计文档

## 1. 目标与边界

`libca_http` 建立在 `libca_net` 与 `libca_io` 上，为 C++17 提供可复用的同步 HTTP 基础设施。
首个增量只实现与连接来源无关的报文层：

- http/https absolute URL 分解。
- HTTP/1.0 与 HTTP/1.1 request/response 数据模型。
- headers 的合法性、重复字段和大小写不敏感查询。
- 基于 `Reader` / `Writer` 的增量读取与序列化。
- Content-Length、chunked、trailers、response close-delimited body。
- 不可信报文的结构校验和资源限制。

依赖方向为：

```text
libca_core <- libca_io <- libca_net <- libca_http
```

本增量不实现 DNS/TCP 连接管理、连接池、服务端 accept loop、路由、SSE、压缩、代理、
WebSocket、HTTP/2 或 TLS。client/server 在后续增量复用同一 codec；TLS 通过实现相同
`Reader` / `Writer` 边界的可选 OpenSSL stream 接入，不能进入 HTTP/1 parser。

## 2. 数据与所有权

`HttpRequest` / `HttpResponse` 是完整缓冲报文。method、target、reason 和 header 使用
`std::string`，因为它们属于外部 wire protocol 边界；body 使用 `ca::core::Bytes`，不假设
UTF-8。chunked trailers 与普通 headers 分开保存，避免调用方误把 trailer 当成首部安全决策。

`HttpHeaders` 内部使用 `vector<HttpHeader>`：

- 保留接收顺序和原始字段名大小写。
- 允许 `Set-Cookie` 等合法重复字段。
- 查询、set 和 remove 按 ASCII 大小写不敏感匹配。
- 字段名必须是 RFC token；值拒绝 CR、LF、NUL 和非法控制字节，阻断 header injection。

不能用 `std::map`，因为它会丢失重复字段与顺序；也不能把 header value 强制为 UTF-8，
因为 HTTP 允许 `obs-text` 字节。

## 3. URL

`HttpUrl` 只接受 http/https absolute URL，拥有 scheme、host、有效端口和 origin-form target。
空 path 规范化为 `/`，query 保留，fragment 不发送。IPv6 host 输入必须使用方括号，内部
存储时去掉方括号，生成 Host authority 时恢复。

首版明确拒绝 userinfo、端口 0、超范围端口、反斜杠、控制字符、未加方括号 IPv6 和
非 ASCII reg-name。IDNA、percent-encoded host 与 IPv6 zone id 留给独立 URL 能力扩展，
不能在 HTTP client 中临时猜测。

## 4. Codec 与缓冲

`Http1Reader` 借用底层 `Reader`，内部持有 8 KiB 固定预读缓冲。它不使用拥有型
`io::BufReader`，因为 TCP/TLS 连接还需要由同一上层对象写响应。reader 实例必须在整条
keep-alive 连接期间存活，才能保留已经读到下一条报文的字节。

解析按 CRLF 行、headers 和 body 三阶段进行。干净 EOF 只允许发生在下一条 start-line
之前；start-line、headers、定长 body 或 chunk 中途 EOF 都是 `InvalidMessage`。

`Http1Writer` 同样只借用 `Writer`。完整缓冲报文没有显式 framing 时，writer 根据 body
补 Content-Length；显式 chunked 时把 body 写成一个或零个 data chunk，再写 trailers。
writer 不隐式 flush，连接管理层决定一条或一批报文何时提交底层缓冲。

## 5. Framing 与安全

报文 framing 遵守以下严格规则：

- Transfer-Encoding 与 Content-Length 并存直接拒绝。
- 多个 Content-Length 只有全部十进制值相同才接受，包括逗号合并形式。
- 首版只支持单一、最终的 `chunked` transfer coding，其它 coding 返回 `Unsupported`。
- HTTP/1.0 request 禁止 Transfer-Encoding。
- HTTP/1.1 request 必须有且只有一个非空 Host，含逗号的合并 Host 也拒绝。
- 裸 LF、header name 前空白、obs-fold、控制字符和 framing trailer 全部拒绝。
- HEAD、CONNECT 2xx、1xx、204、304 按无 body 语义处理，不消费下一条报文字节；HEAD、
  304 的 framing 字段只作为所选表示的元数据，CONNECT 2xx 按规范忽略这些字段。
- 1xx、204 严格拒绝 Content-Length 与 Transfer-Encoding；HTTP/1.0 request/response
  均拒绝 Transfer-Encoding。
- writer 禁止给无 body response 配置 trailers，并禁止在 CONNECT 2xx 中发送 framing 字段。

这些约束优先避免 request smuggling 和连接解析分歧，不为历史宽松实现增加兼容分支。

## 6. 错误模型与限制

`HttpResult<T>` 使用 `HttpError`，区分 IO、URL、协议、header/body 限制和未支持能力。
底层 `IoError` 完整保留，可由 `io_error()` 读取。协议错误不伪装成 IO `InvalidData`，便于
服务端映射 400/413/431，客户端区分网络失败与远端报文错误。

`HttpLimits` 分别限制 start-line、headers 字节、header 数量和解码后 body 字节。普通
headers 与 trailers 共享 byte/count 预算；chunked 的限制作用于解码后累计 body，不能通过
拆成许多小 chunk 绕过。

## 7. 测试策略

单元测试使用每次只返回少量字节的 Reader，覆盖：

- 跨任意读取边界的 start-line、headers、Content-Length 和 chunked。
- 同一输入中的连续 request/response，验证预读字节不丢失。
- chunk extension、trailers、HEAD/204 和 close-delimited response。
- writer 到 reader 的 fixed/chunked round-trip。
- CL/TE、冲突长度、重复/合并 Host、裸 LF、obs-fold、HTTP/1.0 TE 和截断 body。
- start-line、header count/bytes 和 body 限制。
- URL 的默认端口、query、fragment、IPv6 与非法 authority。

Windows 在本地测试；Linux 由 Core CI 构建并运行相同目标。
