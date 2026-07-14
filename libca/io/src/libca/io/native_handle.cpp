#include "libca/io/native_handle.hpp"

#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <cerrno>
#    include <fcntl.h>
#    include <unistd.h>
#endif

namespace ca::io {
namespace {

#if defined(_WIN32)
HANDLE to_handle(RawHandle handle) noexcept
{
    return reinterpret_cast<HANDLE>(handle);
}

RawHandle from_handle(HANDLE handle) noexcept
{
    return reinterpret_cast<RawHandle>(handle);
}

bool is_valid_handle(RawHandle handle) noexcept
{
    // Windows 上无效句柄有两个哨兵：NULL(0) 和 INVALID_HANDLE_VALUE(-1)，
    // 不同 Win32 API 返回不同，需同时排除。
    return handle != 0 && handle != -1;
}
#else
bool is_valid_handle(RawHandle handle) noexcept
{
    return handle >= 0;
}
#endif

}   // namespace

OwnedHandle::OwnedHandle(RawHandle handle) noexcept
    : handle_(handle)
{}

OwnedHandle::OwnedHandle(OwnedHandle&& other) noexcept
    : handle_(other.release())
{}

OwnedHandle& OwnedHandle::operator=(OwnedHandle&& other) noexcept
{
    if (this != &other) {
        close_noexcept();
        handle_ = other.release();
    }
    return *this;
}

OwnedHandle::~OwnedHandle()
{
    close_noexcept();
}

IoResult<OwnedHandle> OwnedHandle::adopt(RawHandle handle)
{
    if (!is_valid_handle(handle))
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::InvalidInput, "cannot adopt an invalid native handle"));
    return ca::core::Ok(OwnedHandle(handle));
}

IoResult<OwnedHandle> OwnedHandle::duplicate() const
{
    if (!is_valid())
        return ca::core::Err(IoError::from_kind(IoErrorKind::InvalidInput,
                                                "cannot duplicate an empty native handle"));
#if defined(_WIN32)
    HANDLE duplicate_handle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(),
                         to_handle(handle_),
                         GetCurrentProcess(),
                         &duplicate_handle,
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS))
        return ca::core::Err(IoError::last_os_error("DuplicateHandle"));
    return ca::core::Ok(OwnedHandle(from_handle(duplicate_handle)));
#else
    int duplicate_fd = -1;
#    if defined(F_DUPFD_CLOEXEC)
    duplicate_fd = fcntl(static_cast<int>(handle_), F_DUPFD_CLOEXEC, 0);
#    else
    duplicate_fd = ::dup(static_cast<int>(handle_));
    if (duplicate_fd >= 0 && fcntl(duplicate_fd, F_SETFD, FD_CLOEXEC) != 0) {
        const auto error = IoError::last_os_error("fcntl");
        ::close(duplicate_fd);
        return ca::core::Err(error);
    }
#    endif
    if (duplicate_fd < 0)
        return ca::core::Err(IoError::last_os_error("duplicate fd"));
    return ca::core::Ok(OwnedHandle(static_cast<RawHandle>(duplicate_fd)));
#endif
}

bool OwnedHandle::is_valid() const noexcept
{
    return is_valid_handle(handle_);
}

RawHandle OwnedHandle::get() const noexcept
{
    return handle_;
}

IoResult<void> OwnedHandle::close()
{
    if (!is_valid())
        return ca::core::Ok();
    const RawHandle closing = release();
#if defined(_WIN32)
    if (!CloseHandle(to_handle(closing)))
        return ca::core::Err(IoError::last_os_error("CloseHandle"));
#else
    if (::close(static_cast<int>(closing)) != 0)
        return ca::core::Err(IoError::last_os_error("close"));
#endif
    return ca::core::Ok();
}

RawHandle OwnedHandle::release() noexcept
{
    const RawHandle released = handle_;
    handle_                  = -1;
    return released;
}

void OwnedHandle::close_noexcept() noexcept
{
    if (!is_valid())
        return;
    const RawHandle closing = release();
#if defined(_WIN32)
    CloseHandle(to_handle(closing));
#else
    ::close(static_cast<int>(closing));
#endif
}

}   // namespace ca::io
