#pragma once

#include "datatype.hpp"
#include "platform.hpp"

/// @file stacktrace.hpp
/// @brief 调用栈捕获/打印工具。
/// @note **Experimental**：输出格式依赖 OS/编译器/符号信息/链接参数，适合调试和日志，
///       不要当作可解析协议或稳定业务数据；目标是在可用平台上尽量安全地返回字符串/打印。

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if CA_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <DbgHelp.h>
    #include <mutex>
    #ifdef _MSC_VER
    // MinGW gcc 不支持 pragma comment(lib)；链接由 core 的 xmake add_syslinks 承担。
    #pragma comment(lib, "dbghelp.lib")
    #endif
#elif CA_PLATFORM_LINUX
    #include <cxxabi.h>
    #include <execinfo.h>
#endif

namespace ca::core {

namespace detail { } // namespace detail

/// @brief 捕获当前调用栈，返回多行字符串（每行一个栈帧）。
/// @param max_frames 最大帧数，钳制到 [1, 256]。
inline std::string capture_stack_trace(i32 max_frames = 64) {
    if (max_frames <= 0) max_frames = 1;
    if (max_frames > 256) max_frames = 256;

    std::ostringstream oss;

#if CA_PLATFORM_WINDOWS
    std::vector<void*> frames(static_cast<usize>(max_frames));
    USHORT frame_count = CaptureStackBackTrace(
        0, static_cast<USHORT>(max_frames), frames.data(), nullptr);

    HANDLE process = GetCurrentProcess();
    static std::mutex dbghelp_mutex;
    std::lock_guard<std::mutex> lock(dbghelp_mutex);

    static const bool sym_initialized = []() {
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        return true;
    }();
    (void)sym_initialized;

    alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256 * sizeof(wchar_t)];
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
    sym->MaxNameLen = 255;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (USHORT i = 0; i < frame_count; i++) {
        DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
        if (SymFromAddr(process, addr, nullptr, sym)) {
            oss << "  " << i << ": " << sym->Name << " [0x" << addr << "]\n";
        } else {
            oss << "  " << i << ": [0x" << addr << "]\n";
        }
    }

#elif CA_PLATFORM_LINUX
    std::vector<void*> frames(static_cast<usize>(max_frames));
    int count = backtrace(frames.data(), max_frames);
    char** entries = backtrace_symbols(frames.data(), count);
    if (!entries) {
        oss << "  (backtrace_symbols failed)\n";
        return oss.str();
    }

    for (int i = 0; i < count; i++) {
        std::string line(entries[i]);
        // 解析 "./exe(_Z3foov+0x1c) [0x...]" 格式
        auto lp = line.find('(');
        auto pl = line.find('+', lp);
        auto rp = line.find(')', pl);
        if (lp != std::string::npos && pl != std::string::npos && rp != std::string::npos) {
            std::string mangled = line.substr(lp + 1, pl - lp - 1);
            std::string offset   = line.substr(pl, rp - pl);
            std::string rest     = line.substr(rp);

            int status = 0;
            char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
            if (status == 0 && demangled) {
                oss << "  " << i << ": " << demangled << offset << rest << "\n";
                std::free(demangled);
            } else {
                oss << "  " << i << ": " << entries[i] << "\n";
            }
        } else {
            oss << "  " << i << ": " << entries[i] << "\n";
        }
    }
    std::free(entries);

#else
    oss << "  (capture_stack_trace not supported on this platform)\n";
#endif

    return oss.str();
}

/// @brief 打印当前调用栈到 stderr。
inline void print_stack_trace(i32 max_frames = 64) {
    std::cerr << capture_stack_trace(max_frames);
}

} // namespace ca::core
