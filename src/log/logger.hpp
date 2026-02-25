#ifndef LIBCA_LOG_LOGGER_H
#define LIBCA_LOG_LOGGER_H

#include <atomic>
#include <memory>
#include <utility>
#include <cstdint>
#include <string>
#include <string_view>

#include <fmt/core.h>

#ifndef COMPILE_LOG_LEVEL
#define COMPILE_LOG_LEVEL 2
#endif

namespace libca {

// 日志等级
enum class Level : uint8_t
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

// 从字符串转换为日志级别
Level stringToLevel(std::string_view level);

// 将日志级别转换为字符串
std::string levelToString(Level level);

inline constexpr Level kCompileTimeLevel = static_cast<Level>(COMPILE_LOG_LEVEL);

constexpr bool should_compile(Level level)
{
	return static_cast<int>(level) >= static_cast<int>(kCompileTimeLevel);
}

class ILogBackend
{
public:
	virtual ~ILogBackend() = default;

	virtual void log(Level level,
					 fmt::string_view target,
					 fmt::string_view file,
					 int line,
					 fmt::string_view formatStr,
					 fmt::format_args args) = 0;

	virtual const std::atomic<Level>& get_level_atomic() const = 0;
	virtual void set_level(Level level)                         = 0;
};

class Logger final
{
public:
	explicit Logger(std::shared_ptr<ILogBackend> backend);

	bool should_log(Level level) const;
	ILogBackend* backend() const;
	void set_level(Level level) const;

private:
	std::shared_ptr<ILogBackend> backend_;
	const std::atomic<Level>*    levelPtr_;
};

void set_global_logger(std::shared_ptr<ILogBackend> backend);
Logger* get_global_logger();

namespace detail {
template <Level level, typename... Args>
inline void log_with_source(fmt::string_view        target,
							const char*             file,
							int                     line,
							fmt::format_string<Args...> formatStr,
							Args&&... args)
{
	if constexpr (should_compile(level)) {
		auto logger = get_global_logger();
		if (logger == nullptr || !logger->should_log(level)) {
			return;
		}

		logger->backend()->log(level, target, file, line, formatStr,
							   fmt::make_format_args(std::forward<Args>(args)...));
	}
}
}   // namespace detail

}   // namespace libca

#define LOG_LEVEL(level, target, ...) \
	::libca::detail::log_with_source<level>(target, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_TRACE(...) LOG_LEVEL(::libca::Level::Trace, "default", __VA_ARGS__)
#define LOG_DEBUG(...) LOG_LEVEL(::libca::Level::Debug, "default", __VA_ARGS__)
#define LOG_INFO(...) LOG_LEVEL(::libca::Level::Info, "default", __VA_ARGS__)
#define LOG_WARN(...) LOG_LEVEL(::libca::Level::Warn, "default", __VA_ARGS__)
#define LOG_ERROR(...) LOG_LEVEL(::libca::Level::Error, "default", __VA_ARGS__)
#define LOG_CRITICAL(...) LOG_LEVEL(::libca::Level::Critical, "default", __VA_ARGS__)

#define LOGT_TRACE(target, ...) LOG_LEVEL(::libca::Level::Trace, target, __VA_ARGS__)
#define LOGT_DEBUG(target, ...) LOG_LEVEL(::libca::Level::Debug, target, __VA_ARGS__)
#define LOGT_INFO(target, ...) LOG_LEVEL(::libca::Level::Info, target, __VA_ARGS__)
#define LOGT_WARN(target, ...) LOG_LEVEL(::libca::Level::Warn, target, __VA_ARGS__)
#define LOGT_ERROR(target, ...) LOG_LEVEL(::libca::Level::Error, target, __VA_ARGS__)
#define LOGT_CRITICAL(target, ...) \
	LOG_LEVEL(::libca::Level::Critical, target, __VA_ARGS__)


#endif   // ! LIBCA_LOG_LOGGER_H
