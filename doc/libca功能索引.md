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

## ini

INI 配置读写模块，保格式（读改写时保留人工注释、空行和顺序）。采用 **Arena 架构**：
`IniDocument` 内嵌 `Utf8StringArena`，所有字符串字段（`IniLine`/`LineRecord` 的 section/key/
value/raw 等）存 `Utf8StringRef`，析构时 arena 一次性释放。输入 `Utf8StringRef`，
值 `Utf8StringRef`（生命周期绑定 document），错误 `ParseError`。

入口头文件：
- `<libca/ini/ini.hpp>`（聚合头）
- `<libca/ini/ini_document.hpp>`
- `<libca/ini/ini_reader.hpp>`
- `<libca/ini/ini_writer.hpp>`
- `<libca/ini/parse_error.hpp>`
- `<libca/ini/source_location.hpp>`

功能：
- `IniDocument`：保格式数据模型 + 内嵌 `Utf8StringArena`。按文件顺序保存行节点（`detail::LineRecord`，字段全为 Utf8StringRef）+ section/key 索引（键也是 Utf8StringRef）。禁拷贝、仅移动。
- `IniReader`：把字符串/文件解析为 `IniDocument`，返回 `Result<IniDocument, ParseError>`。所有字符串经 arena.intern 入池。
- `IniWriter`：按行节点顺序写回，返回 `Utf8String`。
- 类型化访问：`get`（返回 Utf8StringRef）/`get_int`/`get_double`/`get_bool`/`get_or`，自动剥首尾配对引号后转换。`sections()`/`keys()` 返回 `vector<Utf8StringRef>`。
- 保格式：set 只重建受影响行，保留缩进、分隔符、行内注释；带引号 value 修改后保留引号风格。
- 选项：`allow_global_keys`、`hash_comment`/`semicolon_comment`、
  `on_duplicate_section`/`on_duplicate_key`（`KeepLast`/`Error`）、
  `inline_comment_strict_whitespace`；Writer 的 `line_ending`。

设计与使用文档：
- `libca/ini/doc/ini设计文档.md`
- `libca/ini/README.md`（快速示例）

## csv

CSV 表格读写模块（RFC 4180）。采用 **Arena 架构**：`CsvDocument` 内嵌 `Utf8StringArena`，
字段经 `arena.intern_raw(...)` 入池（不校验 UTF-8，按原始字节保留——CSV 不规定编码，字段
可能含任意字节）。字段存 `Utf8StringRef`，生命周期绑定 document。IO 边界接入 `ca::str`：
输入 `Utf8StringRef`，输出 `Utf8String`，错误 `ParseError`（带行+列）。

入口头文件：
- `<libca/csv/csv.hpp>`（聚合头）
- `<libca/csv/csv_document.hpp>`
- `<libca/csv/csv_reader.hpp>`
- `<libca/csv/csv_writer.hpp>`
- `<libca/csv/parse_error.hpp>`
- `<libca/csv/source_location.hpp>`

功能：
- `CsvRow` / `CsvDocument`：表格数据模型（字段为 Utf8StringRef，禁拷贝仅移动），可选标题行 + 若干记录行。
- `CsvDocument::intern_field`：把字节区间经 `intern_raw` 入池（不校验 UTF-8）。
- `CsvReader`：把字符串/文件解析为 `CsvDocument`，返回 `Result<CsvDocument, ParseError>`。
  支持 quoted comma、字段内双引号转义、quoted field 内换行、CRLF/LF。
- `CsvWriter`：序列化为 `Utf8String`，按需加引号转义；`always_quote` 强制全加引号；
  `validate_utf8`（默认 true）控制输出是否校验 UTF-8，置 false 可输出含非 UTF-8 字节的字段。
- 选项：`first_row_is_header`、`delimiter`/`quote`、`trim_unquoted_space`；
  Writer 的 `line_ending`、`write_header`、`validate_utf8`。

设计与使用文档：
- `libca/csv/doc/csv设计文档.md`
- `libca/csv/README.md`（快速示例）

## json

JSON 读写模块，提供 SAX（事件流）与 DOM（树）两种形态。采用 **Arena 架构**：DOM 字符串值
与 object key 用 `Utf8StringRef`，指向所属 `JsonDocument` 内部的 `Utf8StringArena`，
消灭零散堆分配，object key 自动去重。SAX 字符串事件同样传 `Utf8StringRef`（指向 parser
关联的 arena）。输入用 `Utf8StringRef`（零拷贝指向原文本），错误用
`Result<JsonDocument, ParseError>`。

