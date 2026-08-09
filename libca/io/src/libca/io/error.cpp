#include "libca/io/error.hpp"

#include "libca/str/format.hpp"

#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#    include <windows.h>
#else
#    include <cerrno>
#    include <cstring>
#endif

namespace ca::io {
namespace {

#if defined(_WIN32)
// Windows 原生错误码 → IoErrorKind。同时匹配 ERROR_*（Win32，来自 GetLastError）
// 与 WSA_*（Winsock，来自 WSAGetLastError）两套常量，因为二者共用错误码空间，
// 调用方可能经任一接口拿到错误。
IoErrorKind classify_native_error(i64 native_code) noexcept
{
    switch (static_cast<DWORD>(native_code)) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return IoErrorKind::NotFound;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case WSAEACCES: return IoErrorKind::PermissionDenied;
    case ERROR_CONNECTION_REFUSED:
    case WSAECONNREFUSED: return IoErrorKind::ConnectionRefused;
    case ERROR_CONNECTION_ABORTED:
    case WSAECONNABORTED: return IoErrorKind::ConnectionAborted;
    case ERROR_NETNAME_DELETED:
    case WSAECONNRESET: return IoErrorKind::ConnectionReset;
    case ERROR_NOT_CONNECTED:
    case WSAENOTCONN: return IoErrorKind::NotConnected;
    case ERROR_ADDRESS_ALREADY_ASSOCIATED:
    case WSAEADDRINUSE: return IoErrorKind::AddrInUse;
    case ERROR_INVALID_ADDRESS:
    case WSAEADDRNOTAVAIL: return IoErrorKind::AddrNotAvailable;
    // 管道读端遇到 ERROR_BROKEN_PIPE 表示对端写端已关闭（语义等同 POSIX EPIPE / EOF）。
    case ERROR_BROKEN_PIPE:
    case ERROR_PIPE_NOT_CONNECTED: return IoErrorKind::BrokenPipe;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS: return IoErrorKind::AlreadyExists;
    // ERROR_IO_PENDING 表示重叠 I/O 尚未完成（异步），WSAEWOULDBLOCK 表示非阻塞 socket
    // 暂不可用；二者都映射为 WouldBlock 让调用方重试。
    case ERROR_IO_PENDING:
    case WSAEWOULDBLOCK: return IoErrorKind::WouldBlock;
    case ERROR_INVALID_HANDLE:
    case ERROR_INVALID_PARAMETER: return IoErrorKind::InvalidInput;
    case ERROR_SEM_TIMEOUT:
    case ERROR_TIMEOUT:
    case WSAETIMEDOUT: return IoErrorKind::TimedOut;
    case ERROR_OPERATION_ABORTED:
    case WSAEINTR: return IoErrorKind::Interrupted;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY: return IoErrorKind::OutOfMemory;
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED: return IoErrorKind::Unsupported;
    default: return IoErrorKind::Other;
    }
}

std::string native_error_message(i64 native_code)
{
    char*       message = nullptr;
    const DWORD length  = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(native_code),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&message),
        0,
        nullptr);
    if (length == 0 || message == nullptr)
        return ca::str::format_std("Windows error {}", native_code);

    std::string result(message, length);
    LocalFree(message);
    while (!result.empty() &&
           (result.back() == '\r' || result.back() == '\n' || result.back() == ' '))
        result.pop_back();
    return result;
}
#else
IoErrorKind classify_native_error(i64 native_code) noexcept
{
    switch (static_cast<int>(native_code)) {
    case ENOENT: return IoErrorKind::NotFound;
    case EACCES:
    case EPERM: return IoErrorKind::PermissionDenied;
    case ECONNREFUSED: return IoErrorKind::ConnectionRefused;
    case ECONNRESET: return IoErrorKind::ConnectionReset;
    case ECONNABORTED: return IoErrorKind::ConnectionAborted;
    case ENOTCONN: return IoErrorKind::NotConnected;
    case EADDRINUSE: return IoErrorKind::AddrInUse;
    case EADDRNOTAVAIL: return IoErrorKind::AddrNotAvailable;
    case EPIPE: return IoErrorKind::BrokenPipe;
    case EEXIST: return IoErrorKind::AlreadyExists;
    // Linux 上 EAGAIN 与 EWOULDBLOCK 同值，直接写两个 case 会重复标签编译失败，故用预处理守卫。
#    if EAGAIN != EWOULDBLOCK
    case EAGAIN:
#    endif
    case EWOULDBLOCK: return IoErrorKind::WouldBlock;
    case EINVAL: return IoErrorKind::InvalidInput;
    case ETIMEDOUT: return IoErrorKind::TimedOut;
    case EINTR: return IoErrorKind::Interrupted;
    case ENOMEM: return IoErrorKind::OutOfMemory;
    case ESPIPE:
#    ifdef ENOTSUP
    case ENOTSUP:
#    endif
        return IoErrorKind::Unsupported;
    default: return IoErrorKind::Other;
    }
}

