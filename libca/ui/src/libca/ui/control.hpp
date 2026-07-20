// Win32 子控件基类。

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ca::ui {

class Window;

/// @brief Win32 子控件基类。持有父 `Window*`，由 `Window::add_control` 持有所有权。
///
/// 派生类（如 `Button`）负责创建并管理自己的 HWND。本基类只提供父窗口引用。
class Control
{
public:
    /// @brief 构造控件，记录父窗口。
    explicit Control(Window* parent) : parent_(parent) {}

    virtual ~Control() = default;

    Control(const Control&)            = delete;
    Control& operator=(const Control&) = delete;

    /// @brief 返回父窗口。
    Window* parent() const noexcept { return parent_; }

    /// @brief 返回底层 HWND；尚未创建时返回 nullptr。
    virtual HWND native_handle() const noexcept = 0;

    /// @brief 处理父窗口转发的 `WM_COMMAND` 通知。
    /// @param notification `HIWORD(wParam)` 中的控件通知码。
    virtual void handle_command(WORD notification) = 0;

private:
    Window* parent_;
};

}  // namespace ca::ui
