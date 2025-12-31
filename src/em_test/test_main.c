#include "test.h"

#if TEST_ENABLE

#if TEST_SELF_MAIN

int main(int argc, char** argv)
{
    printf("Start running tests...\n");
    return run_tests();
}

#endif


#endif   // TEST_ENABLE