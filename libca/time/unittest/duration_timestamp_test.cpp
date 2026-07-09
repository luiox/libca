#include <gtest/gtest.h>

#include "libca/time/duration.hpp"
#include "libca/time/timestamp.hpp"

#include <chrono>

using namespace ca::time;

constexpr Duration make_constexpr_duration()
{
    auto value = Duration::from_seconds(1);
    value += Duration::from_milliseconds(500);
    value -= Duration::from_microseconds(250);
    return value;
}

constexpr Timestamp make_constexpr_timestamp()
{
    auto value = Timestamp::from_unix_seconds(10);
    value += Duration::from_seconds(5);
    value -= Duration::from_milliseconds(500);
    return value;
}

static_assert(make_constexpr_duration().microseconds() == 1499750,
              "Duration compound arithmetic should be constexpr");
static_assert(make_constexpr_timestamp().unix_milliseconds() == 14500,
              "Timestamp compound arithmetic should be constexpr");
static_assert(Timestamp::from_time_point(Timestamp::from_unix_seconds(2).to_time_point()).unix_seconds() == 2,
              "Timestamp chrono conversion should be constexpr");

TEST(DurationTest, FactoriesAndAccessors)
{
    EXPECT_EQ(Duration::from_nanoseconds(42).nanoseconds(), 42);
    EXPECT_EQ(Duration::from_microseconds(2).nanoseconds(), 2000);
    EXPECT_EQ(Duration::from_milliseconds(3).microseconds(), 3000);
    EXPECT_EQ(Duration::from_seconds(4).milliseconds(), 4000);
    EXPECT_EQ(Duration::from_minutes(2).seconds(), 120);
    EXPECT_EQ(Duration::from_hours(1).seconds(), 3600);
}

TEST(DurationTest, ArithmeticAndComparison)
{
    auto a = Duration::from_seconds(2);
    auto b = Duration::from_milliseconds(500);

    EXPECT_EQ((a + b).milliseconds(), 2500);
    EXPECT_EQ((a - b).milliseconds(), 1500);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= b);

    a -= b;
    EXPECT_EQ(a.milliseconds(), 1500);
    a += b;
    EXPECT_EQ(a.seconds(), 2);
}

TEST(DurationTest, ChronoRoundtrip)
{
    auto duration = Duration::from_chrono(std::chrono::milliseconds(1250));
    EXPECT_EQ(duration.milliseconds(), 1250);
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(duration.to_chrono()).count(), 1250);
}

TEST(TimestampTest, UnixFactories)
{
    auto ts = Timestamp::from_unix_seconds(10);
    EXPECT_EQ(ts.unix_seconds(), 10);
    EXPECT_EQ(ts.unix_milliseconds(), 10000);
    EXPECT_EQ(ts.unix_nanoseconds(), 10000000000LL);
}

TEST(TimestampTest, ArithmeticAndComparison)
{
    auto base = Timestamp::from_unix_seconds(100);
    auto later = base + Duration::from_seconds(5);

    EXPECT_TRUE(later > base);
    EXPECT_EQ((later - base).seconds(), 5);
    EXPECT_EQ((later - Duration::from_seconds(5)).unix_seconds(), 100);

    later -= Duration::from_seconds(2);
    EXPECT_EQ(later.unix_seconds(), 103);
    later += Duration::from_seconds(2);
    EXPECT_EQ(later.unix_seconds(), 105);
}

TEST(TimestampTest, NowIsReasonable)
{
    auto now = Timestamp::now();
    EXPECT_GT(now.unix_seconds(), 1700000000LL);
    EXPECT_TRUE((now + Duration::from_seconds(60)).is_future());
    EXPECT_TRUE((now - Duration::from_seconds(60)).is_past());
}
