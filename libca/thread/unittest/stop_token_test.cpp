#include <gmock/gmock.h>

#include <chrono>
#include <future>

#include "libca/thread/stop_token.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

TEST(StopTokenTest, DefaultTokenHasNoStopState)
{
    StopToken token;

    EXPECT_FALSE(token.stop_possible());
    EXPECT_FALSE(token.stop_requested());
    EXPECT_FALSE(token.wait_for(1ms));
}

TEST(StopTokenTest, CopiesObserveOneIdempotentStopRequest)
{
    StopSource source;
    StopSource copy  = source;
    StopToken  token = source.token();

    EXPECT_TRUE(source.stop_possible());
    EXPECT_FALSE(token.stop_requested());
    EXPECT_TRUE(copy.request_stop());
    EXPECT_TRUE(token.stop_requested());
    EXPECT_TRUE(source.stop_requested());
    EXPECT_FALSE(source.request_stop());
}

TEST(StopTokenTest, WaitWakesWhenStopIsRequested)
{
    StopSource source;
    StopToken  token  = source.token();
    auto       waiter = std::async(std::launch::async, [token]() {
        token.wait();
        return token.stop_requested();
    });

    EXPECT_EQ(waiter.wait_for(10ms), std::future_status::timeout);
    EXPECT_TRUE(source.request_stop());
    EXPECT_EQ(waiter.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(waiter.get());
}

TEST(StopTokenTest, WaitForReportsTimeoutAndLaterStop)
{
    StopSource source;
    StopToken  token = source.token();

    EXPECT_FALSE(token.wait_for(1ms));
    source.request_stop();
    EXPECT_TRUE(token.wait_for(1s));
}

}   // namespace
}   // namespace ca::thread::test
