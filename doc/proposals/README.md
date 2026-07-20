# libca 提案目录

本目录记录**尚未实现**、但已被识别为有价值的功能提案。每份提案对应一个曾经在
`libca.core/`（已删除的旧桌面代码）中以空壳或不可用形式存在、需要在新 `libca/` 中
按现代规范（`ca::*` 命名空间、`ca::core::Status` 错误处理、Doxygen 中文注释）重新设计的模块。

提案不是承诺实现，只保留设计意图，避免旧代码删除后丢失上下文。

| 提案 | 旧位置 | 状态 |
|------|--------|------|
| [timer.md](timer.md) | `libca.core/src/thread/Timer.{hpp,cpp}` | 待设计 |
| [event-bus.md](event-bus.md) | `libca.core/src/event/*` | 待设计 |
| [database.md](database.md) | `libca.core/src/database/*` | 待设计 |
| [zip.md](zip.md) | `libca.core/src/utility/Zip.{hpp,cpp}` | 待设计 |
| [tensor.md](tensor.md) | `libca.core/src/old/tensor.hpp` | 已弃用（仅留档） |
