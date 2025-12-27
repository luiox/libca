#include <libca/core/test.h>

extern void run_tests();

int main(int argc, char** argv)
{
    printf("Start running tests...\n");
    run_tests();
    return LIBCA_TEST_SUCCESS;
}
