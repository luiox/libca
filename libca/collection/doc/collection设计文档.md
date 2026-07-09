---
version: 1.0
update:
2026-07-06 - 首版，补充 collection 模块职责与模板组件边界
---

# libca/collection 设计文档

## 定位

`libca_collection` 提供基础容器和容器处理辅助。当前 main 分支包含不可变列表
`ImmutableList` 和惰性容器处理 `Stream`，主要用于补充标准库在表达力上的小缺口。

collection 处于依赖分层 L1，原则上只依赖 core 或标准库。当前实现是模板为主，因此主要在
头文件中完成。

## 模块结构

- `immutable_list.hpp`：构造后不可修改的列表，支持范围 for、随机访问和追加生成新列表。
- `stream.hpp`：基于容器迭代器范围的惰性 `filter/map/forEach/collect`。
- `collection.hpp`：聚合头文件。

## 设计原则

collection 不追求替代 STL，而是提供语义更明确的薄工具：

- 标准库已有且足够好用的容器不重复实现。
- 新容器需要有明确语义收益，例如不可变、固定容量、视图、持久化结构等。
- 模板组件尽量保持 header-only，避免实例化和链接问题。
- 不把业务集合操作做成通用工具，避免模块失焦。

## 性能边界

`Stream` 当前以 `std::function` 保存 filter/map，优先表达简单而非极致零开销。性能敏感场景
可以直接使用 STL 算法或未来新增的模板化 pipeline。`ImmutableList::appended` 会复制原列表，
适合小列表和配置型数据，不适合大规模追加循环。

## 扩展方向

后续可以考虑：

- 更轻量的 view/range 风格工具。
- 小容量固定数组或 small-vector。
- 持久化 map/list 等不可变数据结构。

新增组件需要补独立单元测试，并在头文件 Doxygen 中说明复杂度和所有权语义。
