# 提案：JSON Schema 校验器

## 背景

`libca/json` 目前提供 RFC 8259 的解析（SAX + DOM）、序列化（writer）和 DOM 编辑能力，
但缺少**结构校验**：给定一份 JSON 文档和一份 schema，判断文档是否符合 schema 描述的
形状（有哪些字段、字段是什么类型、取值范围如何）。

下游 morpher 仓库的 `mjt-deobf` 需要稳定的 release contract：它的 detector / CFF report
以单行 JSON 输出，包含 `schema_version`、`before`/`after` 快照、`processed`/`failed`/
`unreadable_classes` 计数器、`unsupported_reasons` 数组等字段。目前只能靠 C++ 手写
`json.contains(key) && json.at(key).is_number_integer()` 断言，或 PowerShell 的
`ConvertFrom-Json` + 属性检查。这两种方式都有问题：

- C++ 手写断言散落在测试里，schema 演进时容易漏改，且无法复用给其他 JSON contract。
- PowerShell 侧校验与 C++ 单测分离，回归保护弱，跨平台也不一致。

一个 libca 内置的轻量 schema 校验器可以统一这两个场景：morpher 把 release contract 写成
一份 schema 文档（可以是硬编码的 `JsonValue`，也可以从文件加载），C++ 单测和 CLI 都调用
同一个 `validate(document, schema)` 接口。

libca/json 已有的 DOM（`JsonValue` 七类型 + `Utf8StringRef` + arena 架构）和
`ca::Result<T, E>` 错误模型为校验器提供了直接可用的数据基础。

## 建议落点

`libca/json`：

- 新增 `libca/json/src/libca/json/json_schema.hpp`：schema 数据模型 + 校验器接口声明。
- 新增 `libca/json/src/libca/json/json_schema.cpp`（或拆 `json_validator.cpp`）：校验实现。
- 新增 `libca/json/unittest/json_schema_test.cpp`：正反例测试。
- 更新 `libca/json/doc/json设计文档.md`：在校验器小节说明支持的 schema 关键字子集。
- `libca/json/xmake.lua`：把新源文件加入构建。

## 设计要点

1. **基于 JSON Schema draft 2020-12 的子集**。第一版只实现下游实际需要的关键字，
   不追求完整覆盖：
   - `type`：`"object" / "array" / "string" / "integer" / "number" / "boolean" / "null"`
     （`integer` 对应 `JsonType::Int`；`number` 接受 int 或 float）。
   - `required`：字符串数组，object 必须包含这些 key。
   - `properties`：object，每个 key 对应一个子 schema，校验对应字段（只校验出现的字段，
     不强制 `additionalProperties: false` 除非显式声明）。
   - `const`：文档值必须与此常量相等（用于 `schemaVersion` 这类固定值契约）。
   - `enum`：文档值必须等于列表中的某一项。
   - `items`：单个 schema，校验 array 的每个元素。
   - `minimum` / `maximum`：number/integer 的取值范围（闭区间）。
   - `minLength` / `maxLength`：string 长度范围。
   - `pattern`：string 的正则匹配（可选，依赖 libca 是否已有正则；若无则留到后续）。
   后续可按需扩展 `additionalProperties`、`minItems`/`maxItems`、`oneOf`/`anyOf`/`allOf`、
   `$ref` 等。
2. **schema 本身也是 `JsonValue`**。校验器接收 `const JsonValue& document` 和
   `const JsonValue& schema`，不引入新的 schema DSL 类型——schema 就是一份普通 JSON 文档，
   和 draft 2020-12 一致。这样 schema 可以硬编码在 C++ 里（用 `JsonDomBuilder` 构造），
   也可以从文件读。
3. **错误模型**。返回 `Result<std::vector<ValidationError>, SchemaError>`：
   - `ValidationError`：单条校验失败，含 `Utf8String instance_path`（文档内定位，如
     `"/before/classes"`）、`Utf8String schema_path`（schema 内定位）、
     `Utf8String message`（人读原因，如 `"expected integer, got string"`）。
   - `SchemaError`：schema 本身不合法（如 `type` 值不是已知字符串、`properties` 不是
     object），与文档校验失败区分开。
   - 默认收集**所有**失败（不遇到第一个就停），方便一次性报告 contract 的全部问题；
   - 提供 `validate_strict` 或参数控制是否 fail-fast。
4. **不抛异常**。与 json 模块整体风格一致，内部用 `try/catch` 仅在 std::regex 或
   `JsonValue` 访问可能抛的地方兜底，转成 `ValidationError`。
5. **API 草案**（具体签名留给实现）：
   ```cpp
   namespace ca::json {
   struct ValidationError {
       ca::str::Utf8String instance_path;
       ca::str::Utf8String schema_path;
       ca::str::Utf8String message;
   };
   /// 校验 document 是否符合 schema。返回所有失败项；schema 本身非法时返回 Err。
   Result<std::vector<ValidationError>, SchemaError> validate(
       const JsonValue& document, const JsonValue& schema);
   }
   ```
6. **测试正反例**。`json_schema_test.cpp` 至少覆盖：type 匹配/不匹配、required 缺失、
   properties 嵌套、const 固定值、enum、items 数组、minimum/maximum 边界、
   schema 自身非法（如 type 拼错）。

## 不在范围

- 不实现完整 JSON Schema draft 2020-12（`$ref`/`$id` 远程引用、条件 schema
  `if/then/else`、复杂组合 `allOf/anyOf/oneOf/not` 第一版可不做，留扩展点）。
- 不规定正则引擎来源（`pattern` 关键字是否第一版支持取决于 libca 是否已有正则模块；
  若无，第一版跳过 `pattern`，在文档标注）。
- 不集成 morpher 的具体 contract——morpher 升级 libca 后自行编写 schema 文档调用本模块。
- 不提供 schema 元 schema（校验 schema 本身是否符合 draft 2020-12）的第一版实现，
  只做关键字白名单检查。
