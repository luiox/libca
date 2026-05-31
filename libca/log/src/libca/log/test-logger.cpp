#include <gtest/gtest.h>

#include "logger.hpp"
#include "spdlog_backend.hpp"

#include <memory>
#include <sstream>
#include <string>

#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>

namespace {

class LoggerGuard
{
public:
	~LoggerGuard()
	{
		libca::set_global_logger(std::shared_ptr<libca::ILogBackend>{});
	}
};

std::shared_ptr<libca::SpdlogBackend> makeBackend(std::ostringstream& output)
{
	auto sink   = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
	auto logger = std::make_shared<spdlog::logger>("test-libca.log", sink);
	logger->set_pattern("%v");
	logger->set_level(spdlog::level::trace);

	return std::make_shared<libca::SpdlogBackend>(logger);
}

}   // namespace

TEST(LibcaLog, LevelConversion)
{
	EXPECT_EQ(libca::stringToLevel("trace"), libca::Level::Trace);
	EXPECT_EQ(libca::stringToLevel("DEBUG"), libca::Level::Debug);
	EXPECT_EQ(libca::stringToLevel("unknown"), libca::Level::Off);
	EXPECT_EQ(libca::levelToString(libca::Level::Critical), "Critical");
}

TEST(LibcaLog, FacadeRuntimeFiltering)
{
	LoggerGuard guard;

	std::ostringstream output;
	auto               backend = makeBackend(output);
	libca::set_global_logger(backend);

	auto logger = libca::get_global_logger();
	ASSERT_NE(logger, nullptr);

	logger->set_level(libca::Level::Warn);

	if (logger->should_log(libca::Level::Info)) {
		int infoValue = 1;
		logger->backend()->log(libca::Level::Info,
							   "test",
							   __FILE__,
							   __LINE__,
							   "info {}",
							   fmt::make_format_args(infoValue));
	}

	if (logger->should_log(libca::Level::Error)) {
		int errorValue = 7;
		logger->backend()->log(libca::Level::Error,
							   "test",
							   __FILE__,
							   __LINE__,
							   "error {}",
							   fmt::make_format_args(errorValue));
	}

	const std::string logged = output.str();
	EXPECT_EQ(logged.find("info 1"), std::string::npos);
	EXPECT_NE(logged.find("error 7"), std::string::npos);
}

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
