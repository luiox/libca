#include <gmock/gmock.h>

#include <array>
#include <chrono>
#include <string>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <fcntl.h>
#endif

#include "libca/net/tcp.hpp"

namespace ca::net::test {
namespace {

using namespace std::chrono_literals;

static_assert(std::is_base_of_v<io::Reader, TcpStream>);
static_assert(std::is_base_of_v<io::Writer, TcpStream>);

SocketAddress loopback_address(u16 port = 0)
{
    return SocketAddress(IpAddress::localhost_v4(), port);
}

void expect_socket_not_inheritable(RawSocket socket)
{
#if defined(_WIN32)
    DWORD flags = 0;
    ASSERT_NE(GetHandleInformation(reinterpret_cast<HANDLE>(socket), &flags), 0);
    EXPECT_EQ(flags & HANDLE_FLAG_INHERIT, 0U);
#else
    const int flags = fcntl(static_cast<int>(socket), F_GETFD, 0);
    ASSERT_GE(flags, 0);
    EXPECT_NE(flags & FD_CLOEXEC, 0);
#endif
}

TEST(TcpTest, LoopbackStreamUsesReaderWriterAndReportsAddresses)
{
    auto listener_result = TcpListener::bind(loopback_address());
    ASSERT_TRUE(listener_result.is_ok()) << listener_result.unwrap_err().to_string();
    auto listener = std::move(listener_result).unwrap();
    expect_socket_not_inheritable(listener.native_socket());
    auto address = listener.local_address();
    ASSERT_TRUE(address.is_ok()) << address.unwrap_err().to_string();
    ASSERT_NE(address.unwrap().port(), 0);

    auto client_result = TcpStream::connect(address.unwrap());
    ASSERT_TRUE(client_result.is_ok()) << client_result.unwrap_err().to_string();
    auto client = std::move(client_result).unwrap();
    expect_socket_not_inheritable(client.native_socket());
    auto accepted_result = listener.accept();
    ASSERT_TRUE(accepted_result.is_ok()) << accepted_result.unwrap_err().to_string();
    auto accepted = std::move(accepted_result).unwrap();
    expect_socket_not_inheritable(accepted.stream.native_socket());

    const std::string request = "ping";
    ASSERT_TRUE(
        client.write_all(reinterpret_cast<const u8*>(request.data()), request.size()).is_ok());
    std::array<u8, 4> server_buffer{};
    ASSERT_TRUE(accepted.stream.read_exact(server_buffer.data(), server_buffer.size()).is_ok());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(server_buffer.data()), server_buffer.size()),
              request);

    const std::string response = "pong";
    ASSERT_TRUE(
        accepted.stream.write_all(reinterpret_cast<const u8*>(response.data()), response.size())
            .is_ok());
    std::array<u8, 4> client_buffer{};
    ASSERT_TRUE(client.read_exact(client_buffer.data(), client_buffer.size()).is_ok());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(client_buffer.data()), client_buffer.size()),
              response);

    EXPECT_EQ(client.peer_address().unwrap(), address.unwrap());
    EXPECT_EQ(accepted.peer_address.ip(), IpAddress::localhost_v4());
    EXPECT_TRUE(accepted.stream.shutdown(Shutdown::Write).is_ok());
    EXPECT_EQ(client.read(client_buffer.data(), client_buffer.size()).unwrap(), 0U);
}

