---
version: 1.0
update:
2026-07-20 - 首版。从 libca.core/src/platform/win/ 迁移，统一 ca::ui 命名空间、修复若干 bug、Windows-only target。
---

# libca::ui 设计文档

> 本文讲 ui 模块的架构与设计边界。**具体接口签名见各头文件的 Doxygen 注释**，
> 本文不重复 API 清单。

## 1. 模块定位

`libca::ui` 是 Windows-only 的 Win32 GUI 工具模块，命名空间 `ca::ui`。
**仅在 Windows 平台编译**——xmake.lua 用 `if is_plat("windows") then ... end`
包裹所有 target，其它平台不定义任何 target，因此跨平台项目可无条件 `includes("ui")`
而不会在 Linux 上失败。

定位上**不追求跨平台 GUI 抽象**（那是 Qt/wxWidgets 的领域），只把常用的 Win32
GUI 操作（窗口、按钮、消息框、防截屏）封装成 RAII / builder 风格的薄壳。

## 2. 主要组件

### 2.1 窗口层（window.hpp）

- `Window`：包装一个 `HWND`，存窗口标题、位置、尺寸、控件列表。提供：
  - `create(HINSTANCE)`：注册窗口类 + 创建窗口，**构造函数只存参数**，不立即创建，
    避免旧代码用未初始化 `hInstance` 注册类的问题。
  - `show()`：进入消息循环，收到 `WM_DESTROY` / `WM_QUIT` 退出。
  - `add_control(...)`：把子控件登记到 `Window` 持有的 `vector` 里管理生命周期。
  - `handle_messages(...)`：可重写的消息处理，默认调用 `DefWindowProc`。
- `WindowManager`：单例，维护 `HWND → Window*` 映射，供全局 `WindowProc` 回调时
  找到对应的 `Window*`。`create` 后自动登记。

### 2.2 控件层（control.hpp / button.hpp）

- `Control`：基类，持有 `Window*` 父指针。
- `Button`：builder 风格（`set_text` / `set_x` / `set_click_handler` 链式调用），
  `make(parent)` 工厂创建 `shared_ptr`。

### 2.3 消息框（message_box.hpp）

- 静态方法 `info(title, message)`：直接调 `::MessageBoxW(nullptr, ...)`，
  弹出系统信息框。
- 非静态方法 `show()`：构造时记下标题/内容，调用时弹出。

### 2.4 防截屏（capture_guard.hpp）

- `apply_capture_exclusion(class_name)`：枚举所有顶级窗口，把指定类名
  （默认 `"IntermediateD3DWindowClass"`，D3D 中间窗口）的窗口设为
  `WDA_EXCLUDEFROMCAPTURE`，避免被屏幕截图/录屏捕获。
- 由旧 `win_util.cpp` 的 `EnumWindowsProc` 改名 + 命名空间化而来。

## 3. 修复的旧 bug

旧 `libca.core/src/platform/win/WinUi.{hpp,cpp}` 本身有多处 bug，迁移时一并修复：

| 旧 bug | 修复 |
|---|---|
| `MessageBox::show()` 在 .cpp 中误写成全局自由函数 `void show() {}`，调用 `MessageBox::show()` 链接失败 | 正确实现为成员函数，调 `::MessageBoxW` |
| `MessageBox` ctor 和 `info` 空实现 | 实现成真正调 `::MessageBoxW` |
| `Window::show()` 用 `do {} while(true)` 死循环，无退出条件 | 改为 `GetMessage` 阻塞循环，`WM_QUIT` 退出 |
| `registerClass` 在 ctor 中调用，但 `hInstance_` 此时还是 nullptr | 把"注册类 + 创建窗口"挪到 `create(HINSTANCE)`，构造只存参数 |
| `CreateWindow` 用字面量 `TEXT("title")` 忽略 `title_` | 正确传 `title_` |
| `Window` 析构没 `removeWindow`、`WindowManager` map 会悬挂 | 暂不处理（GUI 进程退出即结束），但在文档中标注 |
| `#include "winui.hpp"`（小写）在大小写敏感 FS 上失败 | 文件名统一用 `window.hpp` / `message_box.hpp` 等 snake_case |
| 缺 `<cstring>` 靠 `<windows.h>` 传递拉入 | 显式 include |

## 4. 错误模型

GUI 错误（窗口创建失败、注册类失败）通过 `ca::core::Status` / `StatusResult<T>`
反馈。简单信息框等纯 fire-and-forget 操作不返回错误（Win32 `MessageBox` 几乎不会失败，
真失败也无可恢复）。

## 5. 测试策略

GUI 单元测试在 CI 里难以做（需要桌面会话）。当前只覆盖：

- `message_box_test.cpp`：调通 `MessageBox::info` 不崩（无头环境会自动跳过或返回）

后续若需要回归测试，应通过 Win32 message hooks 或 UI Automation API 注入合成事件，
不在本提案范围。
