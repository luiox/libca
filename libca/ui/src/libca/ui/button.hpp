/// @file button.hpp
/// @brief Win32 按钮控件，builder 风格 API。
/// @author Canrad
/// @date 2026/07/20

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <functional>
#include <memory>
#include <string>

#include "control.hpp"
#include "libca/core/status.hpp"
#include "window.hpp"

namespace ca::ui {

/// @brief 点击事件参数。当前为空，预留给后续扩展（坐标、修饰键等）。
struct ClickEvent
{};

/// @brief Win32 按钮控件（`WC_BUTTON`）。builder 风格：链式 set_xxx 后调 create() 落地。
///
/// 用法：
/// @code
/// auto btn = ca::ui::Button::make(window);
/// btn->set_text("OK")->set_x(10)->set_y(10)->create();
/// btn->set_click_handler([](ca::ui::ClickEvent&) { /* ... */ });
/// @endcode
class Button : public Control
{
public:
    /// @brief 构造按钮，记录父窗口与默认几何参数。
    explicit Button(Window* parent);

    /// @brief 析构时销毁底层 HWND。
    ~Button() override;

    Button(const Button&)            = delete;
    Button& operator=(const Button&) = delete;

    /// @brief 工厂：返回 `shared_ptr<Button>`，便于 `Window::add_control` 持有。
    static std::shared_ptr<Button> make(Window* parent) {
        return std::make_shared<Button>(parent);
    }

    /// @brief 设置按钮文本。必须在 `create()` 之前调用。
    Button& set_text(std::string text) { text_ = std::move(text); return *this; }
    /// @brief 设置 x 坐标（父窗口客户区相对）。
    Button& set_x(int x) { x_ = x; return *this; }
    /// @brief 设置 y 坐标（父窗口客户区相对）。
    Button& set_y(int y) { y_ = y; return *this; }
    /// @brief 设置宽度（像素）。
    Button& set_width(int width) { width_ = width; return *this; }
    /// @brief 设置高度（像素）。
    Button& set_height(int height) { height_ = height; return *this; }
    /// @brief 设置点击回调。
    Button& set_click_handler(std::function<void(ClickEvent&)> handler) {
        click_handler_ = std::move(handler);
        return *this;
    }

    /// @brief 用当前配置创建按钮 HWND。
    /// @note 必须在父窗口 `create()` 之后调用。
    core::Status create();

    /// @brief 返回按钮 HWND；未 create() 前为 nullptr。
    HWND native_handle() const noexcept { return hwnd_; }

    /// @brief 派发点击事件给已注册的回调。由全局 WindowProc 在收到 BN_CLICKED 时调用。
    void dispatch_click();

private:
    std::string                            text_{"Button"};
    int                                    x_{0};
    int                                    y_{0};
    int                                    width_{80};
    int                                    height_{24};
    HWND                                   hwnd_{nullptr};
    std::function<void(ClickEvent&)>       click_handler_;
};

}  // namespace ca::ui
