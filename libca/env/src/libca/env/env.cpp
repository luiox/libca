#include "libca/env/env.hpp"

#include "libca/str/format.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
extern char** environ;
#endif

namespace ca::env {

namespace {

#if defined(_WIN32)

// UTF-8 <-> UTF-16 互转。Windows API 要求 UTF-16，对外接口统一 UTF-8。

std::wstring utf8_to_wide(std::string_view utf8)
{
    if (utf8.empty())
        return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8,
                                  0,
                                  utf8.data(),
                                  static_cast<int>(utf8.size()),
                                  nullptr,
                                  0);
    if (len <= 0)
        return std::wstring();
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        utf8.data(),
                        static_cast<int>(utf8.size()),
                        wide.data(),
                        len);
    return wide;
}

std::string wide_to_utf8(const wchar_t* wide, int len)
{
    if (wide == nullptr || len <= 0)
        return std::string();
    int out = WideCharToMultiByte(CP_UTF8,
                                  0,
                                  wide,
                                  len,
                                  nullptr,
                                  0,
                                  nullptr,
                                  nullptr);
    if (out <= 0)
        return std::string();
    std::string utf8(static_cast<size_t>(out), '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wide,
                        len,
                        utf8.data(),
                        out,
                        nullptr,
                        nullptr);
    return utf8;
}

std::string wide_to_utf8(const std::wstring& wide)
{
    return wide_to_utf8(wide.data(), static_cast<int>(wide.size()));
}

#endif  // _WIN32

}  // namespace

std::optional<std::string> get(std::string_view name)
{
#if defined(_WIN32)
    auto wide_name = utf8_to_wide(name);
    DWORD need = GetEnvironmentVariableW(wide_name.c_str(), nullptr, 0);
    if (need == 0)
        return std::nullopt;  // 不存在（或空值变量，二者难以区分，统一按不存在处理）
    std::wstring buffer(static_cast<size_t>(need), L'\0');
    DWORD actual = GetEnvironmentVariableW(wide_name.c_str(), buffer.data(), need);
    if (actual == 0)
        return std::nullopt;
    // actual 不含末尾 '\0'，截断到实际长度。
    buffer.resize(actual);
    return wide_to_utf8(buffer);
#else
    // 必须以 '\0' 结尾。name 一般来自字符串字面量或 std::string，拷一份。
    std::string key(name);
    const char* value = std::getenv(key.c_str());
    if (value == nullptr)
        return std::nullopt;
    return std::string(value);
#endif
}

bool set(std::string_view name, std::string_view value)
{
#if defined(_WIN32)
    auto wide_name  = utf8_to_wide(name);
    auto wide_value = utf8_to_wide(value);
    return SetEnvironmentVariableW(wide_name.c_str(), wide_value.c_str()) != 0;
#else
    std::string key(name);
    std::string val(value);
    // overwrite=1 表示覆盖已有值。
    return ::setenv(key.c_str(), val.c_str(), 1) == 0;
#endif
}

bool remove(std::string_view name)
{
#if defined(_WIN32)
    auto wide_name = utf8_to_wide(name);
    // SetEnvironmentVariableW(value=nullptr) 删除变量；变量不存在也返回 true。
    return SetEnvironmentVariableW(wide_name.c_str(), nullptr) != 0;
#else
    std::string key(name);
    return ::unsetenv(key.c_str()) == 0;
#endif
}

std::vector<std::pair<std::string, std::string>> all()
{
    std::vector<std::pair<std::string, std::string>> result;
#if defined(_WIN32)
    wchar_t* block = GetEnvironmentStringsW();
    if (block == nullptr)
        return result;

    const wchar_t* cursor = block;
    while (*cursor != L'\0') {
        std::wstring entry(cursor);
        cursor += entry.size() + 1;

        // 特殊处理：Windows 的 '=C:=C:' 这类驱动器当前目录条目以 '=' 开头，跳过。
        if (entry.empty() || entry[0] == L'=')
            continue;

        auto eq = entry.find(L'=');
        if (eq == std::wstring::npos)
            continue;
        std::string key   = wide_to_utf8(entry.substr(0, eq));
        std::string value = wide_to_utf8(entry.substr(eq + 1));
        result.emplace_back(std::move(key), std::move(value));
    }
    FreeEnvironmentStringsW(block);
#else
    if (environ == nullptr)
        return result;
    for (char** entry = environ; *entry != nullptr; ++entry) {
        std::string_view raw(*entry);
        auto eq = raw.find('=');
        if (eq == std::string_view::npos)
            continue;
        result.emplace_back(std::string(raw.substr(0, eq)),
                            std::string(raw.substr(eq + 1)));
    }
#endif
    return result;
}

