#include <gtest/gtest.h>

#include "libca/core/math_util.hpp"

#include <cmath>
#include <limits>

namespace ca::core::test {

TEST(MathUtilTest, MinMaxClampAreConstexprFriendly)
{
    static_assert(MathUtil::min(2, 3) == 2, "min should work in constexpr contexts");
    static_assert(MathUtil::max(2, 3) == 3, "max should work in constexpr contexts");
    static_assert(MathUtil::clamp(5, 0, 3) == 3, "clamp should cap high values");

    EXPECT_EQ(MathUtil::min(10, -3), -3);
    EXPECT_EQ(MathUtil::max(10, -3), 10);
    EXPECT_EQ(MathUtil::clamp(-1, 0, 10), 0);
    EXPECT_EQ(MathUtil::clamp(11, 0, 10), 10);
    EXPECT_EQ(MathUtil::clamp(5, 0, 10), 5);
}

TEST(MathUtilTest, AbsHandlesSignedUnsignedAndFloatingPoint)
{
    static_assert(MathUtil::abs(-7) == 7, "integer abs should be constexpr");
    static_assert(MathUtil::abs(7u) == 7u, "unsigned abs should return the input");

    EXPECT_EQ(MathUtil::abs(-42), 42);
    EXPECT_EQ(MathUtil::abs(42), 42);
    EXPECT_EQ(MathUtil::abs(42u), 42u);
    EXPECT_DOUBLE_EQ(MathUtil::abs(-3.5), 3.5);
}

TEST(MathUtilTest, FloorCeilRoundFloatAndDouble)
{
    EXPECT_FLOAT_EQ(MathUtil::floor(3.8f), 3.0f);
    EXPECT_FLOAT_EQ(MathUtil::ceil(3.2f), 4.0f);
    EXPECT_FLOAT_EQ(MathUtil::round(3.5f), 4.0f);
    EXPECT_FLOAT_EQ(MathUtil::round(-3.5f), -4.0f);

    EXPECT_DOUBLE_EQ(MathUtil::floor(3.8), 3.0);
    EXPECT_DOUBLE_EQ(MathUtil::ceil(3.2), 4.0);
    EXPECT_DOUBLE_EQ(MathUtil::round(3.5), 4.0);
    EXPECT_DOUBLE_EQ(MathUtil::round(-3.5), -4.0);
}

TEST(MathUtilTest, RoundToIntegerMatchesJavaMathRound)
{
    EXPECT_EQ(MathUtil::round_to_i32(3.5f), 4);
    EXPECT_EQ(MathUtil::round_to_i32(3.49f), 3);
    EXPECT_EQ(MathUtil::round_to_i32(-3.5f), -3);
    EXPECT_EQ(MathUtil::round_to_i32(-3.6f), -4);
    EXPECT_EQ(MathUtil::round_to_i32(std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_EQ(MathUtil::round_to_i32(std::numeric_limits<float>::infinity()),
              std::numeric_limits<ca::i32>::max());
    EXPECT_EQ(MathUtil::round_to_i32(-std::numeric_limits<float>::infinity()),
              std::numeric_limits<ca::i32>::min());

    EXPECT_EQ(MathUtil::round_to_i64(3.5), 4);
    EXPECT_EQ(MathUtil::round_to_i64(3.49), 3);
    EXPECT_EQ(MathUtil::round_to_i64(-3.5), -3);
    EXPECT_EQ(MathUtil::round_to_i64(-3.6), -4);
    EXPECT_EQ(MathUtil::round_to_i64(std::numeric_limits<double>::quiet_NaN()), 0);
    EXPECT_EQ(MathUtil::round_to_i64(std::numeric_limits<double>::infinity()),
              std::numeric_limits<ca::i64>::max());
    EXPECT_EQ(MathUtil::round_to_i64(-std::numeric_limits<double>::infinity()),
              std::numeric_limits<ca::i64>::min());
}

TEST(MathUtilTest, FloorDivAndFloorModMatchJavaIntegerSemantics)
{
    static_assert(MathUtil::floor_div<ca::i32>(7, 3) == 2, "positive floor_div");
    static_assert(MathUtil::floor_mod<ca::i32>(7, 3) == 1, "positive floor_mod");

    EXPECT_EQ(MathUtil::floor_div<ca::i32>(7, 3), 2);
    EXPECT_EQ(MathUtil::floor_mod<ca::i32>(7, 3), 1);
    EXPECT_EQ(MathUtil::floor_div<ca::i32>(-7, 3), -3);
    EXPECT_EQ(MathUtil::floor_mod<ca::i32>(-7, 3), 2);
    EXPECT_EQ(MathUtil::floor_div<ca::i32>(7, -3), -3);
    EXPECT_EQ(MathUtil::floor_mod<ca::i32>(7, -3), -2);
    EXPECT_EQ(MathUtil::floor_div<ca::i32>(-7, -3), 2);
    EXPECT_EQ(MathUtil::floor_mod<ca::i32>(-7, -3), -1);

    EXPECT_EQ(MathUtil::floor_div<ca::i32>(std::numeric_limits<ca::i32>::min(), -1),
              std::numeric_limits<ca::i32>::min());
    EXPECT_EQ(MathUtil::floor_mod<ca::i32>(std::numeric_limits<ca::i32>::min(), -1), 0);
    EXPECT_EQ(MathUtil::floor_div<ca::i64>(-7, 3), -3);
    EXPECT_EQ(MathUtil::floor_mod<ca::i64>(-7, 3), 2);
}

TEST(MathUtilTest, FloatingPointNaNPropagatesThroughStdMath)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    EXPECT_TRUE(std::isnan(MathUtil::floor(nan)));
    EXPECT_TRUE(std::isnan(MathUtil::ceil(nan)));
    EXPECT_TRUE(std::isnan(MathUtil::round(nan)));
}

}   // namespace ca::core::test
