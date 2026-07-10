#include "libca/io/native_stream.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <cerrno>
#    include <limits.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

namespace ca::io {
namespace {

IoError closed_stream_error(const char* operation)
{
    return IoError::from_kind(IoErrorKind::InvalidInput,
                              std::string(operation) + " on a closed NativeStream");
}

#if defined(_WIN32)
HANDLE to_handle(RawHandle handle) noexcept
{
    return reinterpret_cast<HANDLE>(handle);
}
#endif

}   // namespace

NativeStream::NativeStream(OwnedHandle handle) noexcept
    : handle_(std::move(handle))
{}

IoResult<usize> NativeStream::read(u8* buffer, usize capacity)
{
    if (capacity == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (buffer == nullptr)
        return ca::core::Err(IoError::from_kind(
            IoErrorKind::InvalidInput, "read buffer must not be null when capacity is nonzero"));
    if (!is_open())
        return ca::core::Err(closed_stream_error("read"));

#if defined(_WIN32)
    const DWORD request = static_cast<DWORD>(
        std::min<usize>(capacity, static_cast<usize>(std::numeric_limits<DWORD>::max())));
    DWORD count = 0;
    if (!ReadFile(to_handle(handle_.get()), buffer, request, &count, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE)
            return ca::core::Ok(static_cast<usize>(0));
        return ca::core::Err(IoError::last_os_error("ReadFile"));
    }
    return ca::core::Ok(static_cast<usize>(count));
#else
    const usize request =
        std::min<usize>(capacity, static_cast<usize>(std::numeric_limits<ssize_t>::max()));
    const ssize_t count = ::read(static_cast<int>(handle_.get()), buffer, request);
    if (count < 0)
        return ca::core::Err(IoError::last_os_error("read"));
    return ca::core::Ok(static_cast<usize>(count));
#endif
}

IoResult<usize> NativeStream::write(const u8* data, usize length)
{
    if (length == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (data == nullptr)
        return ca::core::Err(IoError::from_kind(
            IoErrorKind::InvalidInput, "write data must not be null when length is nonzero"));
    if (!is_open())
        return ca::core::Err(closed_stream_error("write"));

#if defined(_WIN32)
    const DWORD request = static_cast<DWORD>(
        std::min<usize>(length, static_cast<usize>(std::numeric_limits<DWORD>::max())));
    DWORD count = 0;
    if (!WriteFile(to_handle(handle_.get()), data, request, &count, nullptr))
        return ca::core::Err(IoError::last_os_error("WriteFile"));
    return ca::core::Ok(static_cast<usize>(count));
#else
    const usize request =
        std::min<usize>(length, static_cast<usize>(std::numeric_limits<ssize_t>::max()));
    const ssize_t count = ::write(static_cast<int>(handle_.get()), data, request);
    if (count < 0)
        return ca::core::Err(IoError::last_os_error("write"));
    return ca::core::Ok(static_cast<usize>(count));
#endif
}

IoResult<void> NativeStream::flush()
{
    if (!is_open())
        return ca::core::Err(closed_stream_error("flush"));
    return ca::core::Ok();
}

IoResult<u64> NativeStream::seek(const SeekFrom& position)
{
    if (!is_open())
        return ca::core::Err(closed_stream_error("seek"));

#if defined(_WIN32)
    SetLastError(ERROR_SUCCESS);
    const DWORD file_type = GetFileType(to_handle(handle_.get()));
    if (file_type == FILE_TYPE_UNKNOWN && GetLastError() != ERROR_SUCCESS)
        return ca::core::Err(IoError::last_os_error("GetFileType"));
    if (file_type != FILE_TYPE_DISK)
        return ca::core::Err(IoError::from_kind(IoErrorKind::Unsupported,
                                                "seek is only supported for disk handles"));

    LARGE_INTEGER distance{};
    DWORD         move_method = FILE_BEGIN;
    switch (position.origin()) {
    case SeekOrigin::Start:
        if (position.absolute_position() > static_cast<u64>(std::numeric_limits<LONGLONG>::max()))
            return ca::core::Err(IoError::from_kind(IoErrorKind::InvalidInput,
                                                    "seek position exceeds signed 64-bit range"));
        distance.QuadPart = static_cast<LONGLONG>(position.absolute_position());
        move_method       = FILE_BEGIN;
        break;
    case SeekOrigin::Current:
        distance.QuadPart = position.relative_offset();
        move_method       = FILE_CURRENT;
        break;
    case SeekOrigin::End:
        distance.QuadPart = position.relative_offset();
        move_method       = FILE_END;
        break;
    }
    LARGE_INTEGER result{};
    if (!SetFilePointerEx(to_handle(handle_.get()), distance, &result, move_method))
        return ca::core::Err(IoError::last_os_error("SetFilePointerEx"));
    if (result.QuadPart < 0)
        return ca::core::Err(IoError::from_kind(IoErrorKind::InvalidData,
                                                "seek returned a negative stream position"));
    return ca::core::Ok(static_cast<u64>(result.QuadPart));
#else
    int   origin = SEEK_SET;
    off_t offset = 0;
    switch (position.origin()) {
    case SeekOrigin::Start:
        if (position.absolute_position() > static_cast<u64>(std::numeric_limits<off_t>::max()))
            return ca::core::Err(IoError::from_kind(IoErrorKind::InvalidInput,
                                                    "seek position exceeds native off_t range"));
        offset = static_cast<off_t>(position.absolute_position());
        origin = SEEK_SET;
        break;
    case SeekOrigin::Current:
        offset = static_cast<off_t>(position.relative_offset());
        if (static_cast<i64>(offset) != position.relative_offset())
            return ca::core::Err(IoError::from_kind(IoErrorKind::InvalidInput,
                                                    "seek offset exceeds native off_t range"));
        origin = SEEK_CUR;
        break;
    case SeekOrigin::End:
        offset = static_cast<off_t>(position.relative_offset());
        if (static_cast<i64>(offset) != position.relative_offset())
            return ca::core::Err(IoError::from_kind(IoErrorKind::InvalidInput,
                                                    "seek offset exceeds native off_t range"));
        origin = SEEK_END;
        break;
    }
    const off_t result = ::lseek(static_cast<int>(handle_.get()), offset, origin);
    if (result < 0)
        return ca::core::Err(IoError::last_os_error("lseek"));
    return ca::core::Ok(static_cast<u64>(result));
#endif
}

bool NativeStream::is_open() const noexcept
{
    return handle_.is_valid();
}

RawHandle NativeStream::native_handle() const noexcept
{
    return handle_.get();
}

OwnedHandle& NativeStream::handle() noexcept
{
    return handle_;
}

const OwnedHandle& NativeStream::handle() const noexcept
{
    return handle_;
}

OwnedHandle NativeStream::into_handle() noexcept
{
    return std::move(handle_);
}

}   // namespace ca::io
