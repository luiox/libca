#include "libca/time/duration.hpp"
#include "libca/time/timestamp.hpp"

#include <chrono>
#include <limits>
#include <ratio>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::time;

namespace {

constexpr ca::i64 MIN_VALUE = std::numeric_limits<ca::i64>::min();
constexpr ca::i64 MAX_VALUE = std::numeric_limits<ca::i64>::max();
constexpr auto MIN_DURATION = Duration::from_nanoseconds(MIN_VALUE);
constexpr auto MAX_DURATION = Duration::from_nanoseconds(MAX_VALUE);
constexpr auto MIN_TIMESTAMP = Timestamp::from_unix_nanoseconds(MIN_VALUE);
constexpr auto MAX_TIMESTAMP = Timestamp::from_unix_nanoseconds(MAX_VALUE);

template<typename T, typename = void>
struct HasCheckedChrono : std::false_type {};

template<typename T>
struct HasCheckedChrono<T, std::void_t<decltype(Duration::checked_from_chrono(std::declval<T>()))>>
    : std::true_type {};

constexpr auto SECOND = Duration::checked_from_seconds(1);
static_assert(SECOND && SECOND->nanoseconds() == 1000000000, "checked factories must be constexpr");
static_assert(!Duration::checked_from_seconds(MAX_VALUE), "overflow must fail in constant evaluation");
static_assert(!MAX_DURATION.checked_add(Duration::from_nanoseconds(1)), "checked addition guards overflow");
static_assert(MIN_DURATION.checked_sub(MIN_DURATION)->is_zero(), "subtraction must not negate INT64_MIN");
static_assert(!MIN_DURATION.checked_neg(), "negating INT64_MIN overflows");
static_assert(MIN_DURATION.checked_mul(1)->nanoseconds() == MIN_VALUE, "INT64_MIN itself is valid");
static_assert(!MIN_DURATION.checked_mul(-1), "multiplication guards negative extrema");

constexpr auto SUB_NANOSECOND = Duration::checked_from_chrono(std::chrono::duration<ca::i64, std::pico>(-1999));
static_assert(SUB_NANOSECOND && SUB_NANOSECOND->nanoseconds() == -1, "sub-nanoseconds truncate toward zero");
constexpr auto MAX_CHRONO = Duration::checked_from_chrono(std::chrono::duration<ca::i64, std::nano>(MAX_VALUE));
static_assert(MAX_CHRONO && MAX_CHRONO->nanoseconds() == MAX_VALUE, "chrono endpoint is exact");
constexpr auto MIN_CHRONO = Duration::checked_from_chrono(std::chrono::duration<ca::i64, std::nano>(MIN_VALUE));
static_assert(MIN_CHRONO && MIN_CHRONO->nanoseconds() == MIN_VALUE, "negative chrono endpoint is exact");
static_assert(!HasCheckedChrono<std::chrono::duration<double>>::value, "floating chrono is excluded");
static_assert(HasCheckedChrono<std::chrono::duration<ca::u64>>::value, "unsigned chrono is supported");

constexpr auto BEFORE_EPOCH = Timestamp::from_unix_nanoseconds(-1);
static_assert(BEFORE_EPOCH.unix_seconds() == 0, "legacy conversion keeps truncation");
static_assert(BEFORE_EPOCH.unix_seconds_floor() == -1, "floor conversion names preceding second");
static_assert(BEFORE_EPOCH.unix_milliseconds_floor() == -1, "floor conversion names preceding millisecond");
static_assert(MIN_TIMESTAMP.unix_seconds_floor() == -9223372037LL, "floor cannot negate INT64_MIN");
static_assert(MIN_TIMESTAMP.unix_milliseconds_floor() == -9223372036855LL, "millisecond floor handles INT64_MIN");
static_assert(!MAX_TIMESTAMP.checked_add(Duration::from_nanoseconds(1)), "timestamp addition guards overflow");
static_assert(!MAX_TIMESTAMP.checked_duration_since(MIN_TIMESTAMP), "timestamp differences may overflow");
static_assert(MIN_TIMESTAMP.checked_duration_since(MIN_TIMESTAMP)->is_zero(), "identical endpoints subtract safely");

using MillisecondPoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
constexpr auto CHECKED_TIME_POINT = Timestamp::checked_from_time_point(MillisecondPoint(std::chrono::milliseconds(-1)));
static_assert(CHECKED_TIME_POINT && CHECKED_TIME_POINT->unix_nanoseconds() == -1000000,
              "checked chrono time-point conversion is constexpr");

}  // namespace

