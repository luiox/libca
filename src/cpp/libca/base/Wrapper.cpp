#include "Wrapper.hpp"

#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace ca::test;

TEST_CASE(RefTest)
{
    int     v = 0;
    ca::Ref ref(v);
    ASSERT_EQUAL(1, ref);
    ref = 1;
    ASSERT_EQUAL(1, ref);
}

#endif