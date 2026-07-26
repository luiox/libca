#include <gtest/gtest.h>
#include <string>

#include "libca/collection/immutable_list.hpp"

using namespace ca::collection;

TEST(ImmutableListTest, constructFromValues) {
    auto list = ImmutableList<int>(1, 2, 3, 4, 5);
    ASSERT_EQ(list.size(), 5);
    EXPECT_EQ(list[0], 1);
    EXPECT_EQ(list[4], 5);
}

TEST(ImmutableListTest, empty) {
    auto list = ImmutableList<int>::create_empty();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
}

TEST(ImmutableListTest, ofFactory) {
    auto list = ImmutableList<int>::of(10, 20, 30);
    ASSERT_EQ(list.size(), 3);
    EXPECT_EQ(list[2], 30);
}

TEST(ImmutableListTest, rangeFor) {
    auto list = ImmutableList<int>(1, 2, 3);
    int sum = 0;
    for (const auto& v : list) sum += v;
    EXPECT_EQ(sum, 6);
}

TEST(ImmutableListTest, appended) {
    auto list = ImmutableList<int>(1, 2);
    auto list2 = list.appended(3);
    EXPECT_EQ(list.size(), 2);   // original unchanged
    EXPECT_EQ(list2.size(), 3);
    EXPECT_EQ(list2[2], 3);
}

TEST(ImmutableListTest, indexOutOfRange) {
    auto list = ImmutableList<int>(42);
    EXPECT_THROW(list[1], std::out_of_range);
}

TEST(ImmutableListTest, stringType) {
    auto list = ImmutableList<std::string>("hello", "world");
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0], "hello");
    EXPECT_EQ(list[1], "world");
}

TEST(ImmutableListTest, moveSemantics) {
    auto list = ImmutableList<int>(1, 2, 3);
    auto moved = std::move(list);
    EXPECT_EQ(moved.size(), 3);
    // list is now empty (moved-from state is valid but unspecified)
}
