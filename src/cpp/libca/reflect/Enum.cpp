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

TEST_CASE(EnumReflectTest)
{
    Color color = RED;
    ASSERT_EQUAL("RED", get_enum_name(color));
    Color v = enum_from_name<Color>("RED");
}

#endif   // TEST_ENABLE
