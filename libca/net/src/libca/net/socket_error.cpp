#include "libca/net/socket_error.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#    include <windows.h>
#else
#    include <cerrno>
#endif

namespace ca::net {

SocketError from_native(i64 native_code) noexcept
{
#if defined(_WIN32)
    switch (static_cast<DWORD>(native_code)) {
    case WSAEWOULDBLOCK:
    case WSAEINPROGRESS:
    case WSAEALREADY: return SocketError::WouldBlock;
    case WSAETIMEDOUT: return SocketError::TimedOut;
    case WSAECONNREFUSED: return SocketError::ConnectionRefused;
    case WSAECONNRESET:
    case WSAECONNABORTED:  // 连接中止与重置同属"已建立连接被中断"，最小集合内归并
        return SocketError::ConnectionReset;
    case ERROR_BROKEN_PIPE:
    case ERROR_PIPE_NOT_CONNECTED:
        // Win32 管道码与 WSA 码共用 GetLastError 参数空间，防御性兼容（语义等同 EPIPE）
        return SocketError::BrokenPipe;
    case WSAEADDRINUSE: return SocketError::AddrInUse;
    case WSAEACCES:
    case ERROR_ACCESS_DENIED: return SocketError::AccessDenied;
    case WSAENOTCONN: return SocketError::NotConnected;
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
    case ERROR_NETWORK_UNREACHABLE:
    case ERROR_HOST_UNREACHABLE: return SocketError::NetworkUnreachable;
    default: return SocketError::Unknown;
    }
#else
    switch (static_cast<int>(native_code)) {
    case ECONNREFUSED: return SocketError::ConnectionRefused;
    case ECONNRESET:
    case ECONNABORTED:  // 连接中止与重置同属"已建立连接被中断"，最小集合内归并
        return SocketError::ConnectionReset;
    case EPIPE: return SocketError::BrokenPipe;
    case EADDRINUSE: return SocketError::AddrInUse;
    case EACCES:
    case EPERM: return SocketError::AccessDenied;
    case ENOTCONN: return SocketError::NotConnected;
    case ETIMEDOUT: return SocketError::TimedOut;
    // Linux 上 EAGAIN 与 EWOULDBLOCK 同值，直接写两个 case 会重复标签编译失败，故用预处理守卫。
#    if EAGAIN != EWOULDBLOCK
    case EAGAIN:
#    endif
    case EWOULDBLOCK:
    case EINPROGRESS:
    case EALREADY: return SocketError::WouldBlock;
    case ENETUNREACH:
    case EHOSTUNREACH: return SocketError::NetworkUnreachable;
    default: return SocketError::Unknown;
    }
#endif
}

const char* socket_error_name(SocketError error) noexcept
{
    switch (error) {
    case SocketError::WouldBlock:         return "WouldBlock";
    case SocketError::TimedOut:           return "TimedOut";
    case SocketError::ConnectionRefused:  return "ConnectionRefused";
    case SocketError::ConnectionReset:    return "ConnectionReset";
    case SocketError::BrokenPipe:         return "BrokenPipe";
    case SocketError::AddrInUse:          return "AddrInUse";
    case SocketError::AccessDenied:       return "AccessDenied";
    case SocketError::NotConnected:       return "NotConnected";
    case SocketError::NetworkUnreachable: return "NetworkUnreachable";
    case SocketError::Unknown:            return "Unknown";
    }
    return "Unknown";
}

io::IoErrorKind to_io_error_kind(SocketError error) noexcept
{
    switch (error) {
    case SocketError::WouldBlock:         return io::IoErrorKind::WouldBlock;
    case SocketError::TimedOut:           return io::IoErrorKind::TimedOut;
    case SocketError::ConnectionRefused:  return io::IoErrorKind::ConnectionRefused;
    case SocketError::ConnectionReset:    return io::IoErrorKind::ConnectionReset;
    case SocketError::BrokenPipe:         return io::IoErrorKind::BrokenPipe;
    case SocketError::AddrInUse:          return io::IoErrorKind::AddrInUse;
    case SocketError::AccessDenied:       return io::IoErrorKind::PermissionDenied;
    case SocketError::NotConnected:       return io::IoErrorKind::NotConnected;
    case SocketError::NetworkUnreachable: return io::IoErrorKind::AddrNotAvailable;
    case SocketError::Unknown:            return io::IoErrorKind::Other;
    }
    return io::IoErrorKind::Other;
}

}  // namespace ca::net
