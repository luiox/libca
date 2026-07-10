#include "libca/core/dynamic_library.hpp"

#include <string>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <cerrno>
#    include <dlfcn.h>
#endif

namespace ca::core {
namespace {

#if defined(_WIN32)
StatusResult<std::wstring> utf8_to_utf16(const std::string& value)
{
    const auto length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length == 0) {
        return Err(
            ErrStatus(StatusCode::INVALID_ARGUMENT, "dynamic library path is not valid UTF-8"));
    }

    std::wstring converted(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            &converted[0],
                            length) == 0) {
        return Err(
            ErrStatus(StatusCode::INVALID_ARGUMENT, "dynamic library path is not valid UTF-8"));
    }
    return Ok(std::move(converted));
}

std::string windows_error_message(const char* operation, unsigned long error)
{
    return std::string(operation) + " failed with Windows error " + std::to_string(error);
}
#else
std::string loader_error_message(const char* operation, const char* error)
{
    if (error == nullptr) {
        return std::string(operation) + " failed";
    }
    return std::string(operation) + " failed: " + error;
}
#endif

}   // namespace

DynamicLibrary::DynamicLibrary(void* handle) noexcept
    : handle_(handle)
{}

DynamicLibrary::~DynamicLibrary()
{
    unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : handle_(other.handle_)
{
    other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept
{
    if (this != &other) {
        unload();
        handle_       = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

StatusResult<DynamicLibrary> DynamicLibrary::load(const std::string& path)
{
    if (path.empty()) {
        return Err(
            ErrStatus(StatusCode::INVALID_ARGUMENT, "dynamic library path must not be empty"));
    }

#if defined(_WIN32)
    auto wide_path = utf8_to_utf16(path);
    if (wide_path.is_err()) {
        return Err(wide_path.unwrap_err());
    }

    std::wstring library_path = std::move(wide_path).unwrap();
    HMODULE      handle       = LoadLibraryW(library_path.c_str());
    if (handle == nullptr) {
        const auto       error = static_cast<unsigned long>(GetLastError());
        const StatusCode code  = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
                                        error == ERROR_MOD_NOT_FOUND
                                     ? StatusCode::NOT_FOUND
                                     : StatusCode::INTERNAL;
        return Err(ErrStatus(code, windows_error_message("LoadLibraryW", error)));
    }
    return Ok(DynamicLibrary(reinterpret_cast<void*>(handle)));
#else
    dlerror();
    errno        = 0;
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        const char*      error = dlerror();
        const StatusCode code  = errno == ENOENT ? StatusCode::NOT_FOUND : StatusCode::INTERNAL;
        return Err(ErrStatus(code, loader_error_message("dlopen", error)));
    }
    return Ok(DynamicLibrary(handle));
#endif
}

StatusResult<void*> DynamicLibrary::lookup_raw(const std::string& symbol_name) const
{
    if (!is_loaded()) {
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION,
                             "cannot look up a symbol in an unloaded dynamic library"));
    }
    if (symbol_name.empty()) {
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT,
                             "dynamic library symbol name must not be empty"));
    }

#if defined(_WIN32)
    FARPROC symbol = GetProcAddress(reinterpret_cast<HMODULE>(handle_), symbol_name.c_str());
    if (symbol == nullptr) {
        const auto error = static_cast<unsigned long>(GetLastError());
        return Err(
            ErrStatus(StatusCode::NOT_FOUND, windows_error_message("GetProcAddress", error)));
    }
    return Ok(reinterpret_cast<void*>(symbol));
#else
    dlerror();
    void*       symbol = dlsym(handle_, symbol_name.c_str());
    const char* error  = dlerror();
    if (error != nullptr) {
        return Err(ErrStatus(StatusCode::NOT_FOUND, loader_error_message("dlsym", error)));
    }
    return Ok(symbol);
#endif
}

void DynamicLibrary::unload() noexcept
{
    if (!is_loaded()) {
        return;
    }

#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
}

bool DynamicLibrary::is_loaded() const noexcept
{
    return handle_ != nullptr;
}

}   // namespace ca::core
