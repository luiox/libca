#include <gmock/gmock.h>

#include <chrono>
#include <string>
#include <vector>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <netinet/tcp.h>
#    include <sys/socket.h>
#endif

#include "libca/net/sock_util.hpp"
#include "libca/net/tcp.hpp"

namespace ca::net::test {
namespace {

using namespace std::chrono_literals;

TEST(SockUtilTest, GetLocalIpReturnsValidAddressAndFallsBackToLoopback)
{
    // 默认探测地址（8.8.8.8:80）版本：有网时返回出口 IP，无网时回落 127.0.0.1。
    // 两种环境都只断言"合法的 IPv4 地址文本"，不断言非 loopback，保证离线 CI 可过。
    const auto local = get_local_ip(AddressFamily::Ipv4);
    EXPECT_TRUE(local.is_ipv4());
    auto parsed = IpAddress::parse(local.to_string());
    ASSERT_TRUE(parsed.is_ok()) << local.to_string();
    EXPECT_EQ(parsed.unwrap(), local);

    // 指定探测 peer 的重载，并检查 IPv6 族回落 ::1 时依然是合法 IPv6 文本。
    const auto probe_peer_result = IpAddress::parse("8.8.8.8");
    ASSERT_TRUE(probe_peer_result.is_ok());
    const SocketAddress probe_peer(probe_peer_result.unwrap(), 80);
    const auto          via_peer = get_local_ip(probe_peer);
    EXPECT_TRUE(via_peer.is_ipv4());
    EXPECT_EQ(IpAddress::parse(via_peer.to_string()).unwrap(), via_peer);

    const auto local_v6 = get_local_ip(AddressFamily::Ipv6);
    EXPECT_TRUE(local_v6.is_ipv6());
    auto parsed_v6 = IpAddress::parse(local_v6.to_string());
    ASSERT_TRUE(parsed_v6.is_ok()) << local_v6.to_string();
    EXPECT_EQ(parsed_v6.unwrap(), local_v6);
}

TEST(SockUtilTest, KeepaliveRoundTripReadsBackWhatPlatformSupports)
{
    auto listener_result = TcpListener::bind(SocketAddress(IpAddress::localhost_v4(), 0));
    ASSERT_TRUE(listener_result.is_ok()) << listener_result.unwrap_err().to_string();
    auto listener      = std::move(listener_result).unwrap();
    auto client_result = TcpStream::connect(listener.local_address().unwrap());
    ASSERT_TRUE(client_result.is_ok()) << client_result.unwrap_err().to_string();
    auto client = std::move(client_result).unwrap();
    EXPECT_TRUE(listener.accept().is_ok());

    TcpKeepaliveConfig config;
    config.idle     = 10s;
    config.interval = 5s;
    config.count    = 3;
    auto set_result = client.set_keepalive(config);
    ASSERT_TRUE(set_result.is_ok()) << set_result.unwrap_err().to_string();

    auto enabled = client.keepalive_enabled();
    ASSERT_TRUE(enabled.is_ok()) << enabled.unwrap_err().to_string();
    EXPECT_TRUE(enabled.unwrap());

#if defined(_WIN32)
    // Windows 的 SIO_KEEPALIVE_VALS 只写不可读，idle/interval/count 无法回读；
    // 平台差异按编译期分支处理：这里只断言 SO_KEEPALIVE 开关，避免对拿不到的
    // 参数做误报断言。
#else
    // POSIX 三参数完整可回读，逐一与设置值比对。
    const auto native   = detail::to_native_socket(client.native_socket());
    int        idle     = 0;
    int        interval = 0;
    int        count    = 0;
    socklen_t  length   = static_cast<socklen_t>(sizeof(idle));
    ASSERT_EQ(getsockopt(native, IPPROTO_TCP, TCP_KEEPIDLE, &idle, &length), 0);
    length = static_cast<socklen_t>(sizeof(interval));
    ASSERT_EQ(getsockopt(native, IPPROTO_TCP, TCP_KEEPINTVL, &interval, &length), 0);
    length = static_cast<socklen_t>(sizeof(count));
    ASSERT_EQ(getsockopt(native, IPPROTO_TCP, TCP_KEEPCNT, &count, &length), 0);
    EXPECT_EQ(idle, 10);
    EXPECT_EQ(interval, 5);
    EXPECT_EQ(count, 3);
#endif
}

TEST(SockUtilTest, KeepaliveValidatesParameters)
{
    auto listener_result = TcpListener::bind(SocketAddress(IpAddress::localhost_v4(), 0));
    ASSERT_TRUE(listener_result.is_ok()) << listener_result.unwrap_err().to_string();
    auto listener      = std::move(listener_result).unwrap();
    auto client_result = TcpStream::connect(listener.local_address().unwrap());
    ASSERT_TRUE(client_result.is_ok()) << client_result.unwrap_err().to_string();
    auto client = std::move(client_result).unwrap();
    EXPECT_TRUE(listener.accept().is_ok());

    TcpKeepaliveConfig config;
    config.idle     = 10s;
    config.interval = 5s;
    config.count    = 3;

    config.idle    = 0s;
    auto zero_idle = client.set_keepalive(config);
    ASSERT_TRUE(zero_idle.is_err());
    EXPECT_EQ(zero_idle.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    config.idle        = 10s;
    config.interval    = 0s;
    auto zero_interval = client.set_keepalive(config);
    ASSERT_TRUE(zero_interval.is_err());
    EXPECT_EQ(zero_interval.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    config.interval = 5s;
    config.count    = 0;
    auto zero_count = client.set_keepalive(config);
    ASSERT_TRUE(zero_count.is_err());
    EXPECT_EQ(zero_count.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    // 已关闭的 stream 上操作返回错误而不是崩溃；恢复合法参数以隔离"关闭"这一失败原因。
    config.count       = 3;
    auto closed_stream = TcpStream::from_socket(client.into_socket()).unwrap();
    closed_stream.socket().close();
    auto on_closed = closed_stream.set_keepalive(config);
    ASSERT_TRUE(on_closed.is_err());
    EXPECT_EQ(on_closed.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
}

TEST(SockUtilTest, InterfaceListIsNotEmptyAndContainsLoopback)
{
    auto list_result = interface_list();
    ASSERT_TRUE(list_result.is_ok()) << list_result.unwrap_err().to_string();
    auto interfaces = std::move(list_result).unwrap();
    ASSERT_FALSE(interfaces.empty());

    bool has_loopback = false;
    for (const auto& entry : interfaces) {
        // 每个条目都必须带接口名与合法 IP（version 已由 IpAddress 表达）。
        EXPECT_FALSE(entry.name.empty());
        EXPECT_TRUE(entry.address.is_ipv4() || entry.address.is_ipv6());
        // 地址文本必须可再解析（合法格式校验）。
        auto parsed = IpAddress::parse(entry.address.to_string());
        ASSERT_TRUE(parsed.is_ok()) << entry.address.to_string();
        if (entry.is_loopback)
            has_loopback = true;
    }
    // 任何正常主机都至少有 loopback 接口。
    EXPECT_TRUE(has_loopback);
}

}   // namespace
}   // namespace ca::net::test