std::string native_error_message(i64 native_code)
{
    return std::strerror(static_cast<int>(native_code));
}
#endif

ca::core::StatusCode status_code_for(IoErrorKind kind) noexcept
{
    // 瞬时类 I/O 错误（WouldBlock、连接中断、管道断裂等）统一归到 UNAVAILABLE，
    // 对齐 gRPC 重试语义，与永久性的 InvalidInput/NotFound 区分，便于上层决定是否重试。
    using ca::core::StatusCode;
    switch (kind) {
    case IoErrorKind::NotFound: return StatusCode::NOT_FOUND;
    case IoErrorKind::PermissionDenied: return StatusCode::PERMISSION_DENIED;
    case IoErrorKind::AlreadyExists:
    case IoErrorKind::AddrInUse: return StatusCode::ALREADY_EXISTS;
    case IoErrorKind::WouldBlock:
    case IoErrorKind::ConnectionRefused:
    case IoErrorKind::ConnectionReset:
    case IoErrorKind::ConnectionAborted:
    case IoErrorKind::NotConnected:
    case IoErrorKind::AddrNotAvailable:
    case IoErrorKind::BrokenPipe: return StatusCode::UNAVAILABLE;
    case IoErrorKind::InvalidInput: return StatusCode::INVALID_ARGUMENT;
    case IoErrorKind::TimedOut: return StatusCode::DEADLINE_EXCEEDED;
    case IoErrorKind::Interrupted: return StatusCode::ABORTED;
    case IoErrorKind::UnexpectedEof:
    case IoErrorKind::InvalidData:
    case IoErrorKind::WriteZero: return StatusCode::DATA_LOSS;
    case IoErrorKind::OutOfMemory: return StatusCode::RESOURCE_EXHAUSTED;
    case IoErrorKind::Unsupported: return StatusCode::UNIMPLEMENTED;
    case IoErrorKind::Other: return StatusCode::INTERNAL;
    }
    return StatusCode::INTERNAL;
}

}   // namespace

const char* io_error_kind_name(IoErrorKind kind) noexcept
{
    switch (kind) {
    case IoErrorKind::NotFound: return "NotFound";
    case IoErrorKind::PermissionDenied: return "PermissionDenied";
    case IoErrorKind::ConnectionRefused: return "ConnectionRefused";
    case IoErrorKind::ConnectionReset: return "ConnectionReset";
    case IoErrorKind::ConnectionAborted: return "ConnectionAborted";
    case IoErrorKind::NotConnected: return "NotConnected";
    case IoErrorKind::AddrInUse: return "AddrInUse";
    case IoErrorKind::AddrNotAvailable: return "AddrNotAvailable";
    case IoErrorKind::BrokenPipe: return "BrokenPipe";
    case IoErrorKind::AlreadyExists: return "AlreadyExists";
    case IoErrorKind::WouldBlock: return "WouldBlock";
    case IoErrorKind::InvalidInput: return "InvalidInput";
    case IoErrorKind::InvalidData: return "InvalidData";
    case IoErrorKind::TimedOut: return "TimedOut";
    case IoErrorKind::WriteZero: return "WriteZero";
    case IoErrorKind::Interrupted: return "Interrupted";
    case IoErrorKind::UnexpectedEof: return "UnexpectedEof";
    case IoErrorKind::OutOfMemory: return "OutOfMemory";
    case IoErrorKind::Unsupported: return "Unsupported";
    case IoErrorKind::Other: return "Other";
    }
    return "Other";
}

IoError::IoError(IoErrorKind kind, i64 native_code, std::string message) noexcept
    : kind_(kind)
    , native_code_(native_code)
    , message_(std::move(message))
{}

IoError IoError::from_kind(IoErrorKind kind, std::string message)
{
    return IoError(kind, 0, std::move(message));
}

IoError IoError::from_native_error(i64 native_code, std::string operation)
{
    std::string message = native_error_message(native_code);
    if (!operation.empty())
        message = ca::str::format_std("{} failed: {}", operation, message);
    return IoError(classify_native_error(native_code), native_code, std::move(message));
}

IoError IoError::last_os_error(std::string operation)
{
#if defined(_WIN32)
    return from_native_error(static_cast<i64>(GetLastError()), std::move(operation));
#else
    return from_native_error(static_cast<i64>(errno), std::move(operation));
#endif
}

IoErrorKind IoError::kind() const noexcept
{
    return kind_;
}

i64 IoError::native_code() const noexcept
{
    return native_code_;
}

const std::string& IoError::message() const noexcept
{
    return message_;
}

std::string IoError::to_string() const
{
    std::string output;
    ca::str::format_to(output, "{}", io_error_kind_name(kind_));
    if (native_code_ != 0)
        ca::str::format_to(output, " (native {})", native_code_);
    if (!message_.empty())
        ca::str::format_to(output, ": {}", message_);
    return output;
}

ca::core::Status IoError::to_status() const
{
    return ca::core::ErrStatus(status_code_for(kind_), to_string());
}

}   // namespace ca::io
