#include "libca/log/spdlog/spdlog_backend.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace ca::log {

namespace {

constexpr std::array<spdlog::level::level_enum, 7> kSpdlogLevelMap = {
    spdlog::level::trace,
    spdlog::level::debug,
    spdlog::level::info,
    spdlog::level::warn,
    spdlog::level::err,
    spdlog::level::critical,
    spdlog::level::off,
};

spdlog::level::level_enum to_spdlog_level(Level level)
{
    const auto index = static_cast<std::size_t>(level);
    if (index >= kSpdlogLevelMap.size())
        return spdlog::level::off;
    return kSpdlogLevelMap[index];
}

}  // namespace

SpdlogBackend::SpdlogBackend(std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger))
{}

void SpdlogBackend::log(Level              level,
                        std::string_view   /*target*/,
                        std::string_view   file,
                        int                line,
                        const OpaqueFormat& message)
{
    if (!logger_)
        return;

    // 先把门面侧格式化好的消息渲染到字符串，再交给 spdlog。
    // spdlog 不再做 fmt 格式化（消息已完整），直接按 source_loc 输出。
    std::string rendered;
    message.render_to(rendered);

    logger_->log(spdlog::source_loc{file.data(), line, ""},
                 to_spdlog_level(level),
                 rendered);
}

}  // namespace ca::log