入口头文件：
- `<libca/json/json.hpp>`（聚合头）
- `<libca/json/json_value.hpp>`
- `<libca/json/json_document.hpp>`
- `<libca/json/json_handler.hpp>`
- `<libca/json/json_parser.hpp>`
- `<libca/json/json_dom_builder.hpp>`
- `<libca/json/json_reader.hpp>`
- `<libca/json/json_writer.hpp>`
- `<libca/json/parse_error.hpp>`
- `<libca/json/source_location.hpp>`

功能：
- `JsonValue`：DOM 数据模型，七种类型（null/bool/int/float/string/array/object）。String 与 object key 均为 `Utf8StringRef`，因此 JsonValue 可拷贝。number 区分 i64/f64，i64 溢出自动降级 float。
- `JsonDocument`：所有权根，持 `Utf8StringArena` + `JsonValue root_`，禁拷贝、仅移动。析构时 arena 释放所有 chunk，所有 ref 失效。
- `JsonHandler`：SAX 事件接口，用户实现后由 `JsonParser` 驱动。字符串事件传 `Utf8StringRef`。
- `JsonParser`：递归下降解析器，把输入驱动为 handler 事件；构造时接收 arena 引用，字符串经 `arena.intern(...)` 入池。
- `JsonDomBuilder`：`JsonHandler` 的 DOM 装配实现，配合 `JsonParser` 得到 `JsonDocument`。
- `JsonReader`：DOM 静态入口，`read(text)` / `read_file(path)` 返回 `Result<JsonDocument, ParseError>`。
- `JsonWriter`：把 `JsonDocument` 序列化为 `Utf8String`，支持 pretty 缩进和 ensure_ascii。
- `ParseError`：位置（行+列+字节偏移）+ 人读消息。
- 宽松选项：尾随逗号、`//` 与 `/* */` 注释（默认严格 RFC 8259）。

设计与使用文档：
- `libca/json/doc/json设计文档.md`
- `libca/json/README.md`（快速示例）

## toml

TOML 1.0 读写模块（DOM 形态）。采用 **Arena 架构**：DOM 节点存 `Utf8StringRef` 指向
所属 `TomlDocument` 内部的 `Utf8StringArena`，消灭零散堆分配，相同字符串自动去重。
输入用 `Utf8StringRef`（零拷贝指向原文本），错误用 `Result<TomlDocument, ParseError>`。

入口头文件：
- `<libca/toml/toml.hpp>`（聚合头）
- `<libca/toml/toml_value.hpp>`
- `<libca/toml/toml_document.hpp>`
- `<libca/toml/toml_datetime.hpp>`
- `<libca/toml/toml_reader.hpp>`
- `<libca/toml/toml_writer.hpp>`
- `<libca/toml/parse_error.hpp>`
- `<libca/toml/source_location.hpp>`

功能：
- `TomlValue`：DOM 节点，10 种类型（String/Integer/Float/Boolean + 4 种 datetime 变体 +
  Array/Table）。存 `Utf8StringRef` 而非 `Utf8String`，因此**可拷贝**（不像 json 的 move-only）。
- `TomlDatetime`：4 种 datetime 变体共用结构（offset date-time / local date-time /
  local date / local time），用 `Kind` 枚举区分。
- `TomlDocument`：所有权根，持有 arena + root `TomlValue`。析构即释放 arena，
  所有 `Utf8StringRef` 失效。
- `TomlReader`：把字符串/文件解析为 `TomlDocument`，返回 `Result<TomlDocument, ParseError>`。
  支持 TOML 1.0 全子集：4 种字符串、各进制整数、inf/nan、dotted keys、标准表/inline table/
  数组表、4 种 datetime。
- `TomlWriter`：序列化为 `Utf8String`，顶层 key=value 在前，子表以 `[a.b]` / `[[a.b]]`
  表头分段在后；含换行字符串用 multiline basic string 输出。
- `ParseError`：位置（行+列+字节偏移）+ 人读消息。首错即停。
- 整数超 i64 报错（与 TOML 严格语义一致，不降级 float）；重复 key / 重复 table header /
  parent-after-child 表头全部报错。允许 UTF-8 BOM。

设计与使用文档：
- `libca/toml/doc/toml设计文档.md`
- `libca/toml/doc/dev_plan.md`（开发接力文档，含 Arena 架构设计动机）
- `libca/toml/README.md`（快速示例）

