// 仅 with_spdlog=y 时编译本文件（由 xmake 的 has_config("with_spdlog") 控制 add_files）。
#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>

#include "libca/log/logger.hpp"
#include "libca/log/spdlog/spdlog_backend.hpp"

namespace ca::log::test {
namespace {

// 测试用 OpaqueFormat：固定字符串，不依赖门面 fmt 路径。
class LiteralFormat final : public OpaqueFormat
{
public:
    explicit LiteralFormat(std::string s)
        : s_(std::move(s))
    {}
    void render_to(std::string& out) const override { out += s_; }

private:
    std::string s_;
};

std::shared_ptr<SpdlogBackend> make_backend(std::ostringstream& out)
{
    auto sink   = std::make_shared<spdlog::sinks::ostream_sink_mt>(out);
    auto logger = std::make_shared<spdlog::logger>("test", sink);
    logger->set_pattern("%v");   // 只输出消息体，便于断言
    logger->set_level(spdlog::level::trace);
    return std::make_shared<SpdlogBackend>(logger);
}

TEST(SpdlogBackendTest, RoutesRenderedMessageToSpdlog)
{
    std::ostringstream out;
    auto               backend = make_backend(out);

    backend->log(Level::Info, "net", "net.cpp", 10, LiteralFormat("hello spdlog"));

    std::string text = out.str();
    EXPECT_NE(text.find("hello spdlog"), std::string::npos);
}

TEST(SpdlogBackendTest, AllLevelsMapped)
{
    std::ostringstream out;
    auto               backend = make_backend(out);

    backend->log(Level::Trace, "t", "f", 1, LiteralFormat("T"));
    backend->log(Level::Debug, "t", "f", 1, LiteralFormat("D"));
    backend->log(Level::Info, "t", "f", 1, LiteralFormat("I"));
    backend->log(Level::Warn, "t", "f", 1, LiteralFormat("W"));
    backend->log(Level::Error_, "t", "f", 1, LiteralFormat("E"));
    backend->log(Level::Critical, "t", "f", 1, LiteralFormat("C"));

    std::string text = out.str();
    EXPECT_NE(text.find("T"), std::string::npos);
    EXPECT_NE(text.find("C"), std::string::npos);
}

TEST(SpdlogBackendTest, NullLoggerIsSafe)
{
    SpdlogBackend backend(nullptr);
    EXPECT_NO_THROW({ backend.log(Level::Info, "t", "f", 1, LiteralFormat("dropped")); });
}

}   // namespace
}   // namespace ca::log::test
