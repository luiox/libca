//
// @brief Win32 按钮控件实现
// @author Canrad
// @date 2026/07/20
//

#include "button.hpp"

#include "libca/str/charset.hpp"

#include <string>

namespace ca::ui {

Button::Button(Window* parent)
    : Control(parent)
{
}

Button::~Button()
{
    if (hwnd_ != nullptr && IsWindow(hwnd_))
        DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

core::Status Button::create()
{
    if (hwnd_ != nullptr)
        return core::ErrStatus(core::StatusCode::ALREADY_EXISTS,
                               "Button has already been created");

    HWND parent_hwnd = parent() != nullptr ? parent()->native_handle() : nullptr;
    if (parent_hwnd == nullptr || !IsWindow(parent_hwnd))
        return core::ErrStatus(core::StatusCode::FAILED_PRECONDITION,
                               "Button parent window has not been created");

    // 标题 UTF-8 → UTF-16，CreateWindowExW 要求宽字符。
    auto converted = str::CharsetConverter::utf8_to_wide(text_);
    if (converted.is_err())
        return std::move(converted).unwrap_err();
    auto wide_text = std::move(converted).unwrap();

    hwnd_ = CreateWindowExW(0,
                            L"BUTTON",
                            wide_text.c_str(),
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            x_, y_, width_, height_,
                            parent_hwnd,
                            nullptr,
                            reinterpret_cast<HINSTANCE>(
                                GetWindowLongPtrW(parent_hwnd, GWLP_HINSTANCE)),
                            nullptr);
    if (hwnd_ == nullptr) {
        const DWORD err = GetLastError();
        return core::ErrStatus(core::StatusCode::INTERNAL,
                               "Button CreateWindowExW failed with Windows error " +
                                   std::to_string(static_cast<unsigned long>(err)));
    }

    return core::OkStatus();
}

void Button::handle_command(WORD notification)
{
    if (notification == BN_CLICKED)
        dispatch_click();
}

void Button::dispatch_click()
{
    if (click_handler_) {
        ClickEvent event;
        click_handler_(event);
    }
}

}  // namespace ca::ui
