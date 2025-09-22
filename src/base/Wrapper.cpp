#include "Wrapper.hpp"

#ifdef TEST_ENABLE

#    include "../test/Test.hpp"

using namespace ca::test;
using namespace ca;

TEST_CASE(RefTest)
{
    int v = 0;
    Ref ref(v);
    ASSERT_EQUAL(0, ref);
    ref = 1;
    ASSERT_EQUAL(1, ref);
}

TEST_CASE(BoxTest)
{
    int v = 0;
    Box box(v);
    ASSERT_EQUAL(0, *box);
    *box = 1;
    ASSERT_EQUAL(1, *box);
}

#endif