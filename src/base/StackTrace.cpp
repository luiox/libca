#include "StackTrace.hpp"

#if CA_PLATFORM_WINDOWS

#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace ca {

void PrintStackTrace() {
    void* stack[100];
    unsigned short frames;
    SYMBOL_INFO* symbol;
    HANDLE process;

    process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    frames = CaptureStackBackTrace(0, 100, stack, NULL);
    symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (unsigned int i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        std::wcout << i << L": " << symbol->Name << L" - 0x" << symbol->Address << std::endl;
    }

    free(symbol);
    SymCleanup(process);
}

#elif CA_PLATFORM_LINUX

#include <execinfo.h>
#include <cxxabi.h> // For __cxa_demangle
#include <iostream>
#include <cstdlib>
#include <memory>

void PrintStackTrace() {
    void* array[10];
    size_t size;

    // 获取调用栈
    size = backtrace(array, 10);

    // 转换地址为符号
    char** strings = backtrace_symbols(array, size);
    if (strings == nullptr) {
        std::cerr << "Error getting backtrace symbols" << std::endl;
        return;
    }

    // 打印调用栈
    for (size_t i = 0; i < size; i++) {
        std::cout << strings[i] << std::endl;

        // 可选：尝试对符号进行 demangle（仅适用于 C++）
        int status;
        char* demangled = abi::__cxa_demangle(strings[i], nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            std::cout << "Demangled: " << demangled << std::endl;
            free(demangled);
        }
    }

    free(strings);
}


#endif

}
