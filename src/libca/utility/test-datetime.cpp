#include <doctest/doctest.h>
#include <libca/utility/datetime.h>

// using namespace libca::utility;

using namespace libca::utility;

TEST_CASE("libca::utility::DateTime") { auto [date, time] = DateTime::now(); }
