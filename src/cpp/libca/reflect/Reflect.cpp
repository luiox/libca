#include "Enum.hpp"

using namespace ca;

enum Color
{
    RED,
    GREEN,
    BLUE
};

#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace ca::test;

TEST_CASE("Enum Reflect Test")
{
    Color color = RED;
    ASSERT_EQUAL("RED", get_enum_name(color));
    Color v = enum_from_name<Color>("RED");
}

TEST_CASE("Class Member Sizer")
{
    struct X { std::string s{ " " }; }x;
	struct Y { double a{}, b{}, c{}, d{}; }y;
	std::cout << size<X>() << '\n';
	std::cout << size<Y>() << '\n';

	auto print = [](const auto& member) {
		std::cout << member << ' ';
	};
	for_each_member(x, print);
	for_each_member(y, print);
}


#endif   // TEST_ENABLE
