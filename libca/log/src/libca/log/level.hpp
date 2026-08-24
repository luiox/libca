#pragma once

#include <cstdint>
#include <string_view>

#include "libca/core/datatype.hpp"

/// @file level.hpp
/// @brief 日志级别枚举与字符串转换。命名空间 `ca::log`。

namespace ca::log {

/// @brief 日志级别，值越小越详细。
enum class Level : u8
{
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Warn     = 3,
    Error_   = 4,   // 尾下划线与 Windows wingdi.h 的全大写 ERROR 宏区隔（防御性命名；
                    // 预处理器大小写敏感，ERROR 不会匹配 Error，此处仅避免同名混淆）
    Critical = 5,
    Off      = 6
};

/// @brief 字符串转级别，大小写不敏感。无法识别时返回 Level::Off。
Level from_string(std::string_view name) noexcept;

/// @brief 级别转字符串字面量（"Trace".."Off"）。非法值返回 "Off"。
std::string_view to_string(Level level) noexcept;

}  // namespace ca::log