TEST(DurationCheckedTest, FactoriesAcceptLastRepresentableUnitsAndRejectNeighbors) {
    using Factory = std::optional<Duration> (*)(ca::i64);
    struct UnitCase {
        Factory make;
        ca::i64 scale;
    };
    const UnitCase cases[] = {
        {Duration::checked_from_microseconds, 1000},
        {Duration::checked_from_milliseconds, 1000000},
        {Duration::checked_from_seconds, 1000000000},
        {Duration::checked_from_minutes, 60000000000LL},
        {Duration::checked_from_hours, 3600000000000LL},
    };
    for (const auto& unit : cases) {
        SCOPED_TRACE(unit.scale);
        const auto positive = unit.make(MAX_VALUE / unit.scale);
        const auto negative = unit.make(MIN_VALUE / unit.scale);
        ASSERT_TRUE(positive);
        ASSERT_TRUE(negative);
        EXPECT_EQ(positive->nanoseconds(), (MAX_VALUE / unit.scale) * unit.scale);
        EXPECT_EQ(negative->nanoseconds(), (MIN_VALUE / unit.scale) * unit.scale);
        EXPECT_FALSE(unit.make(MAX_VALUE / unit.scale + 1));
        EXPECT_FALSE(unit.make(MIN_VALUE / unit.scale - 1));
        EXPECT_EQ(unit.make(0)->nanoseconds(), 0);
    }
}

TEST(DurationCheckedTest, AdditionAndSubtractionCoverBothLimits) {
    const auto one = Duration::from_nanoseconds(1);
    const auto minus_one = Duration::from_nanoseconds(-1);

    EXPECT_FALSE(MAX_DURATION.checked_add(one));
    EXPECT_FALSE(MIN_DURATION.checked_add(minus_one));
    EXPECT_FALSE(MIN_DURATION.checked_sub(one));
    EXPECT_FALSE(MAX_DURATION.checked_sub(minus_one));
    EXPECT_EQ(MAX_DURATION.checked_add(minus_one)->nanoseconds(), MAX_VALUE - 1);
    EXPECT_EQ(MIN_DURATION.checked_sub(minus_one)->nanoseconds(), MIN_VALUE + 1);
    EXPECT_EQ(MAX_DURATION.checked_sub(MAX_DURATION)->nanoseconds(), 0);
    EXPECT_EQ(MIN_DURATION.checked_sub(MIN_DURATION)->nanoseconds(), 0);
    EXPECT_EQ(MIN_DURATION.checked_add(MAX_DURATION)->nanoseconds(), -1);
    EXPECT_FALSE(Duration().checked_sub(MIN_DURATION));
}

TEST(DurationCheckedTest, MultiplicationAndNegationCoverAllSignQuadrants) {
    const auto two = Duration::from_nanoseconds(2);
    const auto minus_two = Duration::from_nanoseconds(-2);

    EXPECT_EQ(two.checked_mul(3)->nanoseconds(), 6);
    EXPECT_EQ(two.checked_mul(-3)->nanoseconds(), -6);
    EXPECT_EQ(minus_two.checked_mul(3)->nanoseconds(), -6);
    EXPECT_EQ(minus_two.checked_mul(-3)->nanoseconds(), 6);
    EXPECT_EQ(Duration().checked_mul(MIN_VALUE)->nanoseconds(), 0);
    EXPECT_EQ(MIN_DURATION.checked_mul(0)->nanoseconds(), 0);
    EXPECT_EQ(Duration::from_nanoseconds(1).checked_mul(MIN_VALUE)->nanoseconds(), MIN_VALUE);
    EXPECT_FALSE(Duration::from_nanoseconds(-1).checked_mul(MIN_VALUE));
    EXPECT_FALSE(MIN_DURATION.checked_mul(-1));
    EXPECT_FALSE(MAX_DURATION.checked_mul(2));
    EXPECT_EQ(Duration::from_nanoseconds(MIN_VALUE / 2).checked_mul(2)->nanoseconds(), MIN_VALUE);
    EXPECT_EQ(two.checked_neg()->nanoseconds(), -2);
    EXPECT_EQ(MAX_DURATION.checked_neg()->nanoseconds(), -MAX_VALUE);
    EXPECT_FALSE(MIN_DURATION.checked_neg());
}

