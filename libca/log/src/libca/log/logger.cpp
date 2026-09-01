#include "libca/log/logger.hpp"

#include <array>
#include <cctype>
#include <cstring>
#include <utility>

namespace ca::log {

namespace {

constexpr std::array<std::string_view, 7> kLevelNames = {
    "Trace", "Debug", "Info", "Warn", "Error", "Critical", "Off"};

bool iequals(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

}   // namespace

Level from_string(std::string_view name) noexcept
{
    for (std::size_t i = 0; i < kLevelNames.size(); ++i) {
        if (iequals(name, kLevelNames[i]))
            return static_cast<Level>(i);
    }
    return Level::Off;
}

std::string_view to_string(Level level) noexcept
{
    const auto index = static_cast<std::size_t>(level);
    if (index < kLevelNames.size())
        return kLevelNames[index];
    return "Off";
}

Logger::Logger(std::shared_ptr<ILogBackend> backend) noexcept
    : backend_(std::move(backend))
{}

bool Logger::should_log(Level lvl) const noexcept
{
    if (!backend_)
        return false;
    return static_cast<int>(lvl) >= static_cast<int>(level_.load(std::memory_order_relaxed));
}

void Logger::set_level(Level lvl) noexcept
{
    level_.store(lvl, std::memory_order_relaxed);
}

Level Logger::level() const noexcept
{
    return level_.load(std::memory_order_relaxed);
}

ILogBackend* Logger::backend() const noexcept
{
    return backend_.get();
}

}   // namespace ca::log
