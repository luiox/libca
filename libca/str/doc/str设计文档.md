---
version: 1.0
update:
2026-07-06 - 首版，补充 str 模块职责、UTF-8 类型族与 StringUtil 字节工具边界
---

# libca::str 设计文档

> 本文讲 str 模块的架构和设计边界。具体 API 使用请看头文件：
> `utf8_string.hpp`、`string_util.hpp`、`char_util.hpp`、`conversion.hpp`、`cstring.hpp`、`wstring.hpp`。

## 1. 模块定位

`libca::str` 提供字符串与文本处理基础设施。它位于 core 之上，只依赖 `libca_core`，为 fs、crypto、time 和上层业务提供统一的 UTF-8 与文本工具。

str 模块分成两类能力：

- **UTF-8 类型族**：`Utf8String`、`Utf8StringRef`、`ZUtf8StringRef`、builder、pool、arena、twine。
- **std::string 工具族**：`StringUtil`、编码转换、C/W string 适配、ASCII/URL-safe 文本工具。

## 2. UTF-8 类型族

UTF-8 是 libca 的内部文本编码。`Utf8String` 负责拥有和校验 UTF-8 字节，`Utf8StringRef` 负责非拥有视图，二者共同提供码点长度、切片、比较和查找。

设计重点：

- `Utf8String` 不可变，避免隐式共享和写时复制的复杂度。
- 码点数量在构造时缓存，`length()` 为 O(1)。
- 视图不持有内存，适合参数传递和零拷贝切片。
- 比较采用字节字典序，不做 locale collation。

更细的 UTF-8 类型设计见 `utf8_string_design.md` 和 `str-spec.md`。

## 3. StringUtil 的边界

`StringUtil` 面向 `std::string`，按字节或 ASCII 语义工作。它不承担完整 Unicode 分类、大小写折叠或规范化职责。

这种边界是有意的：

- 基础库很多场景只需要配置 key、URL 参数、日志字段、协议文本等轻量字节处理。
- 完整 Unicode 语义需要更大的数据表和规范支持，不应隐式塞进简单工具函数。
- UTF-8 码点语义应优先使用 `Utf8String` / `Utf8StringRef` 或 `char_util`。

## 4. URL-safe 文本工具

URL-safe 文本工具放在 `StringUtil` 中，因为它们处理的是 UTF-8 的字节表示，而不是 Unicode 码点语义。

当前提供三层能力：

- **ASCII 分类**：判断 ASCII 字母、数字、unreserved URL 字符，并做 ASCII 大小写转换。
- **Percent / form component**：按 RFC 3986 unreserved 集合保留字符，其它字节编码为 `%HH`；表单组件额外把空格编码为 `+`。
- **Base64url**：使用 `-` / `_` 替代 `+` / `/`，支持无 padding 和标准 padding。

decode 类接口返回 `Result<std::string, std::string>`，因为非法输入需要把错误原因交给调用方。Base64url 解码采用严格模式，会拒绝非法字符、非法 padding、非法长度和非零尾部填充位，避免同一字节串出现多个可接受编码。

## 5. 依赖与错误模型

str 只依赖 core。需要错误传播的轻量文本工具使用 `Result<T, std::string>`；领域错误枚举暂不引入，避免为很小的解析错误建立过重类型。

公共头文件写使用说明，`.cpp` 只保留实现关键点注释。设计文档不重复 API 清单，只解释为什么这样分层。

## 6. 测试策略

测试位于 `libca/str/unittest/`，按组件拆分。重点覆盖：

- UTF-8 合法/非法输入。
- 字节长度与码点长度一致性。
- 所有权、移动、池化和 arena 生命周期。
- ASCII/URL-safe 工具的 roundtrip 与非法输入拒绝。

URL-safe 工具尤其要覆盖错误路径，因为解析类函数一旦放宽非法输入，后续协议和缓存 key 很容易出现不可见的不一致。
