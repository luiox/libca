#include "ImmutableList.hpp"

#include "libca/test/Test.hpp"
using namespace ca;

TEST_CASE("ImmutableList")
{
    // auto list = ca::ImmutableList::of(1, 2, 3, 4, 5);
    // REQUIRE_TRUE(list.size() == 5);
    // REQUIRE_TRUE(list.get(0) == 1);

    auto list = ImmutableList<int>::of(1, 2, 3);  
    //auto list2 = ImmutableList<std::string>::of("hello", "world"); 

}
