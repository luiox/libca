#include "Test.hpp"
#include <iostream>

struct SelfTestStaticInitPrinter {
    SelfTestStaticInitPrinter() {
        std::cout << "[self_test] self_test_main static init" << std::endl;
        FILE* f = fopen("self_test_main_init.txt", "w");
        if (f) { fputs("init", f); fclose(f); }
    }
} g_selfTestInitPrinter;

int main()
{
    std::cout << "[self_test] start" << std::endl;
    std::fflush(stdout);
    // debug: write a file so we know main was reached
    {
        FILE* f = fopen("self_test_started.txt", "w");
        if (f) {
            fputs("started", f);
            fclose(f);
        }
    }

    int fails = ca::test::runAllTestsAndGetFailCount();
    if (fails > 0) {
        std::cout << "[self_test] FAILS: " << fails << std::endl;
        return 1;
    }
    std::cout << "[self_test] OK" << std::endl;
    return 0;
}
