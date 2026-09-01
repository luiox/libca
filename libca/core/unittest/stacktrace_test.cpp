#include <gmock/gmock.h>

#include "libca/core/stacktrace.hpp"

namespace ca::core {
namespace test {

using namespace testing;

TEST(StacktraceTest, CaptureReturnsNonEmpty)
{
    auto trace = capture_stack_trace(16);
    EXPECT_FALSE(trace.empty());
}

TEST(StacktraceTest, PrintToStderrDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(print_stack_trace(8));
}

TEST(StacktraceTest, ZeroMaxFramesClampedToOne)
{
    auto trace = capture_stack_trace(0);
    EXPECT_FALSE(trace.empty());
}

TEST(StacktraceTest, LargeMaxFramesClamped)
{
    auto trace = capture_stack_trace(999);
    EXPECT_FALSE(trace.empty());
}

}   // namespace test
}   // namespace ca::core