## yaml

YAML **配置子集**读写模块（DOM 形态）。手写解析器、零第三方依赖，与 toml 同构的 **Arena
架构**：DOM 节点存 `Utf8StringRef` 指向所属 `YamlDocument` 内部的 `Utf8StringArena`。
输入用 `Utf8StringRef`（零拷贝指向原文本），错误用 `Result<YamlDocument, ParseError>`。
锚点/别名/标签/多文档/复杂键**明确报错拒绝**（非静默忽略）。

入口头文件：
- `<libca/yaml/yaml.hpp>`（聚合头）
- `<libca/yaml/yaml_value.hpp>`
- `<libca/yaml/yaml_document.hpp>`
- `<libca/yaml/yaml_reader.hpp>`
- `<libca/yaml/yaml_writer.hpp>`
- `<libca/yaml/parse_error.hpp>`
- `<libca/yaml/source_location.hpp>`

功能：
- `YamlValue`：DOM 节点，7 种类型（Null/Boolean/Integer/Float/String/Sequence/Mapping）。
  存 `Utf8StringRef` 因此**可拷贝**；默认构造为 Null（YAML 根可为任意节点）。Mapping 保序 +
  key 索引 O(1) find/set。
- `YamlDocument`：所有权根，持有 arena + root `YamlValue`。析构即释放 arena。
- `YamlReader`：把字符串/文件解析为 `YamlDocument`，返回 `Result<YamlDocument, ParseError>`。
  支持块式 mapping/sequence（含 `- key:` 紧凑式、零缩进 sequence）、YAML 1.2 core schema
  标量（**不支持 yes/no/on/off**，Norway problem）、单/双引号、单行 flow、块标量 `|`/`>` +
  chomping、注释、BOM/CRLF。重复 key 报错。
- `YamlWriter`：序列化为 `Utf8String`，块式输出。字符串按需加引号保证 write→read 类型保真
  （`resolve_plain_scalar != String` 即加引号），含换行字符串输出为 `|` 块标量。
- `ParseError`：位置（行+列+字节偏移）+ 人读消息。首错即停。
- 标量溢出退化为 String（core schema 语义，不像 toml 报错）。锚点/别名/标签/多文档/复杂键/
  合并键/显式缩进指示符/`%` 指令全部报错。

设计与使用文档：
- `libca/yaml/doc/yaml设计文档.md`
- `libca/yaml/README.md`（快速示例）

## xml

XML **配置子集**读写模块（DOM 形态）。手写解析器、零第三方依赖，与 toml 同构的 **Arena
架构**：DOM 节点存 `Utf8StringRef` 指向所属 `XmlDocument` 内部的 `Utf8StringArena`。
输入用 `Utf8StringRef`（零拷贝指向原文本），错误用 `Result<XmlDocument, ParseError>`。
命名空间不特殊处理（`prefix:local` 整体为名字）；DOCTYPE/DTD、自定义实体、非声明 PI
**明确报错拒绝**（非静默忽略）。

入口头文件：
- `<libca/xml/xml.hpp>`（聚合头）
- `<libca/xml/xml_node.hpp>`
- `<libca/xml/xml_document.hpp>`
- `<libca/xml/xml_reader.hpp>`
- `<libca/xml/xml_writer.hpp>`
- `<libca/xml/parse_error.hpp>`
- `<libca/xml/source_location.hpp>`

功能：
- `XmlNode`：统一 DOM 节点，4 种形态（Element/Text/Comment/Cdata）。Element 有名字 +
  属性（保序 + O(1) 索引）+ 子节点（**支持混合内容**）。存 `Utf8StringRef` 因此**可拷贝**。
  导航 `first_element(name)` / `text()` / `attribute(key)`。
- `XmlDocument`：所有权根，持 arena + 声明 + prolog/epilog + root 元素。析构即释放 arena。
- `XmlReader`：把字符串/文件解析为 `XmlDocument`，返回 `Result<XmlDocument, ParseError>`。
  支持元素/属性/文本/注释（保留为节点）/CDATA/混合内容、命名实体 + 数字字符引用、XML 声明、
  BOM/CRLF。`trim_whitespace`（默认开）丢弃元素间纯空白文本节点。
- `XmlWriter`：序列化为 `Utf8String`，缩进美化但对混合内容保真（含文本的元素整体行内输出）。
- `ParseError`：位置（行+列+字节偏移）+ 人读消息。首错即停。
- 多根、闭合标签不匹配、重复属性、未知实体、DOCTYPE/DTD、非声明 PI 全部报错。

