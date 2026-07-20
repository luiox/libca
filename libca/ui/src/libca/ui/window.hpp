/// @file window.hpp
/// @brief Win32 顶层窗口与窗口管理器。
/// @author Canrad
/// @date 2026/07/20
/// @note 仅 Windows 平台可用。其它平台包含本头文件会因为缺少 `<windows.h>` 失败，
///       因此调用方需要用 `#if defined(_WIN32)` 守卫包含语句。

#pragma once

// Win32 头文件包含必须放在 STL 之前，避免 windows.h 的 min/max 宏污染 STL 模板。
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "libca/core/status.hpp"

namespace ca::ui {

class Control;

/// @brief 顶层 Win32 窗口。包装 HWND，提供窗口类注册、消息循环与子控件生命周期管理。
///
/// 构造函数只存参数，**不立即创建窗口**——必须在主线程拿到 HINSTANCE 后显式调
/// `create()` 完成窗口类注册与窗口创建。这样避免了旧实现中"构造时 hInstance 还是
/// nullptr 却调 RegisterClassEx"的初始化顺序 bug。
///
/// `Window` 不允许拷贝（持有 HWND 与控件所有权），允许移动。
class Window
{
public:
    /// @brief 构造一个未创建的窗口，记录标题与初始位置/尺寸。
    Window(std::string title, int x, int y, int width, int height);

    /// @brief 析构时从 WindowManager 移除登记。
    virtual ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&)                 = delete;
    Window& operator=(Window&&)      = delete;

    /// @brief 注册窗口类并创建 HWND，登记到 WindowManager。
    /// @param instance 当前进程的 HINSTANCE（通常来自 wWinMain 的参数）。
    /// @return 创建成功返回 OK，失败返回 INTERNAL 并附 GetLastError 信息。
    core::Status create(HINSTANCE instance);

    /// @brief 显示窗口并进入阻塞消息循环，直到收到 WM_QUIT 才返回。
    /// @note 必须在 `create()` 成功之后调用。消息循环会从调用线程的队列派发消息，
    ///       因此必须在创建窗口的同一线程调用。
    void show();

    /// @brief 隐藏窗口（不退出消息循环）。
    void hide();

    /// @brief 返回底层 HWND；未 `create()` 前为 nullptr。
    HWND native_handle() const noexcept { return hwnd_; }

    /// @brief 把子控件交给窗口持有（共享所有权）。
    void add_control(std::shared_ptr<Control> control);

    /// @brief 可重写的消息处理；默认调用 `DefWindowProc`。
    /// @note 派生类重写时未处理的消息应转发回本基类实现，避免消息被静默吞掉。
    virtual LRESULT handle_messages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
    std::string                          title_;
    int                                  x_;
    int                                  y_;
    int                                  width_;
    int                                  height_;
    HWND                                 hwnd_{nullptr};
    HINSTANCE                            instance_{nullptr};
    std::vector<std::shared_ptr<Control>> controls_;
};

/// @brief HWND → Window* 的全局映射，供 `WindowProc` 回调定位 C++ 对象。
///
/// 单例（`get_instance()`），线程不安全——必须在创建所有窗口的同一线程访问。
class WindowManager
{
public:
    /// @brief 获取单例。
    static WindowManager& get_instance();

    /// @brief 登记一个 HWND 与 Window* 的对应关系。
    void add_window(HWND hwnd, Window* window);

    /// @brief 移除登记。
    void remove_window(HWND hwnd);

    /// @brief 查找 HWND 对应的 Window*；未登记返回 nullptr。
    Window* get_window(HWND hwnd) const;

private:
    WindowManager() = default;

    std::map<HWND, Window*> windows_;
};

}  // namespace ca::ui
