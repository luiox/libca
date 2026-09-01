#pragma once

#include <memory>
#include <string>

#include <spdlog/logger.h>

#include "libca/log/logger.hpp"

/// @file spdlog_backend.hpp
/// @brief spdlog 适配后端。仅 `with_spdlog=y` 时编译。命名空间 `ca::log`。
/// @note 把新 ILogBackend 适配到底层 spdlog::logger。消息先由 OpaqueFormat::render_to
///       渲染成完整字符串，再喂给 spdlog（spdlog 不再做 fmt 格式化）。

namespace ca::log {

/// @brief 适配 spdlog::logger 的日志后端。
class SpdlogBackend final : public ILogBackend
{
public:
    /// @param logger 已配置好的 spdlog logger（pattern/level/sink 由调用方设定）。
    explicit SpdlogBackend(std::shared_ptr<spdlog::logger> logger);

    void log(Level level, std::string_view target, std::string_view file, int line,
             const OpaqueFormat& message) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

}   // namespace ca::log
