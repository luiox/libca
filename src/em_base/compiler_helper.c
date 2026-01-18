#include "compiler_helper.h"
#include "debug.h"

void print_compiler_info(void)
{
    debug_print("Compiler: %s %d.%d\n", COMPILER_NAME, COMPILER_VERSION_MAJOR, COMPILER_VERSION_MINOR);
    debug_print("C Standard: %s\n", C_STANDARD_NAME);
    debug_print("C++ Standard: %s\n", CPP_STANDARD_NAME);
    debug_print("Platform: %s\n", PLATFORM_NAME);
    debug_print("Architecture: %s\n", ARCH_NAME);
}
