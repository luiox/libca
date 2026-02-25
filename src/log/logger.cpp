#include "logger.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>

namespace libca {
    constexpr std::array<std::string_view, 7> kLevelNames = {
    "Trace", "Debug", "Info", "Warn", "Error", "Critical", "Off"};


Level stringToLevel(std::string_view level)
{
    for (std::size_t index = 0; index < kLevelNames.size(); ++index) {
        if (std::equal(level.begin(), level.end(), kLevelNames[index].begin(), kLevelNames[index].end(),
                       [](char lhs, char rhs) {
                           return static_cast<unsigned char>(lhs) ==
                                  static_cast<unsigned char>(rhs) ||
                                  std::tolower(static_cast<unsigned char>(lhs)) ==
                                      std::tolower(static_cast<unsigned char>(rhs));
                       })) {
            return static_cast<Level>(index);
        }
    }
    return Level::Off;
}

std::string levelToString(Level level)
{
    const auto index = static_cast<std::size_t>(level);
    if (index < kLevelNames.size()) {
        return std::string(kLevelNames[index]);
    }
    return "Off";
}

namespace {
std::shared_ptr<Logger> g_logger;
}

Logger::Logger(std::shared_ptr<ILogBackend> backend)
	: backend_(std::move(backend))
	, levelPtr_(backend_ ? &backend_->get_level_atomic() : nullptr)
{}

bool Logger::should_log(Level level) const
{
	if (!levelPtr_) {
		return false;
	}
	return static_cast<int>(level) >=
		   static_cast<int>(levelPtr_->load(std::memory_order_relaxed));
}

ILogBackend* Logger::backend() const
{
	return backend_.get();
}

void Logger::set_level(Level level) const
{
	if (!backend_ || !levelPtr_) {
		return;
	}
	const_cast<std::atomic<Level>*>(levelPtr_)->store(level, std::memory_order_relaxed);
	backend_->set_level(level);
}

void set_global_logger(std::shared_ptr<ILogBackend> backend)
{
	if (!backend) {
		std::atomic_store(&g_logger, std::shared_ptr<Logger>{});
		return;
	}

	std::atomic_store(&g_logger, std::make_shared<Logger>(std::move(backend)));
}

std::shared_ptr<Logger> get_global_logger()
{
	return std::atomic_load(&g_logger);
}

}   // namespace libca