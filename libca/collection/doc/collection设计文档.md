---
version: 1.1
update:
2026-07-14 - 删除冗余英文摘要 design.md，本文成为 collection 唯一设计文档
2026-07-06 - 首版，补充 collection 模块职责与模板组件边界
---

# libca/collection 设计文档

## 定位

`libca_collection` 提供基础容器和容器处理辅助。当前 main 分支包含可变顺序容器
`ArrayList`、哈希映射容器 `HashMap`、哈希集合容器 `HashSet`、不可变列表 `ImmutableList` 和惰性容器处理 `Stream`，主要用于补充标准库在表达力上的小缺口。

collection 处于依赖分层 L1，原则上只依赖 core 或标准库。当前实现是模板为主，因此主要在
头文件中完成。

## 模块结构

- `array_list.hpp`：Rust-like 基础 API 的可变顺序容器。
- `hash_map.hpp`：Rust-like 基础 API 的哈希映射容器。
- `hash_set.hpp`：Rust-like 基础 API 的哈希集合容器。
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

## Java intrinsic runtime 容器方向

mj2x 这类 Java 到 native 翻译器会需要 `ArrayList`、`HashMap`、`HashSet` 等常见集合的 native 辅助能力，但 collection 模块不应直接承载 Java 对象布局、GC/lifetime 或异常策略。libca 只提供稳定 C++ 容器与算法，翻译器 runtime 再做 Java 语义包装。

容器命名可以比 STL 更贴近业务语义，例如后续新增 `ArrayList<T>`、`HashMap<K, V>` 这类入口；但 API 风格建议参考当前 `Bytes` 的 Rust-like 设计，而不是完整复刻 Java 标准库：

- 所有权清晰：拥有型容器、非拥有视图、共享只读数据要分开命名。
- 方法语义短而稳定：优先使用 `len()`、`is_empty()`、`reserve()`、`clear()`、`as_slice()` 等基础能力。
- 类型名可以使用 `ArrayList`、`HashMap`、`HashSet` 这类易读入口，但方法和函数统一使用 `snake_case`，不保留 CamelCase 双命名。
- 底层可以复用 STL 存储实现，但不要把 `std::vector` / `std::unordered_map` 的 API 直接暴露为 libca 的长期契约。

因此 collection 的演进顺序建议是：

1. `ArrayList<T>` 覆盖最小可变顺序容器能力，包括 `add/get/first/last/set/swap/resize/truncate/remove_at/swap_remove/len`。
2. `HashMap<K, V>` 覆盖最小哈希映射能力，包括 `put/get/get_or_default/remove/contains_key/contains_value/len`。
3. `HashSet<T>` 覆盖最小哈希集合能力，包括 `add/get/remove/take/replace/contains/len`。
4. 后续可继续补 entry API、只读视图和更明确的 optional/reference 返回策略。
5. 最后由 mj2x runtime 在外层适配 Java 的对象模型、越界异常、泛型擦除和标准库方法签名。
