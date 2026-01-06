#include "compiler_helper.h"
#include "debug.h"

void print_compiler_info(void)
{
    ca_dprintf("Compiler: %s %d.%d\n", COMPILER_NAME, COMPILER_VERSION_MAJOR, COMPILER_VERSION_MINOR);
    ca_dprintf("C Standard: %s\n", C_STANDARD_NAME);
    ca_dprintf("C++ Standard: %s\n", CPP_STANDARD_NAME);
    ca_dprintf("Platform: %s\n", PLATFORM_NAME);
    ca_dprintf("Architecture: %s\n", ARCH_NAME);
}