TEST(TcpTest, HostnameConnectAndConnectTimeoutReachListener)
{
    auto listener_result = TcpListener::bind(loopback_address());
    ASSERT_TRUE(listener_result.is_ok()) << listener_result.unwrap_err().to_string();
    auto       listener = std::move(listener_result).unwrap();
    const auto address  = listener.local_address().unwrap();

    auto hostname_client = TcpStream::connect("localhost", address.port());
    ASSERT_TRUE(hostname_client.is_ok()) << hostname_client.unwrap_err().to_string();
    EXPECT_TRUE(listener.accept().is_ok());

    auto started        = std::chrono::steady_clock::now();
    auto timeout_client = TcpStream::connect_timeout(address, 2s);
    ASSERT_TRUE(timeout_client.is_ok()) << timeout_client.unwrap_err().to_string();
    EXPECT_LT(std::chrono::steady_clock::now() - started, 1s);
    EXPECT_TRUE(listener.accept().is_ok());

    started                      = std::chrono::steady_clock::now();
    auto hostname_timeout_client = TcpStream::connect_timeout("127.0.0.1", address.port(), 2s);
    ASSERT_TRUE(hostname_timeout_client.is_ok())
        << hostname_timeout_client.unwrap_err().to_string();
    EXPECT_LT(std::chrono::steady_clock::now() - started, 1s);
    EXPECT_TRUE(listener.accept().is_ok());

    auto invalid = TcpStream::connect_timeout(address, 0ms);
    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    auto too_large = TcpStream::connect_timeout(address, std::chrono::milliseconds::max());
    ASSERT_TRUE(too_large.is_err());
    EXPECT_EQ(too_large.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    auto invalid_host_timeout = TcpStream::connect_timeout("localhost", address.port(), 0ms);
    ASSERT_TRUE(invalid_host_timeout.is_err());
    EXPECT_EQ(invalid_host_timeout.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
}

TEST(TcpTest, ConfiguresTimeoutNodelayNonblockingAndClone)
{
    auto listener_result = TcpListener::bind(loopback_address());
    ASSERT_TRUE(listener_result.is_ok()) << listener_result.unwrap_err().to_string();
    auto       listener      = std::move(listener_result).unwrap();
    const auto address       = listener.local_address().unwrap();
    auto       client_result = TcpStream::connect(address);
    ASSERT_TRUE(client_result.is_ok()) << client_result.unwrap_err().to_string();
    auto client   = std::move(client_result).unwrap();
    auto accepted = listener.accept();
    ASSERT_TRUE(accepted.is_ok()) << accepted.unwrap_err().to_string();

    EXPECT_TRUE(client.set_read_timeout(125ms).is_ok());
    EXPECT_TRUE(client.set_write_timeout(250ms).is_ok());
    ASSERT_TRUE(client.read_timeout().unwrap().has_value());
    ASSERT_TRUE(client.write_timeout().unwrap().has_value());
    EXPECT_GT(client.read_timeout().unwrap()->count(), 0);
    EXPECT_GT(client.write_timeout().unwrap()->count(), 0);

    u8   timeout_buffer{};
    auto timed_read = client.read(&timeout_buffer, 1);
    ASSERT_TRUE(timed_read.is_err());
    const auto timeout_kind = timed_read.unwrap_err().kind();
    EXPECT_TRUE(timeout_kind == io::IoErrorKind::TimedOut ||
                timeout_kind == io::IoErrorKind::WouldBlock);

    EXPECT_TRUE(client.set_read_timeout(std::nullopt).is_ok());
    EXPECT_FALSE(client.read_timeout().unwrap().has_value());
    EXPECT_EQ(client.set_write_timeout(0ms).unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    EXPECT_TRUE(client.set_nodelay(true).is_ok());
    EXPECT_TRUE(client.nodelay().unwrap());
    EXPECT_TRUE(client.set_nonblocking(true).is_ok());
    EXPECT_TRUE(client.set_nonblocking(false).is_ok());

    auto clone = client.try_clone();
    ASSERT_TRUE(clone.is_ok()) << clone.unwrap_err().to_string();
    auto original_socket = client.into_socket();
    EXPECT_FALSE(client.is_open());
    auto rewrapped = TcpStream::from_socket(std::move(original_socket));
    ASSERT_TRUE(rewrapped.is_ok()) << rewrapped.unwrap_err().to_string();
    EXPECT_TRUE(std::move(rewrapped).unwrap().is_open());
    auto cloned_stream = std::move(clone).unwrap();
    EXPECT_TRUE(cloned_stream.is_open());
    expect_socket_not_inheritable(cloned_stream.native_socket());
}

TEST(TcpListenerTest, ValidatesBindOptionsAndClosesExplicitly)
{
    TcpListenerOptions invalid_backlog;
    invalid_backlog.backlog = 0;
    auto invalid            = TcpListener::bind(loopback_address(), invalid_backlog);
    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    TcpListenerOptions invalid_ipv6;
    invalid_ipv6.ipv6_only = true;
    auto wrong_family      = TcpListener::bind(loopback_address(), invalid_ipv6);
    ASSERT_TRUE(wrong_family.is_err());
    EXPECT_EQ(wrong_family.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    TcpListenerOptions options;
    options.backlog       = 16;
    options.reuse_address = true;
    auto bound            = TcpListener::bind(loopback_address(), options);
    ASSERT_TRUE(bound.is_ok()) << bound.unwrap_err().to_string();
    auto listener = std::move(bound).unwrap();
    EXPECT_TRUE(listener.is_open());
    expect_socket_not_inheritable(listener.native_socket());
    EXPECT_TRUE(listener.close().is_ok());
    EXPECT_TRUE(listener.close().is_ok());
    EXPECT_FALSE(listener.is_open());
}

TEST(TcpListenerTest, NonblockingAcceptReturnsWouldBlockAndCloneSurvivesClose)
{
    auto listener_result = TcpListener::bind(loopback_address());
    ASSERT_TRUE(listener_result.is_ok()) << listener_result.unwrap_err().to_string();
    auto listener = std::move(listener_result).unwrap();
    EXPECT_TRUE(listener.set_nonblocking(true).is_ok());

    auto empty = listener.accept();
    ASSERT_TRUE(empty.is_err());
    EXPECT_EQ(empty.unwrap_err().kind(), io::IoErrorKind::WouldBlock);

    auto clone_result = listener.try_clone();
    ASSERT_TRUE(clone_result.is_ok()) << clone_result.unwrap_err().to_string();
    auto clone  = std::move(clone_result).unwrap();
    auto socket = listener.into_socket();
    EXPECT_TRUE(socket.close().is_ok());
    EXPECT_TRUE(clone.local_address().is_ok());
}

TEST(OwnedSocketTest, RejectsInvalidSocket)
{
    auto invalid = OwnedSocket::adopt(invalid_raw_socket());

    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
}

}   // namespace
}   // namespace ca::net::test
