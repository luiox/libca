# libca::ui

Windows-only 的极简 Win32 GUI 工具模块。

## 定位

提供基于 Win32 API 的窗口、按钮、消息框与"防截屏"工具，命名空间 `ca::ui`。
**仅在 Windows 平台编译**，其它平台 `xmake.lua` 不定义任何 target。

## 当前能力

- `Window` / `WindowManager`：HWND → `Window*` 调度，WNDCLASS 注册，消息循环
- `Control` / `Button`：builder 风格的子控件（仅 `Button`，后续可扩展）
- `MessageBox`：包装 Win32 `::MessageBoxW`
- `capture_guard`：通过 `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` 把
  目标窗口排除出屏幕捕获

## 不在范围

- 跨平台抽象（Qt / wxWidgets 风格）。本模块是 Win32 专用，不做 Linux/macOS 等价物。
- 完整控件库（编辑框、列表、菜单等）。需要时再增量补。
- DPI 感知、桌面 compositor 高级集成等高级特性。

## 历史背景

源自旧 `libca.core/src/platform/win/`（`WinUi.{hpp,cpp}`、`win_util.cpp`、`WinPlatform.hpp`）。
旧代码本身有多处 bug（`MessageBox::show` 误写成全局自由函数导致链接失败、`Window::show`
死循环、`register_class` 用未初始化的 `hInstance` 等），迁移时已全部修复。
