#pragma once

#include <libca/core/platform.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace ca {

/// 打印当前调用栈到 stderr
///
/// Windows: 使用 CaptureStackBackTrace + SymFromAddr（需链接 dbghelp.lib）
/// Linux:   使用 backtrace + backtrace_symbols + __cxa_demangle
/// macOS:   暂不支持
inline void PrintStackTrace() {
#if CA_PLATFORM_WINDOWS
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
#elif CA_PLATFORM_LINUX
    void* array[10];
    size_t size;

    size = backtrace(array, 10);
    char** strings = backtrace_symbols(array, size);
    if (strings == nullptr) {
        std::cerr << "Error getting backtrace symbols" << std::endl;
        return;
    }

    for (size_t i = 0; i < size; i++) {
        std::cout << i << ": " << strings[i] << std::endl;
    }
    free(strings);
#else
    std::cerr << "PrintStackTrace: not supported on this platform" << std::endl;
#endif
}

} // namespace ca
