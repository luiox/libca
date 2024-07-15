#include <doctest/doctest.h>
#include <libca/utility/string_util.hpp>

// using namespace libca::utility;

using namespace libca::utility;

TEST_CASE("libca::utility::StringUtil")
{
    CHECK_EQ(StringUtil::to_lower("HELLO"), "hello");
    CHECK_EQ(StringUtil::to_lower("Hello"), "hello");
    CHECK_EQ(StringUtil::to_lower("hello"), "hello");

    CHECK_EQ(StringUtil::to_upper("hello"), "HELLO");
    CHECK_EQ(StringUtil::to_upper("HELLO"), "HELLO");
    CHECK_EQ(StringUtil::to_upper("Hello"), "HELLO");

    CHECK_EQ(StringUtil::to_char("hello"), 'h');
    CHECK_EQ(StringUtil::to_char("HELLO"), 'H');

    CHECK_EQ(StringUtil::to_short("12345"), 12345);
    CHECK_EQ(StringUtil::to_short("-12345"), -12345);
}
