#include "libca/core/minidump.hpp"

#include "libca/core/platform.hpp"

#if CA_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <DbgHelp.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace ca::core {
namespace {

// 崩溃过滤器使用的栈缓冲大小：前缀 + "-<pid>-<epoch 毫秒>.dmp" 必须能完整放下。
constexpr usize kMaxPrefixLength = 900;
constexpr usize kPathBufferSize = 1024;

// 安装状态与配置。install/uninstall 是低频配置操作，不设计为与崩溃处理并发；
// 崩溃过滤器只读取 g_path_prefix 与 g_installed。
char g_path_prefix[kMaxPrefixLength + 1] = {};
std::atomic_bool g_installed{false};
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;

// UTF-8 → UTF-16（定长缓冲版）：异常上下文中不可依赖堆分配，输出写入调用方缓冲。
// 成功返回写入的宽字符数（含终止符），失败返回 0。
int utf8_to_utf16_fixed(const char* utf8, wchar_t* out, int out_capacity) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, out_capacity);
}

// 生成 "<prefix>-<pid>-<epoch 毫秒>.dmp"。时间戳取 epoch 毫秒：避免异常上下文中
// 的本地时间格式化，且可直接排序、天然抗同进程多次崩溃的同名碰撞。
void build_dump_path(char* out, usize capacity) {
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::snprintf(out, capacity, "%s-%lu-%lld.dmp",
                  g_path_prefix,
                  static_cast<unsigned long>(GetCurrentProcessId()),
                  static_cast<long long>(now_ms));
}

// 崩溃过滤器：写出 MiniDumpNormal 转储后终止进程（返回值语义见 install_crash_dump 说明）。
LONG WINAPI crash_dump_filter(EXCEPTION_POINTERS* exception_pointers) {
    char path[kPathBufferSize];
    wchar_t wide_path[kPathBufferSize];
    build_dump_path(path, kPathBufferSize);
    if (utf8_to_utf16_fixed(path, wide_path, kPathBufferSize) > 0) {
        HANDLE file = CreateFileW(wide_path, GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION exception_info;
            exception_info.ThreadId = GetCurrentThreadId();
            exception_info.ExceptionPointers = exception_pointers;
            exception_info.ClientPointers = FALSE;
            // dbghelp 非线程安全：此处运行在崩溃线程的异常上下文中，与业务线程
            // 天然串行，是 MiniDumpWriteDump 的安全调用方式；进程随即终止，
            // 不做多余清理。
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                              MiniDumpNormal, &exception_info, nullptr, nullptr);
            CloseHandle(file);
        }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// UTF-8 → UTF-16（堆分配版）：常规上下文使用，非法 UTF-8 返回空串。
std::wstring utf8_to_utf16(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           value.data(), static_cast<int>(value.size()),
                                           nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring converted(static_cast<usize>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            value.data(), static_cast<int>(value.size()),
                            &converted[0], length) <= 0) {
        return std::wstring();
    }
    return converted;
}

std::string windows_error_message(const char* operation, unsigned long error) {
    return std::string(operation) + " failed with Windows error " + std::to_string(error);
}

// 打开文件并写当前进程 dump；exception_info 为 nullptr 表示无异常记录（主动取证）。
Status write_current_process_dump(const wchar_t* wide_path,
                                  MINIDUMP_EXCEPTION_INFORMATION* exception_info) {
    HANDLE file = CreateFileW(wide_path, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const auto error = static_cast<unsigned long>(GetLastError());
        return ErrStatus(StatusCode::INTERNAL, windows_error_message("CreateFileW", error));
    }
    const BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                           MiniDumpNormal, exception_info, nullptr, nullptr);
    CloseHandle(file);
    if (!written) {
        const auto error = static_cast<unsigned long>(GetLastError());
        return ErrStatus(StatusCode::INTERNAL, windows_error_message("MiniDumpWriteDump", error));
    }
    return OkStatus();
}

} // namespace

Status install_crash_dump(const std::string& path_prefix) {
    if (path_prefix.empty()) {
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "crash dump path prefix must not be empty");
    }
    if (path_prefix.size() > kMaxPrefixLength) {
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "crash dump path prefix too long");
    }
    if (!g_installed.load(std::memory_order_acquire)) {
        g_previous_filter = SetUnhandledExceptionFilter(crash_dump_filter);
    }
    g_path_prefix[path_prefix.size()] = '\0';
    std::memcpy(g_path_prefix, path_prefix.c_str(), path_prefix.size());
    g_installed.store(true, std::memory_order_release);
    return OkStatus();
}

Status uninstall_crash_dump() {
    if (g_installed.load(std::memory_order_acquire)) {
        SetUnhandledExceptionFilter(g_previous_filter);
        g_previous_filter = nullptr;
        g_path_prefix[0] = '\0';
        g_installed.store(false, std::memory_order_release);
    }
    return OkStatus();
}

Status write_minidump(const std::string& path) {
    if (path.empty()) {
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "minidump path must not be empty");
    }
    const std::wstring wide_path = utf8_to_utf16(path);
    if (wide_path.empty()) {
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "minidump path is not valid UTF-8");
    }
    return write_current_process_dump(wide_path.c_str(), nullptr);
}

bool is_crash_dump_installed() {
    return g_installed.load(std::memory_order_acquire);
}

} // namespace ca::core

#else // 非 Windows：头文件可正常包含，运行时返回"不支持"错误。

namespace ca::core {

Status install_crash_dump(const std::string& path_prefix) {
    (void)path_prefix;
    return ErrStatus(StatusCode::UNIMPLEMENTED, "minidump is only supported on Windows");
}

Status uninstall_crash_dump() {
    return ErrStatus(StatusCode::UNIMPLEMENTED, "minidump is only supported on Windows");
}

Status write_minidump(const std::string& path) {
    (void)path;
    return ErrStatus(StatusCode::UNIMPLEMENTED, "minidump is only supported on Windows");
}

bool is_crash_dump_installed() {
    return false;
}

} // namespace ca::core

#endif
