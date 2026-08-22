# libca 提案目录

本目录记录**尚未实现**、但已被识别为有价值的功能提案。提案分两类：

- **旧代码迁移**：对应一个曾经在 `libca.core/`（已删除的旧桌面代码）中以空壳或不可用
  形式存在、需要在新 `libca/` 中按现代规范（`ca::*` 命名空间、`ca::core::Status` 错误
  处理、Doxygen 中文注释）重新设计的模块。
- **新增能力**：因下游需求（如 morpher）识别出的全新模块或现有模块的扩展，旧
  `libca.core` 中没有对应物。

提案不是承诺实现，只保留设计意图，避免设计上下文丢失。

| 提案 | 来源 | 类型 | 状态 |
|------|------|------|------|
| [timer.md](timer.md) | `libca.core/src/thread/Timer.{hpp,cpp}` | 旧代码迁移 | 待设计 |
| [event-bus.md](event-bus.md) | `libca.core/src/event/*` | 旧代码迁移 | 待设计 |
| [database.md](database.md) | `libca.core/src/database/*` | 旧代码迁移 | 待设计 |
| [zip.md](zip.md) | `libca.core/src/utility/Zip.{hpp,cpp}` | 旧代码迁移 | 待设计 |
| [tensor.md](tensor.md) | `libca.core/src/old/tensor.hpp` | 旧代码迁移 | 已弃用（仅留档） |
| [fs-unicode-path.md](fs-unicode-path.md) | morpher mjt-deobf 下游需求 | 新增能力 | 待设计 |
| [json-schema-validator.md](json-schema-validator.md) | morpher mjt-deobf 下游需求 | 新增能力 | 待设计 |
| [opt-v2.md](opt-v2.md) | morpher mjt / mj2x-cli 下游需求（三套 CLI 实现收敛） | 既有模块补齐 | 已实施（feat/opt-v2） |
