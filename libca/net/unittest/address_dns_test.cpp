#include <gmock/gmock.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "libca/net/dns.hpp"

namespace ca::net::test {
namespace {

TEST(IpAddressTest, ParsesAndFormatsIpv4AndIpv6)
{
    auto ipv4_result = IpAddress::parse("192.168.1.5");
    ASSERT_TRUE(ipv4_result.is_ok()) << ipv4_result.unwrap_err().to_string();
    auto ipv4 = ipv4_result.unwrap();
    EXPECT_TRUE(ipv4.is_ipv4());
    EXPECT_EQ(ipv4, IpAddress::v4(192, 168, 1, 5));
    EXPECT_EQ(ipv4.to_string(), "192.168.1.5");

    auto ipv6_result = IpAddress::parse("::1");
    ASSERT_TRUE(ipv6_result.is_ok()) << ipv6_result.unwrap_err().to_string();
    auto ipv6 = ipv6_result.unwrap();
    EXPECT_TRUE(ipv6.is_ipv6());
    EXPECT_EQ(ipv6, IpAddress::localhost_v6());
    EXPECT_EQ(ipv6.to_string(), "::1");
}

TEST(IpAddressTest, RejectsInvalidText)
{
    auto invalid = IpAddress::parse("300.1.2.3");

    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
}

TEST(SocketAddressTest, ParsesIpv4Ipv6AndScopeId)
{
    auto ipv4 = SocketAddress::parse("127.0.0.1:8080");
    ASSERT_TRUE(ipv4.is_ok()) << ipv4.unwrap_err().to_string();
    EXPECT_EQ(ipv4.unwrap().ip(), IpAddress::localhost_v4());
    EXPECT_EQ(ipv4.unwrap().port(), 8080);
    EXPECT_EQ(ipv4.unwrap().to_string(), "127.0.0.1:8080");

    auto ipv6 = SocketAddress::parse("[fe80::1%3]:443");
    ASSERT_TRUE(ipv6.is_ok()) << ipv6.unwrap_err().to_string();
    EXPECT_TRUE(ipv6.unwrap().ip().is_ipv6());
    EXPECT_EQ(ipv6.unwrap().scope_id(), 3U);
    EXPECT_EQ(ipv6.unwrap().port(), 443);
    EXPECT_EQ(ipv6.unwrap().to_string(), "[fe80::1%3]:443");
}

TEST(SocketAddressTest, RejectsAmbiguousOrOutOfRangeText)
{
    EXPECT_EQ(SocketAddress::parse("::1:80").unwrap_err().kind(), io::IoErrorKind::InvalidInput);
    EXPECT_EQ(SocketAddress::parse("127.0.0.1:65536").unwrap_err().kind(),
              io::IoErrorKind::InvalidInput);
    EXPECT_EQ(SocketAddress::parse("[::1]:abc").unwrap_err().kind(), io::IoErrorKind::InvalidInput);
    EXPECT_EQ(SocketAddress::parse("[127.0.0.1]:80").unwrap_err().kind(),
              io::IoErrorKind::InvalidInput);

    SocketAddress ipv4_with_ipv6_metadata(IpAddress::localhost_v4(), 80, 7, 9);
    EXPECT_EQ(ipv4_with_ipv6_metadata.flow_info(), 0U);
    EXPECT_EQ(ipv4_with_ipv6_metadata.scope_id(), 0U);
}

TEST(DnsResolverTest, ResolvesLocalhostAndHonorsIpv4Filter)
{
    auto all = DnsResolver::resolve("localhost", 32123);
    ASSERT_TRUE(all.is_ok()) << all.unwrap_err().to_string();
    auto all_addresses = std::move(all).unwrap();
    ASSERT_FALSE(all_addresses.empty());
    EXPECT_TRUE(std::all_of(all_addresses.begin(), all_addresses.end(), [](const auto& address) {
        return address.port() == 32123;
    }));

    auto ipv4 = DnsResolver::resolve("localhost", 80, AddressFamily::Ipv4, SocketKind::Stream);
    ASSERT_TRUE(ipv4.is_ok()) << ipv4.unwrap_err().to_string();
    auto ipv4_addresses = std::move(ipv4).unwrap();
    ASSERT_FALSE(ipv4_addresses.empty());
    EXPECT_TRUE(std::all_of(ipv4_addresses.begin(), ipv4_addresses.end(), [](const auto& address) {
        return address.ip().is_ipv4();
    }));
}

TEST(DnsResolverTest, RejectsEmptyHost)
{
    auto result = DnsResolver::resolve("", 80);

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
}

}   // namespace
}   // namespace ca::net::test
