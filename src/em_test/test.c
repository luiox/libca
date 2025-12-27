#include "test.h"

#if TEST_ENABLE

TEST_CASE(test_module)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
}

int total_tests = 0;

int passed_tests = 0;

#if defined(_MSC_VER)
__declspec(allocate(".test$a")) const test_t* _test_start = NULL;
__declspec(allocate(".test$z")) const test_t* _test_stop  = NULL;
#endif

#if TEST_SELF_MAIN

int main(int argc, char** argv)
{
    printf("Start running tests...\n");
    run_tests();
    return LIBCA_TEST_SUCCESS;
}

#endif


#endif   // TEST_ENABLE