设计与使用文档：
- `libca/xml/doc/xml设计文档.md`
- `libca/xml/README.md`（快速示例）

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

## http

建立在 net/io/thread 上的同步 HTTP/1.0/1.1 模块，提供独立于 TCP/TLS 的 codec、明文
http client、可选 OpenSSL 3 HTTPS client 与精确路由明文 server。

入口头文件：
- `<libca/http/http.hpp>`（聚合头）
- `<libca/http/client.hpp>`
- `<libca/http/server.hpp>`
- `<libca/http/http_error.hpp>`
- `<libca/http/headers.hpp>`
- `<libca/http/message.hpp>`
- `<libca/http/url.hpp>`
- `<libca/http/http1_codec.hpp>`

功能：
- `HttpRequest` / `HttpResponse`：完整缓冲报文，body 使用 `ca::core::Bytes`。
- `HttpRequestHead` / `HttpResponseHead` / `HttpBodyInfo`：流式读取前的 head 与 framing 信息。
- `HttpHeaders`：保序、允许重复、ASCII 大小写不敏感查询，拒绝 header injection 字节。
- `HttpUrl`：http/https absolute URL、DNS/IPv4、方括号 IPv6、端口、query 和 authority。
- `Http1Reader` / `Http1Writer`：完整缓冲或流式处理碎片化字节流、Content-Length、chunked、
  trailers、close-delimited response、HEAD/无 body 状态码和 keep-alive framing。
- `Http1ChunkedBodyWriter`：逐 chunk 写入与显式 flush/finalize，支持 SSE 等低延迟输出。
- `HttpClient`：HTTP/HTTPS 完整缓冲 response、同源 keep-alive、TLS verification、1xx 与分阶段总 deadline。
- `HttpServer`：method/path 精确路由、有界 worker/排队、keep-alive、stop-aware IO。
- `HttpServerResponse`：handler 返回 buffered response 或 chunked producer。
- `HttpLimits` / `HttpError`：start-line、header count/bytes、body 上限与结构化协议错误。
- 严格拒绝 CL/TE 冲突、冲突 Content-Length、裸 LF、obs-fold、重复 Host 和超限报文。

设计与使用文档：
- `libca/http/doc/design.md`
- `libca/http/README.md`（快速示例）

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
- `Stream`：基于迭代器范围的惰性 `filter/map/for_each/collect`。

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

## process

跨平台子进程控制与进程间通信模块。进程控制对齐 Rust `std::process`。

入口头文件：
- `<libca/process/subprocess.hpp>`
- `<libca/process/ipc.hpp>`

功能：
- `Command`：可复用的子进程启动配置，`spawn()`/`status()`/`output()` 三种启动方式。
- `Child`：move-only 子进程句柄，`try_wait`/`wait`/`wait_for`/`kill`，标准流经 `take_*` 取出。
- `Stdio`：标准流配置（inherit / null / piped）。
- `ipc::NamedPipeServer` / `NamedPipeClient` / `NamedPipeConnection`：命名管道（Windows Win32 管道 / Linux Unix-domain socket）。
- `ipc::SharedMemory`：共享内存（Windows 文件映射 / Linux shm_open）。
- `ipc::NamedSemaphore`：命名信号量。
- `ipc::MessageQueue`：保序消息队列（Windows mailslot / Linux POSIX mqueue）。

设计文档：
- `libca/process/doc/process设计文档.md`

## 暂未作为主线使用的代码

- `libca/log/`、`libca/utility/`：有代码但未接入 `libca/xmake.lua`，使用前先确认是否要纳入主线。
- `libca/opt/`、`libca/reflect/`：规划或实验性质，使用前先看当前代码状态。

> 历史上的旧 `libca.core/` 目录已删除。其中有价值的能力已迁移到 `libca/str`（`CharsetConverter`，
> 代码页转换）和 `libca/ui`（Win32 GUI）。剩余的 `database` / `event` / `Timer` / `Zip` / `tensor`
> 等空壳或占位实现未迁移，未来重新设计的需求见 `doc/proposals/`。

## 变更策略

libca 不维护单独的接口冻结清单，也不承诺严格长期 API/ABI 兼容。项目会尽量减少无意义的破坏性改动；当接口设计、错误模型、所有权语义或模块边界需要调整时，可以进行不兼容变更，并应在 README、CHANGELOG 或相关模块文档中说明影响和迁移方式。
