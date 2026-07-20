//
// @brief Win32 顶层窗口与窗口管理器实现
// @author Canrad
// @date 2026/07/20
//

#include "window.hpp"

#include "button.hpp"

#include <cstring>
#include <string>

namespace ca::ui {

namespace {

// 窗口类名固定，所有 Window 实例共享同一个 WNDCLASS。
// 用宽字符（WNDCLASSEXW + RegisterClassExW）避免在 NT 系统上被内部 A→W 转换。
constexpr const wchar_t* kWindowClassName = L"ca::ui::Window";

// 全局 WindowProc：把消息派发回对应 C++ Window 对象。WindowProc 是 Win32 注册类时
// 必须给出的 C 风格回调，无法直接捕获 this，所以走 WindowManager 单例查找。
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Window* window = WindowManager::get_instance().get_window(hwnd);
    if (window != nullptr) {
        // Button 等子控件的 WM_COMMAND 通知在此处理：HIWORD(wparam) 是通知码，
        // lParam 是控件 HWND。BN_CLICKED 时按控件 HWND 找 Button 派发点击。
        if (msg == WM_COMMAND && HIWORD(wparam) == BN_CLICKED) {
            HWND ctrl_hwnd = reinterpret_cast<HWND>(lparam);
            // 子控件由 Window 持有，遍历找匹配 HWND 的 Button。
            // 实际控件通过 add_control 登记；Button 的 dispatch_click 是 no-op
            // 如果未设置 handler，因此可以直接调用。
            // 注意：这里无法直接从 HWND 反查 Button*，因为 Button 不登记到 manager。
            // 为简单起见，Button 自己通过 GWLP_USERDATA 存指针（见 button.cpp）。
            if (ctrl_hwnd != nullptr) {
                auto* btn = reinterpret_cast<Button*>(GetWindowLongPtr(ctrl_hwnd, GWLP_USERDATA));
                if (btn != nullptr)
                    btn->dispatch_click();
            }
        }
        return window->handle_messages(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

Window::Window(std::string title, int x, int y, int width, int height)
    : title_(std::move(title))
    , x_(x)
    , y_(y)
    , width_(width)
    , height_(height)
{
}

Window::~Window()
{
    if (hwnd_ != nullptr)
        WindowManager::get_instance().remove_window(hwnd_);
}

core::Status Window::create(HINSTANCE instance)
{
    instance_ = instance;

    WNDCLASSEXW wcex{};
    wcex.cbSize        = sizeof(WNDCLASSEXW);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = window_proc;
    wcex.hInstance     = instance_;
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = kWindowClassName;
    // RegisterClassExW 对已注册的类返回 ERROR_CLASS_ALREADY_EXISTS，是正常幂等行为，
    // 失败时除了 "class already exists" 都需要当作真正的注册错误。
    if (RegisterClassExW(&wcex) == 0) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            return core::ErrStatus(core::StatusCode::INTERNAL,
                                   "RegisterClassExW failed with Windows error " +
                                       std::to_string(static_cast<unsigned long>(err)));
        }
    }

    // 标题 UTF-8 → UTF-16，CreateWindowExW 要求宽字符。
    int wide_len = MultiByteToWideChar(CP_UTF8,
                                       0,
                                       title_.data(),
                                       static_cast<int>(title_.size()),
                                       nullptr,
                                       0);
    std::wstring wide_title(static_cast<usize>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        title_.data(),
                        static_cast<int>(title_.size()),
                        &wide_title[0],
                        wide_len);

    hwnd_ = CreateWindowExW(0,
                            kWindowClassName,
                            wide_title.c_str(),
                            WS_OVERLAPPEDWINDOW,
                            x_, y_, width_, height_,
                            nullptr,
                            nullptr,
                            instance_,
                            nullptr);
    if (hwnd_ == nullptr) {
        const DWORD err = GetLastError();
        return core::ErrStatus(core::StatusCode::INTERNAL,
                               "CreateWindowExW failed with Windows error " +
                                   std::to_string(static_cast<unsigned long>(err)));
    }

    WindowManager::get_instance().add_window(hwnd_, this);
    return core::OkStatus();
}

void Window::show()
{
    if (hwnd_ == nullptr)
        return;
    ShowWindow(hwnd_, SW_SHOW);

    // GetMessage 阻塞获取消息，返回 FALSE 仅在收到 WM_QUIT 时；-1 表示错误。
    // 旧实现用 PeekMessage + while(true)，永不退出，已修。
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Window::hide()
{
    if (hwnd_ != nullptr)
        ShowWindow(hwnd_, SW_HIDE);
}

void Window::add_control(std::shared_ptr<Control> control)
{
    controls_.push_back(std::move(control));
}

LRESULT Window::handle_messages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // 默认消息处理。派生类重写后未处理的消息应转发回这里。
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

WindowManager& WindowManager::get_instance()
{
    static WindowManager instance;
    return instance;
}

void WindowManager::add_window(HWND hwnd, Window* window)
{
    windows_[hwnd] = window;
}

void WindowManager::remove_window(HWND hwnd)
{
    windows_.erase(hwnd);
}

Window* WindowManager::get_window(HWND hwnd) const
{
    auto it = windows_.find(hwnd);
    return it == windows_.end() ? nullptr : it->second;
}

}  // namespace ca::ui
