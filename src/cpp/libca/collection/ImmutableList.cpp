#include "ImmutableList.hpp"

#include "libca/test/Test.hpp"

TEST_CASE("ImmutableList")
{
    auto list = ca::ImmutableList<int, 5>::of(1, 2, 3, 4, 5);
    REQUIRE_TRUE(list.size() == 5);
    REQUIRE_TRUE(list.get(0) == 1);

}
