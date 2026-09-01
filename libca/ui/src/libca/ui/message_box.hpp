// Win32 MessageBox 包装。

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
/// - 构造实例 `MessageDialog dlg(title, message); dlg.show();`，便于先准备参数。
///
/// @note 不叫 MessageBox：windows.h 定义了 `#define MessageBox MessageBoxW`，
///       同名类会被宏静默改写，ANSI 构建或 #undef 后即破坏 ABI/编译。
class MessageDialog
{
public:
    /// @brief 构造，记录标题与内容。
    MessageDialog(std::string title, std::string message)
        : title_(std::move(title))
        , message_(std::move(message))
    {}

    /// @brief 弹出信息框（::MessageBoxW，MB_ICONINFORMATION）。
    /// @return 用户点击的按钮 ID（IDOK/IDCANCEL 等）；文本不是合法 UTF-8 时返回
    /// `INVALID_ARGUMENT`，系统调用失败时返回 `INTERNAL`。
    core::StatusResult<int> show() const;

    /// @brief 一次性弹出信息框，等价于 `MessageDialog(title, message).show()`。
    static core::StatusResult<int> info(const std::string& title, const std::string& message);

private:
    std::string title_;
    std::string message_;
};

}   // namespace ca::ui
