#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include "libca/log/logger.hpp"

/// @file simple_log_backend.hpp
/// @brief 零依赖同步日志后端：控制台（彩色）+ 可选文件输出。命名空间 `ca::log`。
/// @note 同步直写：调用线程在 mutex 保护下直接 fwrite。零外部依赖（不依赖 fmt/spdlog）。
///       高并发异步场景请用 with_spdlog 后端。

namespace ca::log {

/// @brief SimpleLogBackend 配置。
struct SimpleLogConfig
{
    /// 输出流目标。
    enum class Stream
    {
        Stdout,
        Stderr
    } stream{Stream::Stderr};

    /// 非空则同时追加写入该文件（UTF-8）。
    std::string file_path;

    /// 是否对终端输出着色（Windows 用 SetConsoleTextAttribute，POSIX 用 ANSI 转义）。
    bool color{true};

    /// 是否输出源文件:行号。
    bool show_location{true};

    /// 是否输出 target（模块名）。
    bool show_target{true};

    /// 时间戳格式（strftime 风格），留空则不输出时间。
    std::string time_format{"%H:%M:%S"};
};

/// @brief 零依赖同步日志后端。
///
/// 格式：`[time] [LEVEL] [target] message (file:line)`（各项可由 config 开关）。
/// 线程安全：内部持一把 mutex 串行化所有写操作，保证单条日志不被交错。
class SimpleLogBackend final : public ILogBackend
{
public:
    /// @brief 用默认配置构造（Stderr、彩色、显示 location/target/时间）。
    SimpleLogBackend();
    /// @brief 用指定配置构造。file_path 非空时会打开文件，失败则仅写终端（不抛）。
    explicit SimpleLogBackend(SimpleLogConfig config);

    void log(Level level, std::string_view target, std::string_view file, int line,
             const OpaqueFormat& message) override;

private:
    SimpleLogConfig config_;
    std::ofstream   file_;
    std::mutex      mutex_;   // 串行化本后端的写操作
};

}   // namespace ca::log
