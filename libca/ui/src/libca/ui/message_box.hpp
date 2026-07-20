/// @file message_box.hpp
/// @brief Win32 MessageBox 包装。
/// @author Canrad
/// @date 2026/07/20

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>

#include "libca/core/status.hpp"

namespace ca::ui {

/// @brief Win32 `::MessageBoxW` 的薄包装。
///
/// 提供两种用法：
/// - 静态 `info(title, message)` 直接弹信息框；
/// - 构造实例 `MessageBox mb(title, message); mb.show();`，便于先准备参数。
class MessageBox
{
public:
    /// @brief 构造，记录标题与内容。
    MessageBox(std::string title, std::string message)
        : title_(std::move(title)), message_(std::move(message)) {}

    /// @brief 弹出信息框（::MessageBoxW，MB_ICONINFORMATION）。
    /// @return 用户点击的按钮 ID（IDOK/IDCANCEL 等），失败返回 INTERNAL Status。
    core::StatusResult<int> show() const;

    /// @brief 一次性弹出信息框，等价于 `MessageBox(title, message).show()`。
    static core::StatusResult<int> info(const std::string& title, const std::string& message);

private:
    std::string title_;
    std::string message_;
};

}  // namespace ca::ui
