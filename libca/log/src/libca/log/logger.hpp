#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "libca/core/datatype.hpp"
#include "libca/log/level.hpp"

/// @file logger.hpp
/// @brief 日志门面：Logger 句柄、后端接口、类型擦除的格式化参数载体。
/// @note 后端实现者只需 include 本文件与 level.hpp，**不需要依赖 fmt**。
///       门面侧（detail/fmt_format.hpp）才依赖 fmt，负责构造 OpaqueFormat。
///       命名空间 `ca::log`。

namespace ca::log {

class Logger;

/// @brief 类型擦除的格式化参数载体（view 语义）。
///
/// 由门面侧（detail::FmtArgsHolder）构造，后端只读。后端调 `render_to(out)` 即可
/// 拿到格式化完成的字符串，无需知道 fmt 的存在。
///
/// @warning 本对象是 **view**：生命周期不得超过单次 log 调用。实参来自调用方栈帧，
///          同步调用 `backend->log()` 安全；**严禁跨线程入队**（会悬空）。异步需求须由
///          调用方在同步路径内先 `render_to` 成 std::string 再入队。
class OpaqueFormat
{
public:
    virtual ~OpaqueFormat() = default;

    /// @brief 渲染格式化结果到 out（追加，不清空）。后端实现时调此拿完整消息字符串。
    virtual void render_to(std::string& out) const = 0;
};

/// @brief 日志后端接口。后端只负责输出，级别管理由 Logger 负责。
///
/// 新接口与 fmt 完全解耦：参数经 OpaqueFormat 传入，后端调 `render_to` 取字符串即可。
class ILogBackend
{
public:
    virtual ~ILogBackend() = default;

    /// @brief 输出一条日志。
    /// @param level   级别（已通过 Logger 运行期过滤，后端无需再判级）。
    /// @param target  日志目标/模块名（如 "net"、"fs"）。
    /// @param file    源文件名（__FILE__）。
    /// @param line    源文件行号（__LINE__）。
    /// @param message 格式化参数载体；调 message.render_to(buf) 取完整字符串。
    virtual void log(Level level, std::string_view target, std::string_view file, int line,
                     const OpaqueFormat& message) = 0;
};

/// @brief 日志句柄，持有后端并管理级别。
///
/// 级别管理收归此处（旧设计把 set_level/get_level_atomic 委派给后端，导致每个后端
/// 重复实现）。Logger 持一个 atomic<Level> 做运行期过滤。
class Logger
{
public:
    /// @brief 构造，绑定后端。backend 为空时 should_log 恒为 false。
    explicit Logger(std::shared_ptr<ILogBackend> backend) noexcept;

    /// @brief 当前级别下是否应输出该级别日志。
    bool should_log(Level level) const noexcept;

    /// @brief 设置运行期级别（原子，线程安全）。
    void set_level(Level level) noexcept;

    /// @brief 当前运行期级别。
    Level level() const noexcept;

    /// @brief 底层后端指针（可能为 nullptr）。
    ILogBackend* backend() const noexcept;

private:
    std::shared_ptr<ILogBackend> backend_;
    std::atomic<Level>           level_{Level::Info};
};

}   // namespace ca::log
