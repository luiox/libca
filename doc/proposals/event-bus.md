# 提案：Event / EventBus（事件总线）

## 背景

旧 `libca.core/src/event/` 是无法编译的脚手架：

- `Event.hpp` 定义了 `EventPriority` 枚举和抽象 `Event` 类，以及三个空类
  （`EventListner` 拼写错误，应为 `Listener`；`EventDispatcher`、`EventManager`）
- `Event.cpp` 只有 `#include "Event.hpp"`，`Event` 的构造/析构/`GetName`/`GetPriority`
  **声明但未定义**，链接时会报 undefined reference
- `EventBus.hpp` 定义 `IEventBus` 抽象接口（`subscribe` / `post`），但
  - **类定义末尾缺少分号**（`}` 后没 `;`），无法编译
  - **不在任何命名空间**（与 `Event.hpp` 的 `namespace libca` 不一致）
  - `post(Event&)` 用的 `Event` 名字在全局命名空间下解析不到
- `EventBus.cpp` 只有 `#include "EventBus.hpp"`
- 没有任何具体的 `EventBus` 实现类

旧代码不可用，已随 `libca.core/` 删除。本提案记录未来在新 `libca/` 中实现事件总线的设计意图。

## 建议落点

新建 `libca/event` 模块，命名空间 `ca::event`。

## 设计要点

1. **基于新库原语**：
   - 用 `ca::thread::Thread` / `ca::thread::StopToken` 实现可停止的事件分发线程
   - 用 `ca::core::Status` 反馈订阅/发布错误
2. **核心抽象**：
   - `Event` 基类（携带名称、优先级、时间戳），具体事件类型由用户派生
   - `EventPriority` 枚举（按数值排序，便于优先级队列调度）
   - `EventBus` 接口：`subscribe(event_type, handler)` / `post(event)` /
     `unsubscribe(handle)`
   - 同步分发实现（`SynchronousEventBus`）与异步分发实现（`AsyncEventBus`，后台线程）
3. **修复旧代码问题**：
   - 修正命名空间一致性（统一 `ca::event`）
   - 修正 `EventListner` 拼写（→ `Listener`，或干脆删除空类）
   - 提供真正的具体实现，不仅是接口
4. **命名**：API 使用 snake_case，例如 `subscribe` / `post` / `dispatch` /
   `event_type` / `handler`。

## 不在范围

- 本提案不规定精确 API 签名，留给具体设计阶段决定。
- 是否需要泛型 `EventBus<T>`（按事件类型参数化）待讨论。
