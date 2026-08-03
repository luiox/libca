#include <gtest/gtest.h>

#include <memory>

#include "libca/log/log_macros.hpp"
#include "libca/log/logger.hpp"

#include "capture_backend.hpp"

// 编译期裁剪验证（issue 验收要求）：默认 CA_COMPILE_LOG_LEVEL=2 (Info)，
// 故 Trace/Debug 在编译期应被裁剪。下列 static_assert 若编译通过即证明裁剪逻辑正确。
static_assert(!ca::log::should_compile(ca::log::Level::Trace), "Trace below default level");
static_assert(!ca::log::should_compile(ca::log::Level::Debug), "Debug below default level");
static_assert(ca::log::should_compile(ca::log::Level::Info), "Info at default level");
static_assert(ca::log::should_compile(ca::log::Level::Critical), "Critical above default level");

namespace ca::log::test {
namespace {

// RAII 清理 registry，避免用例间污染。
class RegistryGuard
{
public:
    ~RegistryGuard() { LoggerRegistry::clear(); }
};

TEST(LoggerTest, ShouldLogRespectsLevel)
{
    Logger logger(std::make_shared<CaptureBackend>());
    logger.set_level(Level::Warn);

    EXPECT_FALSE(logger.should_log(Level::Trace));
    EXPECT_FALSE(logger.should_log(Level::Info));
    EXPECT_TRUE(logger.should_log(Level::Warn));
    EXPECT_TRUE(logger.should_log(Level::Error));
}

TEST(LoggerTest, NoBackendNeverLogs)
{
    Logger logger(nullptr);

    EXPECT_FALSE(logger.should_log(Level::Critical));
}

TEST(LoggerTest, SetLevelIsAtomic)
{
    Logger logger(std::make_shared<CaptureBackend>());
    logger.set_level(Level::Debug);
    EXPECT_EQ(logger.level(), Level::Debug);
    logger.set_level(Level::Off);
    EXPECT_EQ(logger.level(), Level::Off);
}

TEST(LoggerMacroTest, LogReachesBackend)
{
    RegistryGuard guard;
    auto          backend = std::make_shared<CaptureBackend>();
    LoggerRegistry::register_logger("default",
                                    std::make_shared<Logger>(backend));

    CA_LOG_INFO("hello {}", 42);

    auto entries = backend->entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].level, Level::Info);
    EXPECT_EQ(entries[0].message, "hello 42");
    EXPECT_EQ(entries[0].target, "default");
    EXPECT_GT(entries[0].line, 0);
    EXPECT_NE(entries[0].file.find("logger_test"), std::string::npos);
}

TEST(LoggerMacroTest, TargettedMacroUsesGivenTarget)
{
    RegistryGuard guard;
    auto net_backend = std::make_shared<CaptureBackend>();
    LoggerRegistry::register_logger("net", std::make_shared<Logger>(net_backend));

    CA_LOGT_WARN("net", "conn lost: {}", "10.0.0.1");

    auto entries = net_backend->entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].target, "net");
    EXPECT_EQ(entries[0].message, "conn lost: 10.0.0.1");
}

TEST(LoggerMacroTest, UnregisteredTargetIsDropped)
{
    RegistryGuard guard;
    // 不注册任何 target，宏应静默丢弃。
    CA_LOG_ERROR("this should be dropped {}", 1);
    // 无 crash 即通过；无后端可查。
    SUCCEED();
}

TEST(LoggerMacroTest, RuntimeLevelFiltersAtLogger)
{
    RegistryGuard guard;
    auto backend = std::make_shared<CaptureBackend>();
    auto logger  = std::make_shared<Logger>(backend);
    logger->set_level(Level::Error);
    LoggerRegistry::register_logger("default", logger);

    CA_LOG_INFO("filtered out");
    CA_LOG_ERROR("passes {}", 1);

    auto entries = backend->entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].message, "passes 1");
}

}  // namespace
}  // namespace ca::log::test
