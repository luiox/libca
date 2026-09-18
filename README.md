# libca [![CI](https://github.com/luiox/libca/actions/workflows/ci.yml/badge.svg)](https://github.com/luiox/libca/actions/workflows/ci.yml) [![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

C++17 基础设施库：把 Rust 的核心语义带进现代 C++ —— `Result<T,E>`/`Option<T>` 错误处理、UTF-8 所有权字符串、Rust 风格容器，外加 JSON/HTTP/子进程/线程池/日志/ZIP 等 26 个开箱模块。

## 特性

* **Rust 语义**：`Result<T,E>` 替代异常，`Option<T>` 替代裸空值，错误处理强制显式
* **全程 UTF-8**：`Utf8String`/`Utf8StringRef` 所有权字符串族，无隐式编码转换
* **Rust 风格容器**：ArrayList/HashMap/HashSet/ImmutableList/Stream
* **模块化分层**：单向依赖（core → str/fs → json/net/http → 业务），模块零耦合
* **按需链接**：包消费可声明 `modules = "core,str,json"`，只链用到的子库
* 全模块 Google Test 覆盖，CI 覆盖 Linux + Windows（MSVC/MinGW 双前端）

## 使用

### 接入

xmake ≥ 2.8.3，包定义来自 [luiox-repo](https://github.com/luiox/luiox-repo)：

```lua
add_repositories("luiox-repo https://github.com/luiox/luiox-repo.git")
add_requires("libca 0.0.7")   -- 默认链接除 test 外全部模块

target("app")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("libca")
```

只链子集（依赖闭包自动补全，拼错直接报错）：

```lua
add_requires("libca 0.0.7", {configs = {modules = "core,str,json"}})
```

### 代码

```cpp
#include "libca/core/result.hpp"
#include "libca/str/utf8_string.hpp"

ca::core::Result<int, std::string> parse_port(const ca::str::Utf8String& text) {
    if (text.byte_length() == 0) {
        return ca::core::Err(std::string("empty"));
    }
    return ca::core::Ok(8080);
}

int main() {
    ca::str::Utf8String s("你好，libca");
    auto r = parse_port(s);
    return r.is_ok() ? std::move(r).unwrap() : 1;
}
```

API 即文档：所有公开接口都有 Doxygen 头注释，查头文件即得用法。

## 模块

| 模块 | 职责 |
|------|------|
| **core** | `Result`/`Option`/bytes（varint/zigzag）/类型转换，全库地基 |
| **str** | UTF-8 所有权字符串族 + 内置编码转换（GBK/GB18030/CP1252/Latin-1，iconv 仅长尾回落） |
| **collection** | Rust 风格容器：ArrayList/HashMap/HashSet/ImmutableList/Stream |
| **json / toml / xml / yaml** | 四种格式的 DOM 解析与写出 |
| **csv / ini** | CSV / INI 读写 |
| **fs** | 文件与路径（封装 std::filesystem） |
| **io** | Reader/Writer 抽象、buffer 与 native stream |
| **net / http** | Socket/DNS（带 TTL 缓存，可注入解析器）/TCP/UDP/TLS 适配层/SockUtil；HTTP client/server（可选 OpenSSL，可选共享连接池与 DNS 缓存） |
| **thread** | 结构化并发：ThreadPool/StopToken/BoundedQueue/Timer/EventBus/MessageLoop |
| **process** | 子进程控制 + IPC（管道/共享内存/信号量/消息队列/无内核对象共享内存环 ShmRingQueue） |
| **crypto** | SHA-1/2/3、MD5、HMAC/HKDF/PBKDF2、CRC、base64/base32；AES（ECB/CBC/CTR，内置实现 + 可选 OpenSSL/CNG 后端） |
| **log** | 日志门面，后端可插拔（可选 spdlog） |
| **config** | 强类型配置中心：`ConfigVar<T>` 幂等注册与变更监听 |
| **opt** | 命令行选项解析 |
| **time** | 日期时间 / duration / timestamp |
| **zip** | JVM ZipFile 语义 ZIP 读写 + 流式 gzip（可选 zlib） |
| **ui** | Win32 窗口、控件、消息框（Windows） |
| **random / uuid / env** | 随机数 / UUID / 环境变量 |
| **resources / i18n** | 构建期资源嵌入 / `.lang` 消息国际化 |
| **test** | 多项目测试布局与样本定位 |

原内嵌的 C99 嵌入式组件（em）已拆分至独立仓库 [luiox/libca-em](https://github.com/luiox/libca-em)。

## 构建（本仓库开发）

```bash
xmake f -p windows -a x64 --with_tests=y -y   # Windows/MSVC
xmake f -p linux --with_tests=y -y            # Linux
xmake
xmake test -g libs/test
```

## 说明

* 本库**由 AI 生成**，优先服务于作者个人项目；pre-1.0 阶段无 API 兼容性与可用性保证，生产使用请自行评估（免责条款见 [LICENSE](LICENSE)）。
* Issue 欢迎提：作者会安排 AI 分诊处理，但不承诺时效。

## License

[Apache-2.0](LICENSE) © Canrad