TEST(DurationCheckedTest, ChronoSignedAndUnsignedCountsRetainExactNanosecondLimits) {
    using UnsignedNanos = std::chrono::duration<ca::u64, std::nano>;
    using SignedNanos = std::chrono::duration<ca::i64, std::nano>;

    EXPECT_EQ(Duration::checked_from_chrono(SignedNanos(MIN_VALUE))->nanoseconds(), MIN_VALUE);
    EXPECT_EQ(Duration::checked_from_chrono(UnsignedNanos(MAX_VALUE))->nanoseconds(), MAX_VALUE);
    EXPECT_FALSE(Duration::checked_from_chrono(UnsignedNanos(ca::u64{1} << 63)));
    EXPECT_FALSE(Duration::checked_from_chrono(UnsignedNanos(std::numeric_limits<ca::u64>::max())));
    EXPECT_EQ(Duration::checked_from_chrono(std::chrono::duration<ca::i8>(-2))->seconds(), -2);
    EXPECT_EQ(Duration::checked_from_chrono(std::chrono::duration<bool>(true))->seconds(), 1);
}

TEST(DurationCheckedTest, ChronoRationalPeriodsUseTruncationWithoutIntermediateOverflow) {
    using Thirds = std::chrono::duration<ca::i64, std::ratio<1, 3>>;
    using HalfNanos = std::chrono::duration<ca::i64, std::ratio<1, 2000000000>>;
    using Picos = std::chrono::duration<ca::i64, std::pico>;
    using HugePeriod = std::chrono::duration<ca::i64, std::ratio<MAX_VALUE>>;
    using FractionalPeriod = std::chrono::duration<ca::u64, std::ratio<MAX_VALUE - 1, MAX_VALUE>>;
    using TinyPeriod = std::chrono::duration<ca::u64, std::ratio<1, MAX_VALUE>>;

    EXPECT_EQ(Duration::checked_from_chrono(Thirds(2))->nanoseconds(), 666666666);
    EXPECT_EQ(Duration::checked_from_chrono(Thirds(-2))->nanoseconds(), -666666666);
    EXPECT_EQ(Duration::checked_from_chrono(HalfNanos(-3))->nanoseconds(), -1);
    EXPECT_EQ(Duration::checked_from_chrono(Picos(999))->nanoseconds(), 0);
    EXPECT_EQ(Duration::checked_from_chrono(Picos(-1999))->nanoseconds(), -1);
    EXPECT_EQ(Duration::checked_from_chrono(HugePeriod(0))->nanoseconds(), 0);
    EXPECT_FALSE(Duration::checked_from_chrono(HugePeriod(1)));
    EXPECT_EQ(Duration::checked_from_chrono(FractionalPeriod(1))->nanoseconds(), 999999999);
    EXPECT_EQ(Duration::checked_from_chrono(TinyPeriod(std::numeric_limits<ca::u64>::max()))->nanoseconds(),
              2000000000);
    EXPECT_FALSE((Duration::checked_from_chrono(std::chrono::duration<ca::i64, std::ratio<3600>>(MAX_VALUE))));
}

TEST(TimestampCheckedTest, FactoriesCheckBothEnds) {
    EXPECT_TRUE(Timestamp::checked_from_unix_milliseconds(MAX_VALUE / 1000000));
    EXPECT_TRUE(Timestamp::checked_from_unix_milliseconds(MIN_VALUE / 1000000));
    EXPECT_FALSE(Timestamp::checked_from_unix_milliseconds(MAX_VALUE / 1000000 + 1));
    EXPECT_FALSE(Timestamp::checked_from_unix_milliseconds(MIN_VALUE / 1000000 - 1));
    EXPECT_TRUE(Timestamp::checked_from_unix_seconds(MAX_VALUE / 1000000000));
    EXPECT_TRUE(Timestamp::checked_from_unix_seconds(MIN_VALUE / 1000000000));
    EXPECT_FALSE(Timestamp::checked_from_unix_seconds(MAX_VALUE / 1000000000 + 1));
    EXPECT_FALSE(Timestamp::checked_from_unix_seconds(MIN_VALUE / 1000000000 - 1));
}

