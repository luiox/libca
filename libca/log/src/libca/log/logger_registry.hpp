#pragma once

#include <memory>
#include <string_view>

#include "libca/log/logger.hpp"

/// @file logger_registry.hpp
/// @brief 按 target（模块名）分发 Logger。命名空间 `ca::log`。
/// @note 用 LoggerRegistry 取代旧设计的单一全局 logger 指针，从架构层面根治
///       set_global_logger 的竞态：不再有单一裸指针被并发读写，每个 target 的 Logger
///       由 shared_ptr 引用计数保护，注册/查找经 shared_mutex 串行化。

namespace ca::log {

/// @brief 全局 Logger 注册表（按 target 名索引）。
///
/// 读取（get）用共享锁，多读并发；写入（register/unregister）用排他锁。get 返回的
/// Logger* 在同一次 log 调用栈帧内有效：被替换/注销的旧 Logger 进入内部退休列表
/// 保活（进程生命周期），不会在 backend->log() 同步执行期间被析构。
class LoggerRegistry
{
public:
    LoggerRegistry() = delete;

    /// @brief 注册或替换某 target 的 Logger。
    /// @param target 模块名（如 "default"、"net"、"fs"）。
    /// @param logger 要绑定的 Logger；为空等价于注销该 target。
    static void register_logger(std::string_view target, std::shared_ptr<Logger> logger);

    /// @brief 查找某 target 的 Logger。
    /// @return 找到返回 Logger*（运行期有效，调用方在一次 log 表达式内使用安全）；
    ///         未注册返回 nullptr。
    static Logger* get(std::string_view target) noexcept;

    /// @brief 注销某 target。
    /// @return 注销前存在返回 true。
    static bool unregister_logger(std::string_view target) noexcept;

    /// @brief 注销所有 target（主要用于测试隔离）。
    static void clear() noexcept;
};

}   // namespace ca::log
