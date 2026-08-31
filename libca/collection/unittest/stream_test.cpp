#include <gtest/gtest.h>
#include <vector>
#include <string>

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

// 管道化语义：多个 filter 取交集（旧实现是覆盖式，只应用最后一个谓词）。
TEST(StreamTest, FiltersCompose) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto result = stream(vec)
                      .filter([](int x) { return x % 2 == 0; })  // 2 4 6 8 10
                      .filter([](int x) { return x > 2; })       // 4 6 8 10
                      .filter([](int x) { return x < 9; })       // 4 6 8
                      .collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 4);
    EXPECT_EQ(result[1], 6);
    EXPECT_EQ(result[2], 8);
}

// 管道化语义：多个 map 依次复合 ((x * 10) + 1)。
TEST(StreamTest, MapsCompose) {
    std::vector<int> vec = {1, 2};
    auto result = stream(vec)
                      .map([](int x) { return x * 10; })
                      .map([](int x) { return x + 1; })
                      .collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 11);
    EXPECT_EQ(result[1], 21);
}

// map 允许改变元素类型：int 流映射为 string 流。
TEST(StreamTest, mapChangesElementType) {
    std::vector<int> vec = {1, 2, 3};
    auto result = stream(vec)
                      .map([](int x) { return std::to_string(x); })
                      .collect<std::vector<std::string>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "1");
    EXPECT_EQ(result[2], "3");
}

TEST(StreamTest, ForEach) {
    std::vector<int> vec = {1, 2, 3};
    std::vector<int> output;
    stream(vec).for_each([&output](int x) { output.push_back(x); });
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

// ---- take / skip ----

TEST(StreamTest, takeLimitsOutput) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto result = stream(vec).take(3).collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[2], 3);
}

TEST(StreamTest, takeZeroYieldsNothing) {
    std::vector<int> vec = {1, 2, 3};
    auto result = stream(vec).take(0).collect<std::vector<int>>();
    EXPECT_TRUE(result.empty());
}

TEST(StreamTest, takeBeyondSizeYieldsAll) {
    std::vector<int> vec = {1, 2};
    auto result = stream(vec).take(10).collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 2);
}

TEST(StreamTest, skipDropsPrefix) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto result = stream(vec).skip(2).collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[2], 5);
}

TEST(StreamTest, skipBeyondSizeYieldsNothing) {
    std::vector<int> vec = {1, 2};
    auto result = stream(vec).skip(10).collect<std::vector<int>>();
    EXPECT_TRUE(result.empty());
}

TEST(StreamTest, filterMapTakeSkipChain) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7, 8};
    auto result = stream(vec)
                      .filter([](int x) { return x % 2 == 1; })  // 1 3 5 7
                      .skip(1)                                    // 3 5 7
                      .map([](int x) { return x * 100; })         // 300 500 700
                      .take(2)                                    // 300 500
                      .collect<std::vector<int>>();
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 300);
    EXPECT_EQ(result[1], 500);
}

// ---- count / reduce / any / all ----

TEST(StreamTest, countTerminal) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(stream(vec).count(), static_cast<ca::usize>(6));
    EXPECT_EQ(stream(vec).filter([](int x) { return x % 2 == 0; }).count(),
              static_cast<ca::usize>(3));
}

TEST(StreamTest, reduceTerminal) {
    std::vector<int> vec = {1, 2, 3, 4};
    const auto sum = stream(vec).reduce(0, [](int acc, int x) { return acc + x; });
    EXPECT_EQ(sum, 10);
    EXPECT_EQ(stream(vec).reduce(1, [](int acc, int x) { return acc * x; }), 24);
}

TEST(StreamTest, anyAndAllTerminals) {
    std::vector<int> vec = {2, 4, 6};
    EXPECT_TRUE(stream(vec).any([](int x) { return x > 5; }));
    EXPECT_FALSE(stream(vec).any([](int x) { return x > 10; }));
    EXPECT_TRUE(stream(vec).all([](int x) { return x % 2 == 0; }));
    EXPECT_FALSE(stream(vec).all([](int x) { return x > 3; }));
}

TEST(StreamTest, anyAndAllOnEmptyStream) {
    std::vector<int> vec;
    EXPECT_FALSE(stream(vec).any([](int x) { return x > 0; }));
    EXPECT_TRUE(stream(vec).all([](int x) { return x > 0; }));  // 空流上的 all 为真
}
