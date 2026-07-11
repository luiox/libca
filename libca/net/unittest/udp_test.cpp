#include <gmock/gmock.h>

#include <array>
#include <chrono>
#include <string>
#include <type_traits>
#include <utility>

#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"
#include "libca/net/udp.hpp"

namespace ca::net::test {
namespace {

using namespace std::chrono_literals;

static_assert(!std::is_base_of_v<io::Reader, UdpSocket>);
static_assert(!std::is_base_of_v<io::Writer, UdpSocket>);

SocketAddress udp_loopback_address(u16 port = 0)
{
    return SocketAddress(IpAddress::localhost_v4(), port);
}

TEST(UdpSocketTest, SendsReceivesAndPreservesPeerAddress)
{
    auto first_result  = UdpSocket::bind(udp_loopback_address());
    auto second_result = UdpSocket::bind(udp_loopback_address());
    ASSERT_TRUE(first_result.is_ok()) << first_result.unwrap_err().to_string();
    ASSERT_TRUE(second_result.is_ok()) << second_result.unwrap_err().to_string();
    auto       first          = std::move(first_result).unwrap();
    auto       second         = std::move(second_result).unwrap();
    const auto first_address  = first.local_address().unwrap();
    const auto second_address = second.local_address().unwrap();

    const std::string payload = "datagram";
    auto              sent =
        first.send_to(reinterpret_cast<const u8*>(payload.data()), payload.size(), second_address);
    ASSERT_TRUE(sent.is_ok()) << sent.unwrap_err().to_string();
    EXPECT_EQ(sent.unwrap(), payload.size());

    std::array<u8, 32> buffer{};
    auto               received = second.receive_from(buffer.data(), buffer.size());
    ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
    EXPECT_EQ(received.unwrap().length, payload.size());
    EXPECT_EQ(received.unwrap().peer_address, first_address);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buffer.data()), payload.size()), payload);
}

TEST(UdpSocketTest, SupportsConnectedAndZeroLengthDatagrams)
{
    auto first_result  = UdpSocket::bind(udp_loopback_address());
    auto second_result = UdpSocket::bind(udp_loopback_address());
    ASSERT_TRUE(first_result.is_ok()) << first_result.unwrap_err().to_string();
    ASSERT_TRUE(second_result.is_ok()) << second_result.unwrap_err().to_string();
    auto       first          = std::move(first_result).unwrap();
    auto       second         = std::move(second_result).unwrap();
    const auto first_address  = first.local_address().unwrap();
    const auto second_address = second.local_address().unwrap();

    ASSERT_TRUE(first.connect(second_address).is_ok());
    ASSERT_TRUE(second.connect(first_address).is_ok());
    EXPECT_EQ(first.peer_address().unwrap(), second_address);
    EXPECT_EQ(second.peer_address().unwrap(), first_address);

    const u8 value[] = {1, 2, 3};
    EXPECT_EQ(first.send(value, sizeof(value)).unwrap(), sizeof(value));
    u8 output[3]{};
    EXPECT_EQ(second.receive(output, sizeof(output)).unwrap(), sizeof(output));
    EXPECT_THAT(output, ::testing::ElementsAre(1, 2, 3));

    EXPECT_EQ(first.send(nullptr, 0).unwrap(), 0U);
    u8 zero_buffer{};
    EXPECT_EQ(second.receive(&zero_buffer, 1).unwrap(), 0U);
}

TEST(UdpSocketTest, ConfiguresTimeoutBroadcastNonblockingAndClone)
{
    auto socket_result = UdpSocket::bind(udp_loopback_address());
    ASSERT_TRUE(socket_result.is_ok()) << socket_result.unwrap_err().to_string();
    auto socket = std::move(socket_result).unwrap();

    EXPECT_TRUE(socket.set_read_timeout(100ms).is_ok());
    EXPECT_TRUE(socket.set_write_timeout(200ms).is_ok());
    EXPECT_TRUE(socket.read_timeout().unwrap().has_value());
    EXPECT_TRUE(socket.write_timeout().unwrap().has_value());
    EXPECT_EQ(socket.set_read_timeout(0ms).unwrap_err().kind(), io::IoErrorKind::InvalidInput);
    EXPECT_TRUE(socket.set_read_timeout(std::nullopt).is_ok());

    EXPECT_TRUE(socket.set_broadcast(true).is_ok());
    EXPECT_TRUE(socket.broadcast().unwrap());
    EXPECT_TRUE(socket.set_nonblocking(true).is_ok());
    u8   value{};
    auto empty = socket.receive_from(&value, 1);
    ASSERT_TRUE(empty.is_err());
    EXPECT_EQ(empty.unwrap_err().kind(), io::IoErrorKind::WouldBlock);

    auto clone = socket.try_clone();
    ASSERT_TRUE(clone.is_ok()) << clone.unwrap_err().to_string();
    auto original  = socket.into_socket();
    auto rewrapped = UdpSocket::from_socket(std::move(original));
    ASSERT_TRUE(rewrapped.is_ok()) << rewrapped.unwrap_err().to_string();
    EXPECT_TRUE(std::move(rewrapped).unwrap().is_open());
    auto cloned_socket = std::move(clone).unwrap();
    EXPECT_TRUE(cloned_socket.is_open());
}

}   // namespace
}   // namespace ca::net::test
