#include <gtest/gtest.h>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#    include <windows.h>
#else
#    include <cerrno>
#endif

#include <string>

#include "libca/io/error.hpp"
#include "libca/net/socket_error.hpp"

namespace ca::net::test {

// ==================== from_native：静态映射用例（平台门控） ====================

#if defined(_WIN32)
TEST(SocketErrorTest, FromNativeWindowsMappings)
{
    EXPECT_EQ(from_native(static_cast<i64>(WSAEWOULDBLOCK)), SocketError::WouldBlock);
    EXPECT_EQ(from_native(static_cast<i64>(WSAEINPROGRESS)), SocketError::WouldBlock);
    EXPECT_EQ(from_native(static_cast<i64>(WSAEALREADY)), SocketError::WouldBlock);
    EXPECT_EQ(from_native(static_cast<i64>(WSAETIMEDOUT)), SocketError::TimedOut);
    EXPECT_EQ(from_native(static_cast<i64>(WSAECONNREFUSED)), SocketError::ConnectionRefused);
    EXPECT_EQ(from_native(static_cast<i64>(WSAECONNRESET)), SocketError::ConnectionReset);
    EXPECT_EQ(from_native(static_cast<i64>(WSAECONNABORTED)), SocketError::ConnectionReset);
    EXPECT_EQ(from_native(static_cast<i64>(ERROR_BROKEN_PIPE)), SocketError::BrokenPipe);
    EXPECT_EQ(from_native(static_cast<i64>(WSAEADDRINUSE)), SocketError::AddrInUse);
    EXPECT_EQ(from_native(static_cast<i64>(WSAEACCES)), SocketError::AccessDenied);
    EXPECT_EQ(from_native(static_cast<i64>(ERROR_ACCESS_DENIED)), SocketError::AccessDenied);
    EXPECT_EQ(from_native(static_cast<i64>(WSAENOTCONN)), SocketError::NotConnected);
    EXPECT_EQ(from_native(static_cast<i64>(WSAENETUNREACH)), SocketError::NetworkUnreachable);
    EXPECT_EQ(from_native(static_cast<i64>(WSAEHOSTUNREACH)), SocketError::NetworkUnreachable);
    EXPECT_EQ(from_native(static_cast<i64>(ERROR_NETWORK_UNREACHABLE)),
              SocketError::NetworkUnreachable);
    EXPECT_EQ(from_native(static_cast<i64>(ERROR_HOST_UNREACHABLE)),
              SocketError::NetworkUnreachable);
}
#else
TEST(SocketErrorTest, FromNativePosixMappings)
{
    EXPECT_EQ(from_native(EWOULDBLOCK), SocketError::WouldBlock);
    EXPECT_EQ(from_native(EINPROGRESS), SocketError::WouldBlock);
    EXPECT_EQ(from_native(EALREADY), SocketError::WouldBlock);
    EXPECT_EQ(from_native(ETIMEDOUT), SocketError::TimedOut);
    EXPECT_EQ(from_native(ECONNREFUSED), SocketError::ConnectionRefused);
    EXPECT_EQ(from_native(ECONNRESET), SocketError::ConnectionReset);
    EXPECT_EQ(from_native(ECONNABORTED), SocketError::ConnectionReset);
    EXPECT_EQ(from_native(EPIPE), SocketError::BrokenPipe);
    EXPECT_EQ(from_native(EADDRINUSE), SocketError::AddrInUse);
    EXPECT_EQ(from_native(EACCES), SocketError::AccessDenied);
    EXPECT_EQ(from_native(EPERM), SocketError::AccessDenied);
    EXPECT_EQ(from_native(ENOTCONN), SocketError::NotConnected);
    EXPECT_EQ(from_native(ENETUNREACH), SocketError::NetworkUnreachable);
    EXPECT_EQ(from_native(EHOSTUNREACH), SocketError::NetworkUnreachable);
}
#endif

TEST(SocketErrorTest, FromNativeUnknownCodeFallsBack)
{
    // 不落入任何已知错误码空间的值归 Unknown
    EXPECT_EQ(from_native(0x7FFFFFFF), SocketError::Unknown);
}

// ==================== socket_error_name ====================

TEST(SocketErrorTest, NameIsStable)
{
    EXPECT_STREQ(socket_error_name(SocketError::WouldBlock), "WouldBlock");
    EXPECT_STREQ(socket_error_name(SocketError::TimedOut), "TimedOut");
    EXPECT_STREQ(socket_error_name(SocketError::ConnectionRefused), "ConnectionRefused");
    EXPECT_STREQ(socket_error_name(SocketError::ConnectionReset), "ConnectionReset");
    EXPECT_STREQ(socket_error_name(SocketError::BrokenPipe), "BrokenPipe");
    EXPECT_STREQ(socket_error_name(SocketError::AddrInUse), "AddrInUse");
    EXPECT_STREQ(socket_error_name(SocketError::AccessDenied), "AccessDenied");
    EXPECT_STREQ(socket_error_name(SocketError::NotConnected), "NotConnected");
    EXPECT_STREQ(socket_error_name(SocketError::NetworkUnreachable), "NetworkUnreachable");
    EXPECT_STREQ(socket_error_name(SocketError::Unknown), "Unknown");
}

// ==================== to_io_error_kind 桥接 ====================

TEST(SocketErrorTest, BridgeToIoErrorKind)
{
    EXPECT_EQ(to_io_error_kind(SocketError::WouldBlock), io::IoErrorKind::WouldBlock);
    EXPECT_EQ(to_io_error_kind(SocketError::TimedOut), io::IoErrorKind::TimedOut);
    EXPECT_EQ(to_io_error_kind(SocketError::ConnectionRefused), io::IoErrorKind::ConnectionRefused);
    EXPECT_EQ(to_io_error_kind(SocketError::ConnectionReset), io::IoErrorKind::ConnectionReset);
    EXPECT_EQ(to_io_error_kind(SocketError::BrokenPipe), io::IoErrorKind::BrokenPipe);
    EXPECT_EQ(to_io_error_kind(SocketError::AddrInUse), io::IoErrorKind::AddrInUse);
    // io 词表无 AccessDenied，取 PermissionDenied
    EXPECT_EQ(to_io_error_kind(SocketError::AccessDenied), io::IoErrorKind::PermissionDenied);
    EXPECT_EQ(to_io_error_kind(SocketError::NotConnected), io::IoErrorKind::NotConnected);
    // io 词表无 NetworkUnreachable，取语义最接近的 AddrNotAvailable
    EXPECT_EQ(to_io_error_kind(SocketError::NetworkUnreachable),
              io::IoErrorKind::AddrNotAvailable);
    EXPECT_EQ(to_io_error_kind(SocketError::Unknown), io::IoErrorKind::Other);
}

}  // namespace ca::net::test
