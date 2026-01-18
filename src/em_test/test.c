#include "test.h"

int total_tests = 0;

int passed_tests = 0;

int failed_tests = 0;

int current_test_failed = 0;

#if defined(_MSC_VER)
__declspec(allocate(".test$a")) const test_t* _test_start = NULL;
__declspec(allocate(".test$z")) const test_t* _test_stop  = NULL;
#else
TEST_CASE_ALLOC const test_t* _test_dummy = NULL;
#endif

int run_tests()
{
    total_tests = 0;
    passed_tests = 0;
    failed_tests = 0;

#if defined(_MSC_VER)
    const test_t** begin = (const test_t**)&_test_start;
    const test_t** end   = (const test_t**)&_test_stop;

    // 计算个数
    for (const test_t** it = begin; it <= end; it++) {
        if (*it != NULL && (*it)->name != NULL) {
            total_tests++;
        }
    }
    printf("num_tests = %d\n", total_tests);

    for (const test_t** it = begin; it <= end; it++) {
        if (*it == NULL || (*it)->name == NULL) {
            continue;
        }
        printf("Running test: %s\n", (*it)->name);
        current_test_failed = 0;
        (*it)->func();
        if (current_test_failed) {
            failed_tests++;
        } else {
            passed_tests++;
        }
    }
#else
    const test_t** begin = (const test_t**)__start_test_array;
    const test_t** end   = (const test_t**)__stop_test_array;

    for (const test_t** t = begin; t < end; t++) {
        if (*t != NULL && (*t)->name != NULL)
            total_tests++;
    }
    printf("num_tests = %d\n", total_tests);

    for (const test_t** t = begin; t < end; t++) {
        if (*t == NULL || (*t)->name == NULL)
            continue;
        printf("Running test: %s\n", (*t)->name);
        current_test_failed = 0;
        (*t)->func();
        if (current_test_failed) {
            failed_tests++;
        } else {
            passed_tests++;
        }
    }
#endif
    printf("\nTests finished: %d total, %d passed, %d failed\n", total_tests, passed_tests, failed_tests);
    return failed_tests;
}

#if TEST_ENABLE

#if TEST_SELF_MAIN
TEST_CASE(test_module)
{
    int result = 2 + 2;
    TEST_ASSERT_EQUAL_INT(4, result);
}

#endif // TEST_SELF_MAIN

#endif // TEST_ENABLE
