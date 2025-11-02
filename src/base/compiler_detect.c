#include "compiler_detect.h"
#include <stdio.h>

void print_compiler_info()
{
    printf("Compiler: %s %d.%d\n", COMPILER_NAME, COMPILER_VERSION_MAJOR, COMPILER_VERSION_MINOR);
    printf("C Standard: %s\n", C_STANDARD_NAME);
    printf("C++ Standard: %s\n", CPP_STANDARD_NAME);
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Architecture: %s\n", ARCH_NAME);
}
