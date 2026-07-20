/// @file control.hpp
/// @brief Win32 子控件基类。
/// @author Canrad
/// @date 2026/07/20

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

private:
    Window* parent_;
};

}  // namespace ca::ui
