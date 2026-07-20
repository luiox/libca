# 提案：Zip / 压缩

## 背景

旧 `libca.core/src/utility/Zip.{hpp,cpp}` 不可用：

- `Zip.hpp` 中 ~85% 是注释掉的死代码（minizip / Info-ZIP 风格的 `zipFile`、
  `zipOpenNewFileInZip`、`Z_DEFLATED` 符号），实际 `#include` 的却是另一套
  Windows-only 的 `zip_utils/zip.h`（Lucian Wischik 的 Zip Utils，依赖 `<windows.h>`）
- `ZipEntry` 空类
- `ZipArchive::createZip` 是唯一被定义的方法，调用 `CreateZip(filename_.c_str(), 0)`
- `putNextEntry` / `closeEntry` 声明但**未定义**，调用会链接失败
- 无 RAII 析构 close
- `Zip.cpp` 仅 14 行，且 namespace 写错（声明在 `ca`，但 `.cpp` 又 `#include "Zip.hpp"`）

旧代码不可用，已随 `libca.core/` 删除。本提案记录未来在新 `libca/` 中实现压缩的设计意图。

## 建议落点

`libca/utility`（已存在，目前只有 `BitsUitl`），或新建 `libca/archive`。
命名空间 `ca::utility` 或 `ca::archive`。

## 设计要点

1. **后端选型**（旧代码用 Windows-only `zip_utils`，新库需要跨平台）：
   - 选项 A：`add_requires("zlib")` + `add_requires("minizip")`，行业标准
   - 选项 B：纯 C++ 实现（无外部依赖，但工作量大）
   - 建议选项 A，并通过 `with_zip` / `with_archive` option 做成可选模块
2. **基于新库原语**：
   - 错误用 `ca::core::Status` / `StatusResult<>` 反馈
   - 与 `libca/io`（Reader/Writer 抽象）结合，支持流式压缩/解压
3. **核心抽象**：
   - `ZipArchive`：RAII，构造打开，析构自动 close，移动语义
   - `create(path) -> StatusResult<ZipArchive>`
   - `add_entry(name, reader) -> Status` / `extract_entry(name, writer) -> Status`
   - `entries() const -> std::vector<ZipEntry>`（列出条目）
   - `close() -> Status`（显式关闭，可拿到关闭错误）
4. **修复旧代码问题**：
   - 删除所有注释死代码
   - 统一 namespace（`ca::utility` 或 `ca::archive`）
   - RAII，避免 handle 泄漏
   - 跨平台，不依赖 `<windows.h>`

## 不在范围

- 本提案不规定精确 API 签名，留给具体设计阶段决定。
- 是否需要 gzip / tar / 7z 等其他格式待讨论。
