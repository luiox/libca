# libca_json

JSON 读写模块，提供 SAX（事件流）与 DOM（树）两种形态。命名空间 `ca::json`，构建目标 `libca_json`。

深度集成 `ca::str`：输入接受 `Utf8StringRef`（零拷贝指向原文本），DOM 字符串值用 `Utf8String`。
错误用 `Result<JsonValue, ParseError>`，不用异常。

> 设计与取舍见 `doc/json设计文档.md`；以下为快速示例。接口签名见头文件 Doxygen 注释。

## 引入

```cpp
#include "libca/json/json.hpp"

using namespace ca::json;
```

构建时依赖 `libca_json`、`libca_str`、`libca_core`。

## DOM 读取

```cpp
auto result = JsonReader::read(Utf8StringRef::from_cstr(R"({"name":"Alice","age":30})"));
if (result.is_err()) {
    auto& err = std::move(result).unwrap_err();
    // err.location.line / .column / err.message
    return;
}

JsonValue root = std::move(result).unwrap();
const JsonValue* name = root.find(Utf8StringRef::from_cstr("name"));
if (name != nullptr) {
    // name->as_string() == "Alice"
}
```

`JsonValue` 是 move-only（树所有权清晰），需要深拷贝用 `clone()`。

## SAX 流式（大文件零内存峰值）

需要时跳过 DOM，自己实现 `JsonHandler` 接收事件：

```cpp
class CountHandler : public JsonHandler {
public:
    int null_count = 0;
    void on_null(const SourceLocation&) override { ++null_count; }
    void on_error(const ParseError& err) override { /* ... */ }
};

CountHandler h;
JsonParser parser(big_text_view, h);
if (!parser.parse()) {
    // parser.last_error()
}
// h.null_count 即 null 值的个数，全程不构造 JsonValue 树
```

## DOM 编辑 + 写回

```cpp
JsonValue root = JsonValue::make_object();
root.set(Utf8String::from_cstr("name"), JsonValue::make_string(Utf8String::from_cstr("Bob")));
root.set(Utf8String::from_cstr("scores"), JsonValue::make_array());
root.find(Utf8StringRef::from_cstr("scores"))->append(JsonValue::make_int(95));

JsonWriterOptions opts;
opts.pretty = true;
Utf8String text = JsonWriter::write(root, opts);
```

`write` 默认输出紧凑 JSON；开启 `pretty` 后按 `indent` 缩进换行；`ensure_ascii` 把所有
非 ASCII 字符转义为 `\uXXXX`（BMP）或 surrogate pair（astral plane）。

## 读写文件

```cpp
auto read_result = JsonReader::read_file(Utf8StringRef::from_cstr("data.json"));
auto write_result = JsonWriter::write_file(Utf8StringRef::from_cstr("out.json"), root);
```

## 宽松选项

默认严格遵循 RFC 8259。需要时开启非标准扩展：

```cpp
JsonReaderOptions opts;
opts.allow_trailing_comma = true;  // [1,2,] / {"a":1,}
opts.allow_comments = true;        // // 行注释 和 /* 块注释 */
```

## number 的 int/float 判定

JSON 规范不区分整数与浮点，本库按字面量形态判定：不含 `.`/`e`/`E` 的数字解析为
`Int`（i64），否则为 `Float`（f64）。超出 i64 范围的整数自动降级为 Float。访问时可用
`is_int()` / `is_float()` 判别，或用 `as_int_or(fallback)` / `as_float_or(fallback)` 安全取值。
