#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "libca/core/datatype.hpp"

namespace ca::process {

/// @brief 子进程启动与 I/O 配置。字符串均按 UTF-8 解释。
struct SubprocessOptions
{
    std::string                              executable;
    std::vector<std::string>                 args;
    bool                                     capture_stdout{true};
    bool                                     capture_stderr{true};
    std::optional<std::string>               stdin_data;
    std::optional<std::chrono::milliseconds> timeout;
    std::optional<std::string>               working_dir;
};

/// @brief 子进程执行结果。输出内容为未转换的原始字节。
struct SubprocessResult
{
    i32         exit_code{-1};
    std::string stdout_data;
    std::string stderr_data;
    bool        timed_out{false};

    /// @brief 子进程是否以 0 正常退出。
    bool succeeded() const noexcept;
};

/// @brief 执行一个外部程序，不经过 shell。
/// @note 启动失败或超时时 exit_code 为 -1；超时还会设置 timed_out。
SubprocessResult run(const SubprocessOptions& options);

}   // namespace ca::process
