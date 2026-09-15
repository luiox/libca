#pragma once

#include <string_view>
#include <utility>

#include <fmt/core.h>

#include "libca/core/datatype.hpp"
#include "libca/log/detail/fmt_format.hpp"
#include "libca/log/level.hpp"
#include "libca/log/logger.hpp"
#include "libca/log/logger_registry.hpp"

/// @file log_macros.hpp
/// @brief 用户入口：日志宏 + 编译期/运行期过滤。命名空间 `ca::log`。
/// @note 用法：`#include "libca/log/log_macros.hpp"` 然后 `CA_LOG_INFO("hello {}", name)`。
///       编译期级别由 `CA_COMPILE_LOG_LEVEL` 宏控制（默认 Info），低于该级别的宏不生成调用。
/// @note 短路求值保证：编译期被裁剪的级别（if constexpr 整体丢弃）与运行期级别不满足
///       （未注册 target 或 should_log 为 false）时，格式化实参 **零求值**——实参中的
///       函数调用、临时对象构造等副作用均不会发生。运行期检查在宏内、实参求值之前完成。

namespace ca::log {

#ifndef CA_COMPILE_LOG_LEVEL
#define CA_COMPILE_LOG_LEVEL 2  // 默认 Info
#endif

inline constexpr Level kCompileTimeLevel = static_cast<Level>(CA_COMPILE_LOG_LEVEL);

/// @brief 该级别是否在编译期保留（低于 kCompileTimeLevel 的日志在编译期被裁剪）。
constexpr bool should_compile(Level level) noexcept
{
    return static_cast<int>(level) >= static_cast<int>(kCompileTimeLevel);
}

namespace detail {

/// @brief 门面分发入口：按已通过过滤的 Logger 直接调后端。
/// @note 级别过滤（编译期 + 运行期）已在宏内完成——这是"级别不满足时实参零求值"的
///       前提：若把运行期检查留到本函数内，实参早在进入函数前就被求值了。因此本函数
///       假定 logger 非 nullptr 且 should_log 已通过，直接分发。
///       FmtArgsHolder 是 view：实参来自调用方栈帧，在本函数同步调用 backend->log() 期间
///       存活，安全。严禁把 holder 异步入队（会悬空）。
template<typename... Args>
inline void log_with_source(Logger*              logger,
                            Level                level,
                            std::string_view     target,
                            std::string_view     file,
                            int                  line,
                            fmt::format_string<Args...> fmt_str,
                            Args&&... args)
{
    // make_format_args 要求实参为左值（fmt 10.x+ 禁止临时量，见 fmt issue #3589）。
    // 函数参数 args... 本身是左值，直接传入即可；不要 std::forward（那会得到右值引用）。
    // store 是本栈帧上的临时量，但在同一全表达式内同步调用 backend->log()，实参仍在栈上，
    // FmtArgsHolder 的 view 安全。
    auto             store = fmt::make_format_args(args...);
    FmtArgsHolder holder(fmt_str, store);
    logger->backend()->log(level, target, file, line, holder);
}

}  // namespace detail

}  // namespace ca::log

// ============================================================================
// 用户宏。CA_ 前缀避免污染用户命名空间。
// - CA_LOG_<LEVEL>(fmt, ...)       : target="default"
// - CA_LOGT_<LEVEL>(target, ...)   : 指定 target
// ============================================================================

#define CA_LOG_LEVEL(level, target, ...)                                                           \
    do {                                                                                           \
        if constexpr (::ca::log::should_compile(level)) {                                          \
            /* 运行期检查必须在实参求值前完成：get/should_log 通过后才展开 __VA_ARGS__，  */        \
            /* 级别不满足时实参零求值（短路求值）。target 只求值一次，暂存后两处使用。   */        \
            const std::string_view ca_log_target = (target);                                       \
            auto* ca_log_logger = ::ca::log::LoggerRegistry::get(ca_log_target);                   \
            if (ca_log_logger != nullptr && ca_log_logger->should_log(level)) {                    \
                ::ca::log::detail::log_with_source(                                                \
                    ca_log_logger, level, ca_log_target, __FILE__, __LINE__, __VA_ARGS__);         \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define CA_LOG_TRACE(...)    CA_LOG_LEVEL(::ca::log::Level::Trace, "default", __VA_ARGS__)
#define CA_LOG_DEBUG(...)    CA_LOG_LEVEL(::ca::log::Level::Debug, "default", __VA_ARGS__)
#define CA_LOG_INFO(...)     CA_LOG_LEVEL(::ca::log::Level::Info, "default", __VA_ARGS__)
#define CA_LOG_WARN(...)     CA_LOG_LEVEL(::ca::log::Level::Warn, "default", __VA_ARGS__)
#define CA_LOG_ERROR(...)    CA_LOG_LEVEL(::ca::log::Level::Error_, "default", __VA_ARGS__)
#define CA_LOG_CRITICAL(...) CA_LOG_LEVEL(::ca::log::Level::Critical, "default", __VA_ARGS__)

#define CA_LOGT_TRACE(target, ...)    CA_LOG_LEVEL(::ca::log::Level::Trace, target, __VA_ARGS__)
#define CA_LOGT_DEBUG(target, ...)    CA_LOG_LEVEL(::ca::log::Level::Debug, target, __VA_ARGS__)
#define CA_LOGT_INFO(target, ...)     CA_LOG_LEVEL(::ca::log::Level::Info, target, __VA_ARGS__)
#define CA_LOGT_WARN(target, ...)     CA_LOG_LEVEL(::ca::log::Level::Warn, target, __VA_ARGS__)
#define CA_LOGT_ERROR(target, ...)    CA_LOG_LEVEL(::ca::log::Level::Error_, target, __VA_ARGS__)
#define CA_LOGT_CRITICAL(target, ...) CA_LOG_LEVEL(::ca::log::Level::Critical, target, __VA_ARGS__)
