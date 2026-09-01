// 屏幕捕获排除工具（基于 SetWindowDisplayAffinity）。
// `WDA_EXCLUDEFROMCAPTURE` 需要 Windows 10 version 2004+；旧版本会返回失败。

#pragma once

// API 可用性由实现文件按 _WIN32_WINNT 检查；头文件只声明。

#include "libca/core/status.hpp"

#include <string>

namespace ca::ui {

/// @brief 把所有类名匹配 `class_name` 的顶级窗口标记为不可捕获。
///
/// 通过 `EnumWindows` 枚举所有顶级窗口，对每个窗口 `GetClassNameA` 后比对，
/// 命中者调用 `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)`。
/// 常用于隐藏 D3D 中间窗口（`"IntermediateD3DWindowClass"`），避免被屏幕录像软件捕获。
///
/// @param class_name 目标窗口类名。默认 `"IntermediateD3DWindowClass"`。
/// @return 至少成功设置一个窗口返回 OK；未命中或 API 不可用返回 INTERNAL。
core::Status apply_capture_exclusion(const std::string& class_name = "IntermediateD3DWindowClass");

}   // namespace ca::ui
