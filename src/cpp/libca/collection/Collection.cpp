#include "Collection.hpp"
#include "libca/test/Test.hpp"
#include <iostream>
#include <vector>
#include <map>

TEST_CASE("Stream")
{
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};

        auto result = stream(vec)
                          .filter([](int x) { return x % 2 == 0; })
                          .map([](int x) { return x * x; })
                          .forEach([](int x) { std::cout << x << ' '; })
                          .collect<std::vector<int>>();
        std::cout << std::endl;
        ASSERT_EQUAL(result.size(), 2);
        ASSERT_EQUAL(result[0], 4);
        ASSERT_EQUAL(result[1], 16);

        // std::cout << "Stream: ";
        // for (auto x : result) {
        //     std::cout << x << ' ';
        // }
    }

}