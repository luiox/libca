#include <gtest/gtest.h>

#include "libca/time/time_util.hpp"

#include <chrono>
#include <thread>

using namespace ca::time;

namespace {

ca::i64 std_current_time_millis()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<ca::i64>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

}   // namespace

TEST(TimeUtilTest, current_time_millisMatchesSystemClock)
{
    const ca::i64 before = std_current_time_millis();
    const ca::i64 actual = TimeUtil::current_time_millis();
    const ca::i64 after  = std_current_time_millis();

    EXPECT_GE(actual, before - 1);
    EXPECT_LE(actual, after + 1);
}

TEST(TimeUtilTest, current_time_millisLooksLikeUnixEpochMillis)
{
    const ca::i64 actual = TimeUtil::current_time_millis();

    EXPECT_GT(actual, 1600000000000LL);
}

TEST(TimeUtilTest, nano_timeIsMonotonicForRelativeTiming)
{
    const ca::i64 before = TimeUtil::nano_time();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const ca::i64 after = TimeUtil::nano_time();

    EXPECT_GE(before, 0);
    EXPECT_GE(after, before);
    EXPECT_GE(after - before, 500000LL);
}
