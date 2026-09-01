#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// @file env.hpp
/// @brief 环境变量与操作系统信息查询。命名空间 `ca::env`。
/// @note 仅桌面端使用，嵌入式不需要。Windows 上内部做 UTF-8 ↔ UTF-16 转换，
///       保证含非 ASCII 的环境变量正确往返。所有函数不抛异常，失败返回空值/false。

namespace ca::env {

/// @brief 读取环境变量值。
/// @return 存在返回值，不存在返回空 optional。
/// @note Windows 上空值变量（设为空串）与不存在的变量无法区分，统一按不存在返回空。
std::optional<std::string> get(std::string_view name);

/// @brief 设置环境变量。value 为空串等价于设为空值变量（不是删除）。
/// @return 成功返回 true。
bool set(std::string_view name, std::string_view value);

/// @brief 删除环境变量。
/// @return 成功（包括变量本就不存在）返回 true。
bool remove(std::string_view name);

/// @brief 获取所有环境变量（key=value 对，未拆分）。
std::vector<std::pair<std::string, std::string>> all();

/// @brief 当前工作目录（UTF-8）。失败返回空串。
std::string current_dir();

/// @brief 设置当前工作目录。失败返回 false。
bool set_current_dir(std::string_view path);

/// @brief 临时目录路径（UTF-8），末尾不含分隔符。
std::string temp_dir();

/// @brief 当前可执行文件绝对路径（UTF-8）。失败返回空串。
std::string executable_path();

/// @brief 操作系统名："windows" / "linux" / "macos" / "unknown"。
std::string os_name();

/// @brief 操作系统版本字符串（如 "10.0.22631"）。失败返回空串。
std::string os_version();

}   // namespace ca::env
