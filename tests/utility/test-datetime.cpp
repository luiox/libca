#include <libca/utility/datetime.h>

using namespace libca::utility;

// TEST("libca::utility::DateTime") {  }

int
main()
{
    auto [date, time] = DateTime::now();
}