TEST(TimestampCheckedTest, ArithmeticAndDifferenceCheckBothLimits) {
    const auto one = Duration::from_nanoseconds(1);
    const auto minus_one = Duration::from_nanoseconds(-1);

    EXPECT_FALSE(MAX_TIMESTAMP.checked_add(one));
    EXPECT_FALSE(MIN_TIMESTAMP.checked_add(minus_one));
    EXPECT_FALSE(MIN_TIMESTAMP.checked_sub(one));
    EXPECT_FALSE(MAX_TIMESTAMP.checked_sub(minus_one));
    EXPECT_EQ(MIN_TIMESTAMP.checked_add(MAX_DURATION)->unix_nanoseconds(), -1);
    EXPECT_EQ(MIN_TIMESTAMP.checked_sub(MIN_DURATION)->unix_nanoseconds(), 0);
    EXPECT_EQ(Timestamp().checked_add(MIN_DURATION)->unix_nanoseconds(), MIN_VALUE);
    EXPECT_FALSE(Timestamp().checked_sub(MIN_DURATION));
    EXPECT_FALSE(MAX_TIMESTAMP.checked_duration_since(MIN_TIMESTAMP));
    EXPECT_FALSE(MIN_TIMESTAMP.checked_duration_since(MAX_TIMESTAMP));
    EXPECT_EQ(Timestamp().checked_duration_since(MAX_TIMESTAMP)->nanoseconds(), -MAX_VALUE);
    EXPECT_EQ(MIN_TIMESTAMP.checked_duration_since(Timestamp())->nanoseconds(), MIN_VALUE);
}

TEST(TimestampCheckedTest, TimePointConversionChecksRepresentableNanosecondRange) {
    using SecondsPoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;
    const auto valid = Timestamp::checked_from_time_point(SecondsPoint(std::chrono::seconds(-1)));
    ASSERT_TRUE(valid);
    EXPECT_EQ(valid->unix_nanoseconds(), -1000000000);
    EXPECT_FALSE(Timestamp::checked_from_time_point(SecondsPoint(std::chrono::seconds(MAX_VALUE))));
    EXPECT_FALSE(Timestamp::checked_from_time_point(SecondsPoint(std::chrono::seconds(MIN_VALUE))));
}

TEST(TimestampCheckedTest, NegativeUnixFloorsDistinguishPartialPreEpochUnits) {
    struct UnitCase {
        ca::i64 nanoseconds;
        ca::i64 seconds;
        ca::i64 seconds_floor;
        ca::i64 milliseconds;
        ca::i64 milliseconds_floor;
    };
    const UnitCase cases[] = {
        {MIN_VALUE, -9223372036LL, -9223372037LL, -9223372036854LL, -9223372036855LL},
        {-1000000001, -1, -2, -1000, -1001},
        {-1000000000, -1, -1, -1000, -1000},
        {-1000001, 0, -1, -1, -2},
        {-1000000, 0, -1, -1, -1},
        {-1, 0, -1, 0, -1},
        {0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0},
        {1000000001, 1, 1, 1000, 1000},
        {MAX_VALUE, 9223372036LL, 9223372036LL, 9223372036854LL, 9223372036854LL},
    };
    for (const auto& unit : cases) {
        SCOPED_TRACE(unit.nanoseconds);
        const auto timestamp = Timestamp::from_unix_nanoseconds(unit.nanoseconds);
        EXPECT_EQ(timestamp.unix_seconds(), unit.seconds);
        EXPECT_EQ(timestamp.unix_seconds_floor(), unit.seconds_floor);
        EXPECT_EQ(timestamp.unix_milliseconds(), unit.milliseconds);
        EXPECT_EQ(timestamp.unix_milliseconds_floor(), unit.milliseconds_floor);
    }
}
