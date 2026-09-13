#include <gtest/gtest.h>

#include <memory>
#include <ostream>
#include <sstream>

#include <fmt/core.h>

#include "libca/log/log_macros.hpp"
#include "libca/log/logger.hpp"
#include "libca/log/logger_registry.hpp"

#include "capture_backend.hpp"

namespace ca::log::test {

// ==================== 短路求值探测 ====================
//
// 证明手段：禁用级别下的日志宏若"急切求值"，实参表达式会产生可观测副作用；
// 短路求值则副作用计数保持为 0。三类探测器互相印证：
// - lambda 调用：调用即递增计数器；
// - 探测对象构造：构造函数递增计数器（证明对象本身未被创建）；
// - 探测对象 operator<< / fmt formatter：流式输出与格式化渲染各递增计数器。

// lambda / 探测对象构造计数（各用例独立变量）。
// 流式输出与 fmt 格式化渲染计数（全局，便于在 fmt 特化内递增）。
int g_probe_stream_count  = 0;
int g_probe_format_count  = 0;

/// @brief 探测对象：构造即计数，附带 operator<< 供流式路径探测。
class Probe
{
public:
    explicit Probe(int* constructed) : constructed_(constructed) { ++(*constructed_); }

    int* constructed_;
};

std::ostream& operator<<(std::ostream& out, const Probe& /*probe*/)
{
    ++g_probe_stream_count;
    out << "probe";
    return out;
}

}  // namespace ca::log::test

// fmt 渲染探测：formatter 只有在真正格式化时才被调起。显式特化必须位于 fmt 命名空间。
template <> struct fmt::formatter<ca::log::test::Probe> : fmt::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const ca::log::test::Probe& probe, FormatContext& context) const
    {
        ++ca::log::test::g_probe_format_count;
        return fmt::formatter<std::string_view>::format("probe", context);
    }
};

namespace ca::log::test {
namespace {

// RAII 清理 registry，避免用例间污染。
class RegistryGuard
{
public:
    ~RegistryGuard() { LoggerRegistry::clear(); }
};

void reset_probe_counters(int* constructed)
{
    *constructed          = 0;
    g_probe_stream_count  = 0;
    g_probe_format_count  = 0;
}

// 注册一个级别为 level 的 default target。
void register_logger_with_level(Level level)
{
    auto backend = std::make_shared<CaptureBackend>();
    auto logger  = std::make_shared<Logger>(backend);
    logger->set_level(level);
    LoggerRegistry::register_logger("default", logger);
}

// 运行期级别不满足（Info < Error_）：实参 lambda 不得被调用。
TEST(LogMacroShortCircuitTest, RuntimeDisabledLevelSkipsLambdaEvaluation)
{
    RegistryGuard guard;
    register_logger_with_level(Level::Error_);

    int eval_count = 0;
    CA_LOG_INFO("eager {}", [&eval_count] { ++eval_count; return 42; }());

    EXPECT_EQ(eval_count, 0);
}

// 未注册 target：LoggerRegistry::get 返回 nullptr，同样必须短路。
TEST(LogMacroShortCircuitTest, UnregisteredTargetSkipsLambdaEvaluation)
{
    RegistryGuard guard;

    int eval_count = 0;
    CA_LOG_CRITICAL("nobody home {}", [&eval_count] { ++eval_count; return 7; }());

    EXPECT_EQ(eval_count, 0);
}

// 编译期被裁剪的级别（默认 CA_COMPILE_LOG_LEVEL=2 即 Info，Trace/Debug 不生成调用）：
// 实参零求值——该行为由 if constexpr 固有保证，此处测试固化之，防止未来改造回退。
TEST(LogMacroShortCircuitTest, CompileTimeDisabledLevelSkipsLambdaEvaluation)
{
    int eval_count = 0;
    CA_LOG_TRACE("compile-time disabled {}", [&eval_count] { ++eval_count; return 1; }());

    EXPECT_EQ(eval_count, 0);
}

// 探测对象：运行期级别不满足时，对象构造、operator<<、fmt 渲染均不发生。
TEST(LogMacroShortCircuitTest, RuntimeDisabledLevelSkipsProbeObject)
{
    RegistryGuard guard;
    register_logger_with_level(Level::Critical);

    int constructed = 0;
    reset_probe_counters(&constructed);
    CA_LOGT_INFO("default", "value={}", Probe(&constructed));

    EXPECT_EQ(constructed, 0);
    EXPECT_EQ(g_probe_stream_count, 0);
    EXPECT_EQ(g_probe_format_count, 0);
}

// 自证探测装置有效：级别放行时三路计数器都会真实工作。
// 注意 fmt 走 formatter 而不走 operator<<，故 stream 计数仅在直接流式输出时递增。
TEST(LogMacroShortCircuitTest, ProbeRigDetectsEvaluationWhenEnabled)
{
    RegistryGuard guard;
    register_logger_with_level(Level::Trace);

    int constructed = 0;
    reset_probe_counters(&constructed);
    CA_LOGT_INFO("default", "value={}", Probe(&constructed));

    EXPECT_EQ(constructed, 1);
    EXPECT_EQ(g_probe_format_count, 1);
    EXPECT_EQ(g_probe_stream_count, 0);

    // 验证 operator<< 探测路径本身可用（与日志宏无关的直接流式输出）。
    std::ostringstream stream;
    stream << Probe(&constructed);
    EXPECT_EQ(g_probe_stream_count, 1);
    EXPECT_EQ(stream.str(), "probe");
}

}  // namespace
}  // namespace ca::log::test
