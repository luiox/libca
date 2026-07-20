//
// @brief Win32 按钮控件实现
// @author Canrad
// @date 2026/07/20
//

#include "button.hpp"

#include <string>

namespace ca::ui {

Button::Button(Window* parent)
    : Control(parent)
{
}

Button::~Button()
{
    if (hwnd_ != nullptr)
        DestroyWindow(hwnd_);
}

core::Status Button::create()
{
    // 标题 UTF-8 → UTF-16，CreateWindowExW 要求宽字符。
    int wide_len = MultiByteToWideChar(CP_UTF8,
                                       0,
                                       text_.data(),
                                       static_cast<int>(text_.size()),
                                       nullptr,
                                       0);
    std::wstring wide_text(static_cast<usize>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        text_.data(),
                        static_cast<int>(text_.size()),
                        &wide_text[0],
                        wide_len);

    // 父窗口 HWND 通过 Window::native_handle() 取得。
    HWND parent_hwnd = (parent() != nullptr) ? parent()->native_handle() : nullptr;

    hwnd_ = CreateWindowExW(0,
                            L"BUTTON",
                            wide_text.c_str(),
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            x_, y_, width_, height_,
                            parent_hwnd,
                            nullptr,
                            (parent() != nullptr) ? reinterpret_cast<HINSTANCE>(
                                GetWindowLongPtr(parent_hwnd, GWLP_HINSTANCE))
                                                  : GetModuleHandle(nullptr),
                            nullptr);
    if (hwnd_ == nullptr) {
        const DWORD err = GetLastError();
        return core::ErrStatus(core::StatusCode::INTERNAL,
                               "Button CreateWindowExW failed with Windows error " +
                                   std::to_string(static_cast<unsigned long>(err)));
    }

    // 把 Button* 存到 GWLP_USERDATA，让 WindowProc 收到 BN_CLICKED 时能反查。
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return core::OkStatus();
}

void Button::dispatch_click()
{
    if (click_handler_) {
        ClickEvent event;
        click_handler_(event);
    }
}

}  // namespace ca::ui