std::string current_dir()
{
#if defined(_WIN32)
    DWORD need = GetCurrentDirectoryW(0, nullptr);
    if (need == 0)
        return std::string();
    std::wstring buffer(static_cast<size_t>(need), L'\0');
    DWORD actual = GetCurrentDirectoryW(need, buffer.data());
    if (actual == 0)
        return std::string();
    buffer.resize(actual);
    return wide_to_utf8(buffer);
#else
    char* cwd = getcwd(nullptr, 0);
    if (cwd == nullptr)
        return std::string();
    std::string result(cwd);
    free(cwd);
    return result;
#endif
}

bool set_current_dir(std::string_view path)
{
#if defined(_WIN32)
    auto wide = utf8_to_wide(path);
    return SetCurrentDirectoryW(wide.c_str()) != 0;
#else
    std::string p(path);
    return ::chdir(p.c_str()) == 0;
#endif
}

std::string temp_dir()
{
#if defined(_WIN32)
    DWORD need = GetTempPathW(0, nullptr);
    if (need == 0)
        return std::string();
    std::wstring buffer(static_cast<size_t>(need), L'\0');
    DWORD actual = GetTempPathW(need, buffer.data());
    if (actual == 0)
        return std::string();
    buffer.resize(actual);
    std::string result = wide_to_utf8(buffer);
    // 去掉末尾分隔符，统一对外接口。
    if (!result.empty() && (result.back() == '\\' || result.back() == '/'))
        result.pop_back();
    return result;
#else
    const char* tmp = std::getenv("TMPDIR");
    if (tmp == nullptr)
        tmp = std::getenv("TMP");
    if (tmp == nullptr)
        tmp = std::getenv("TEMP");
    if (tmp == nullptr)
        tmp = "/tmp";
    std::string result(tmp);
    // 去掉末尾分隔符，与 Windows 分支及 hpp 文档承诺（末尾不含分隔符）保持一致。
    while (!result.empty() && result.back() == '/')
        result.pop_back();
    return result;
#endif
}

std::string executable_path()
{
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD actual = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (actual == 0)
        return std::string();
    while (actual == buffer.size()) {  // 缓冲区不足，扩容重试
        buffer.resize(buffer.size() * 2);
        actual = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (actual == 0)
            return std::string();
    }
    buffer.resize(actual);
    return wide_to_utf8(buffer);
#elif defined(__linux__)
    std::string buffer(4096, '\0');
    ssize_t     len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (len <= 0)
        return std::string();
    buffer.resize(static_cast<size_t>(len));
    return buffer;
#else
    return std::string();
#endif
}

std::string os_name()
{
#if defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string os_version()
{
#if defined(_WIN32)
    // RtlGetVersion 比 GetVersionEx 更可靠（不受 manifest 兼容性声明影响）。
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll != nullptr) {
        typedef LONG(WINAPI * RtlGetVersionPtr)(OSVERSIONINFOW*);
        auto rtl_get_version = reinterpret_cast<RtlGetVersionPtr>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (rtl_get_version != nullptr && rtl_get_version(&info) == 0) {
            return ca::str::format_std("{}.{}.{}",
                                       info.dwMajorVersion,
                                       info.dwMinorVersion,
                                       info.dwBuildNumber);
        }
    }
    return std::string();
#elif defined(__linux__)
    // 解析 /etc/os-release 的 VERSION_ID（如 "22.04"）。
    std::FILE* fp = std::fopen("/etc/os-release", "r");
    if (fp == nullptr)
        return std::string();
    std::string version;
    char line[256];
    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        std::string_view raw(line);
        if (raw.substr(0, 11) == "VERSION_ID=") {
            auto value = raw.substr(11);
            // 去掉首尾引号与换行。
            if (!value.empty() && value.front() == '"')
                value.remove_prefix(1);
            if (!value.empty() && (value.back() == '"' || value.back() == '\n'))
                value.remove_suffix(1);
            version = std::string(value);
            break;
        }
    }
    std::fclose(fp);
    return version;
#else
    return std::string();
#endif
}

}  // namespace ca::env
