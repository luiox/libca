//
// @brief Win32 顶层窗口与窗口管理器实现
// @author Canrad
// @date 2026/07/20
//

#include "window.hpp"

#include "control.hpp"
#include "libca/str/charset.hpp"

#include <cstring>
#include <string>

namespace ca::ui {

namespace {

// 窗口类名固定，所有 Window 实例共享同一个 WNDCLASS。
// 用宽字符（WNDCLASSEXW + RegisterClassExW）避免在 NT 系统上被内部 A→W 转换。
constexpr const wchar_t* kWindowClassName = L"ca::ui::Window";

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
    controls_.clear();
    if (hwnd_ != nullptr) {
        const HWND hwnd = hwnd_;
        if (!DestroyWindow(hwnd)) {
            hwnd_ = nullptr;
            WindowManager::get_instance().remove_window(hwnd);
        }
    }
}

LRESULT CALLBACK Window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    auto&   manager = WindowManager::get_instance();
    Window* window  = manager.get_window(hwnd);
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window       = static_cast<Window*>(create->lpCreateParams);
        if (window != nullptr) {
            window->hwnd_ = hwnd;
            manager.add_window(hwnd, window);
        }
    }
    if (window == nullptr)
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    const LRESULT result = window->handle_messages(hwnd, msg, wparam, lparam);
    if (msg == WM_NCDESTROY) {
        manager.remove_window(hwnd);
        if (window->hwnd_ == hwnd)
            window->hwnd_ = nullptr;
    }
    return result;
}

core::Status Window::create(HINSTANCE instance)
{
    if (hwnd_ != nullptr)
        return core::ErrStatus(core::StatusCode::ALREADY_EXISTS,
                               "Window has already been created");
    if (instance == nullptr)
        return core::ErrStatus(core::StatusCode::INVALID_ARGUMENT,
                               "Window HINSTANCE must not be null");

    auto converted = str::CharsetConverter::utf8_to_wide(title_);
    if (converted.is_err())
        return std::move(converted).unwrap_err();
    auto wide_title = std::move(converted).unwrap();

    instance_ = instance;

    WNDCLASSEXW wcex{};
    wcex.cbSize        = sizeof(WNDCLASSEXW);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = Window::window_proc;
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

    hwnd_ = CreateWindowExW(0,
                            kWindowClassName,
                            wide_title.c_str(),
                            WS_OVERLAPPEDWINDOW,
                            x_, y_, width_, height_,
                            nullptr,
                            nullptr,
                            instance_,
                            this);
    if (hwnd_ == nullptr) {
        const DWORD err = GetLastError();
        return core::ErrStatus(core::StatusCode::INTERNAL,
                               "CreateWindowExW failed with Windows error " +
                                   std::to_string(static_cast<unsigned long>(err)));
    }
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
    if (control != nullptr)
        controls_.push_back(std::move(control));
}

LRESULT Window::handle_messages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_COMMAND) {
        const HWND control_hwnd = reinterpret_cast<HWND>(lparam);
        for (const auto& control : controls_) {
            if (control->native_handle() == control_hwnd) {
                control->handle_command(HIWORD(wparam));
                return 0;
            }
        }
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
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
