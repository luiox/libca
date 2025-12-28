#include "test.h"

#if TEST_ENABLE

TEST_CASE(test_module)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
}

#endif   // TEST_ENABLE
