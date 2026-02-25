#include "spdlog_backend.hpp"
#include <array>
#include <iterator>

namespace libca {
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

constexpr spdlog::level::level_enum to_spdlog_level(Level level)
{
    const auto index = static_cast<std::size_t>(level);
    if (index >= kSpdlogLevelMap.size()) {
        return spdlog::level::off;
    }
    return kSpdlogLevelMap[index];
}
}   // namespace

SpdlogBackend::SpdlogBackend(std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger))
    , levelAtomic_(Level::Info)
{
    if (logger_) {
        levelAtomic_.store(static_cast<Level>(logger_->level()), std::memory_order_relaxed);
    }
}

void SpdlogBackend::log(Level level,
                        fmt::string_view,
                        fmt::string_view file,
                        int line,
                        fmt::string_view formatStr,
                        fmt::format_args args)
{
    if (!logger_) {
        return;
    }

    const auto spdLevel = to_spdlog_level(level);

    spdlog::memory_buf_t buffer;
    fmt::vformat_to(std::back_inserter(buffer), formatStr, args);

    logger_->log(spdlog::source_loc{file.data(), line, ""},
                 spdLevel,
                 fmt::string_view(buffer.data(), buffer.size()));
}

const std::atomic<Level>& SpdlogBackend::get_level_atomic() const
{
    return levelAtomic_;
}

void SpdlogBackend::set_level(Level level)
{
    levelAtomic_.store(level, std::memory_order_relaxed);
    if (logger_) {
        logger_->set_level(to_spdlog_level(level));
    }
}
}   // namespace libca
