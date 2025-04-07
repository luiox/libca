#include "Collection.hpp"
#include "libca/test/Test.hpp"

#include <iostream>

TEST_CASE("ChainableVector")
{
    std::vector<int> vec = { 1, 2, 3, 4, 5, 6 };

    auto result = ChainableVector<int>(vec)
        .filter([](const int& x) { return x % 2 == 1; })
        .map<double>([](const int& x) { return x * x* 3.14; })
        .collect();

    for (auto num : result) {
        std::cout << num << ' ';
    }
    std::cout << std::endl;
}