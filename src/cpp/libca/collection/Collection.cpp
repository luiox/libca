#include "Collection.hpp"
#include "libca/test/Test.hpp"

#include <iostream>
#include <vector>
#include <map>

TEST_CASE("ChainableVector")
{
    std::vector<int> vec = {1, 2, 3, 4, 5, 6};

    auto result = ChainableVector<int>(vec)
                      .filter([](const int& x) { return x % 2 == 1; })
                      .map<double>([](const int& x) { return x * x * 3.14; })
                      .collect();

    for (auto num : result) {
        std::cout << num << ' ';
    }
    std::cout << std::endl;
}

TEST_CASE("Stream")
{
    // std::vector<int> nums = {1, 2, 3, 4, 5};

    // auto result = Stream<std::vector<int>>(nums)
    //     .map([](int x) { return x * x; })
    //     .filter([](int x) { return x % 2 == 0; })
    //     .collect();
    // // map => 1 4 9 16 25
    // // filter => 4 16
    // ASSERT_EQUAL(result.size(), 2);
    // ASSERT_EQUAL(result[0], 4);
    // ASSERT_EQUAL(result[1], 16);

    {
        std::vector<int> vec = {1, 2, 3, 4, 5};

        auto result = LazyStream<std::vector<int>::iterator>(vec.begin(), vec.end())
                          .filter([](int x) { return x % 2 == 0; })   // 惰性过滤偶数
                          .map([](int x) { return x * x; })           // 惰性映射到平方
                          .collect<std::vector<int>>();               // 执行操作并收集结果

        // std::cout << "LazyStream: " << result;
        // std::cout << "LazyStream: ";
        // for (auto x : result) {
        //     std::cout << x << ' ';
        // }
        ASSERT_EQUAL(result.size(), 2);
        ASSERT_EQUAL(result[0], 4);
        ASSERT_EQUAL(result[1], 16);
    }

    {
        std::vector<int> vec = {1, 2, 3, 4, 5};

        auto result = stream(vec)
                          .filter([](int x) { return x % 2 == 0; })
                          .map([](int x) { return x * x; })
                          .forEach([](int x) { std::cout << x << ' '; })
                          .collect<std::vector<int>>();

        // std::cout << "LazyStream: " << result;
        // std::cout << "LazyStream: ";
        // for (auto x : result) {
        //     std::cout << x << ' ';
        // }
        std::cout << std::endl;
        ASSERT_EQUAL(result.size(), 2);
        ASSERT_EQUAL(result[0], 4);
        ASSERT_EQUAL(result[1], 16);
    }

}