#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <sstream>

#include "libca/collection/stream.hpp"

using namespace ca::collection;

TEST(StreamTest, collectToVector) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto result = stream(vec).collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[4], 5);
}

TEST(StreamTest, filterEven) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6};
    auto result = stream(vec)
                      .filter([](int x) { return x % 2 == 0; })
                      .collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 6);
}

TEST(StreamTest, mapSquare) {
    std::vector<int> vec = {1, 2, 3};
    auto result = stream(vec)
                      .map([](int x) { return x * x; })
                      .collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 9);
}

TEST(StreamTest, filterThenMap) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6};
    auto result = stream(vec)
                      .filter([](int x) { return x % 2 == 0; })
                      .map([](int x) { return x * 10; })
                      .collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 20);
    EXPECT_EQ(result[1], 40);
    EXPECT_EQ(result[2], 60);
}

TEST(StreamTest, forEach) {
    std::vector<int> vec = {1, 2, 3};
    std::vector<int> output;
    stream(vec).forEach([&output](int x) { output.push_back(x); });
    ASSERT_EQ(output.size(), 3);
    EXPECT_EQ(output[0], 1);
}

TEST(StreamTest, emptyContainer) {
    std::vector<int> vec;
    auto result = stream(vec).collect<std::vector<int>>();
    EXPECT_TRUE(result.empty());
}

TEST(StreamTest, stringContainer) {
    std::vector<std::string> vec = {"hello", "world"};
    auto result = stream(vec)
                      .map([](const std::string& s) { return s + "!"; })
                      .collect<std::vector<std::string>>();
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "hello!");
    EXPECT_EQ(result[1], "world!");
}
