#include <gtest/gtest.h>

#include "libca/core/array_util.hpp"

#include <stdexcept>

namespace ca::core::test {

TEST(ArrayUtilTest, EqualsComparesElementContent)
{
    const ca::i32 left[]      = {1, 2, 3};
    const ca::i32 same[]      = {1, 2, 3};
    const ca::i32 different[] = {1, 2, 4};

    EXPECT_TRUE(ArrayUtil::equals(left, 3, same, 3));
    EXPECT_FALSE(ArrayUtil::equals(left, 3, different, 3));
    EXPECT_FALSE(ArrayUtil::equals(left, 3, same, 2));
    EXPECT_TRUE(ArrayUtil::equals<ca::i32>(nullptr, 0, nullptr, 0));
    EXPECT_FALSE(ArrayUtil::equals<ca::i32>(nullptr, 3, same, 3));
}

TEST(ArrayUtilTest, FillAssignsEveryElement)
{
    ca::i32 values[] = {1, 2, 3, 4};

    ArrayUtil::fill(values, 4, 9);

    EXPECT_EQ(values[0], 9);
    EXPECT_EQ(values[1], 9);
    EXPECT_EQ(values[2], 9);
    EXPECT_EQ(values[3], 9);
    EXPECT_NO_THROW(ArrayUtil::fill<ca::i32>(nullptr, 0, 7));
    EXPECT_THROW(ArrayUtil::fill<ca::i32>(nullptr, 1, 7), std::invalid_argument);
}

TEST(ArrayUtilTest, CopyOfTruncatesOrPadsWithDefaultValues)
{
    const ca::i32 values[] = {1, 2, 3};

    auto shorter = ArrayUtil::copy_of(values, 3, 2);
    ASSERT_NE(shorter, nullptr);
    EXPECT_EQ(shorter[0], 1);
    EXPECT_EQ(shorter[1], 2);

    auto longer = ArrayUtil::copy_of(values, 3, 5);
    ASSERT_NE(longer, nullptr);
    EXPECT_EQ(longer[0], 1);
    EXPECT_EQ(longer[1], 2);
    EXPECT_EQ(longer[2], 3);
    EXPECT_EQ(longer[3], 0);
    EXPECT_EQ(longer[4], 0);

    EXPECT_EQ(ArrayUtil::copy_of(values, 3, 0), nullptr);
    EXPECT_THROW(ArrayUtil::copy_of<ca::i32>(nullptr, 1, 1), std::invalid_argument);
}

TEST(ArrayUtilTest, CopyOfRangeSlicesAndPads)
{
    const ca::i32 values[] = {1, 2, 3};

    auto middle = ArrayUtil::copy_of_range(values, 3, 1, 3);
    ASSERT_NE(middle, nullptr);
    EXPECT_EQ(middle[0], 2);
    EXPECT_EQ(middle[1], 3);

    auto padded = ArrayUtil::copy_of_range(values, 3, 2, 5);
    ASSERT_NE(padded, nullptr);
    EXPECT_EQ(padded[0], 3);
    EXPECT_EQ(padded[1], 0);
    EXPECT_EQ(padded[2], 0);

    EXPECT_EQ(ArrayUtil::copy_of_range(values, 3, 2, 2), nullptr);
    EXPECT_THROW(ArrayUtil::copy_of_range(values, 3, 2, 1), std::invalid_argument);
    EXPECT_THROW(ArrayUtil::copy_of_range(values, 3, 4, 5), std::out_of_range);
    EXPECT_THROW(ArrayUtil::copy_of_range<ca::i32>(nullptr, 1, 0, 1), std::invalid_argument);
}

}   // namespace ca::core::test
