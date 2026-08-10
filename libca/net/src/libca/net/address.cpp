#include "libca/net/address.hpp"

#include "libca/str/format.hpp"

#include <charconv>
#include <cstring>
#include <system_error>
#include <utility>

#include "libca/net/detail/socket_platform.hpp"

namespace ca::net {
namespace {

io::IoResult<u16> parse_port(const std::string& value)
{
    if (value.empty())
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::InvalidInput, "socket port is empty"));

    u32        port   = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), port);
    if (result.ec != std::errc() || result.ptr != value.data() + value.size() || port > 65535)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "socket port must be an integer from 0 to 65535"));
    return ca::core::Ok(static_cast<u16>(port));
}

io::IoResult<u32> parse_scope_id(const std::string& value)
{
    if (value.empty())
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::InvalidInput, "IPv6 scope id is empty"));

    u32        scope_id = 0;
    const auto result   = std::from_chars(value.data(), value.data() + value.size(), scope_id);
    if (result.ec != std::errc() || result.ptr != value.data() + value.size())
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "IPv6 scope id must be an unsigned integer"));
    return ca::core::Ok(scope_id);
}

}   // namespace

IpAddress::IpAddress(IpVersion version, std::array<u8, 16> octets) noexcept
    : version_(version)
    , octets_(octets)
{}

IpAddress IpAddress::v4(u8 a, u8 b, u8 c, u8 d) noexcept
{
    std::array<u8, 16> octets{};
    octets[0] = a;
    octets[1] = b;
    octets[2] = c;
    octets[3] = d;
    return IpAddress(IpVersion::V4, octets);
}

IpAddress IpAddress::v6(const std::array<u8, 16>& octets) noexcept
{
    return IpAddress(IpVersion::V6, octets);
}

IpAddress IpAddress::localhost_v4() noexcept
{
    return v4(127, 0, 0, 1);
}

IpAddress IpAddress::localhost_v6() noexcept
{
    std::array<u8, 16> octets{};
    octets[15] = 1;
    return v6(octets);
}

IpAddress IpAddress::unspecified_v4() noexcept
{
    return v4(0, 0, 0, 0);
}

IpAddress IpAddress::unspecified_v6() noexcept
{
    return v6(std::array<u8, 16>{});
}

io::IoResult<IpAddress> IpAddress::parse(const std::string& value)
{
    if (value.empty())
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::InvalidInput, "IP address is empty"));

    auto runtime = detail::ensure_socket_runtime();
    if (runtime.is_err())
        return ca::core::Err(runtime.unwrap_err());

    std::array<u8, 16> octets{};
    if (inet_pton(AF_INET, value.c_str(), octets.data()) == 1)
        return ca::core::Ok(IpAddress(IpVersion::V4, octets));

    octets.fill(0);
    if (inet_pton(AF_INET6, value.c_str(), octets.data()) == 1)
        return ca::core::Ok(IpAddress(IpVersion::V6, octets));

    return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                ca::str::format_std("invalid IPv4 or IPv6 address: {}", value)));
}

IpVersion IpAddress::version() const noexcept
{
    return version_;
}

bool IpAddress::is_ipv4() const noexcept
{
    return version_ == IpVersion::V4;
}

bool IpAddress::is_ipv6() const noexcept
{
    return version_ == IpVersion::V6;
}

const std::array<u8, 16>& IpAddress::octets() const noexcept
{
    return octets_;
}

std::string IpAddress::to_string() const
{
    if (detail::ensure_socket_runtime().is_err())
        return {};

    char      buffer[INET6_ADDRSTRLEN]{};
    const int family = version_ == IpVersion::V4 ? AF_INET : AF_INET6;
    if (inet_ntop(family, octets_.data(), buffer, sizeof(buffer)) == nullptr)
        return {};
    return buffer;
}

bool operator==(const IpAddress& lhs, const IpAddress& rhs) noexcept
{
    return lhs.version() == rhs.version() && lhs.octets() == rhs.octets();
}

bool operator!=(const IpAddress& lhs, const IpAddress& rhs) noexcept
{
    return !(lhs == rhs);
}

SocketAddress::SocketAddress(IpAddress ip, u16 port, u32 flow_info, u32 scope_id) noexcept
    : ip_(std::move(ip))
    , port_(port)
    , flow_info_(ip_.is_ipv6() ? flow_info : 0)
    , scope_id_(ip_.is_ipv6() ? scope_id : 0)
{}

io::IoResult<SocketAddress> SocketAddress::parse(const std::string& value)
{
    if (value.empty())
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::InvalidInput, "socket address is empty"));

    std::string address_text;
    std::string port_text;
    u32         scope_id  = 0;
    bool        bracketed = false;

    if (value.front() == '[') {
        bracketed        = true;
        const auto close = value.find(']');
        if (close == std::string::npos || close + 1 >= value.size() || value[close + 1] != ':')
            return ca::core::Err(io::IoError::from_kind(
                io::IoErrorKind::InvalidInput, "IPv6 socket address must use [address]:port"));
        address_text = value.substr(1, close - 1);
        port_text    = value.substr(close + 2);

        const auto scope = address_text.rfind('%');
        if (scope != std::string::npos) {
            auto parsed_scope = parse_scope_id(address_text.substr(scope + 1));
            if (parsed_scope.is_err())
                return ca::core::Err(parsed_scope.unwrap_err());
            scope_id     = parsed_scope.unwrap();
            address_text = address_text.substr(0, scope);
        }
    }
    else {
        const auto separator = value.rfind(':');
        if (separator == std::string::npos || value.find(':') != separator)
            return ca::core::Err(io::IoError::from_kind(
                io::IoErrorKind::InvalidInput, "IPv4 socket address must use address:port"));
        address_text = value.substr(0, separator);
        port_text    = value.substr(separator + 1);
    }

    auto ip_result = IpAddress::parse(address_text);
    if (ip_result.is_err())
        return ca::core::Err(ip_result.unwrap_err());
    auto ip = std::move(ip_result).unwrap();
    if (bracketed && ip.is_ipv4())
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "bracketed socket address must contain IPv6"));
    if (scope_id != 0 && ip.is_ipv4())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "IPv4 socket address cannot contain a scope id"));

    auto port_result = parse_port(port_text);
    if (port_result.is_err())
        return ca::core::Err(port_result.unwrap_err());
    return ca::core::Ok(SocketAddress(std::move(ip), port_result.unwrap(), 0, scope_id));
}

const IpAddress& SocketAddress::ip() const noexcept
{
    return ip_;
}

u16 SocketAddress::port() const noexcept
{
    return port_;
}

u32 SocketAddress::flow_info() const noexcept
{
    return flow_info_;
}

u32 SocketAddress::scope_id() const noexcept
{
    return scope_id_;
}

std::string SocketAddress::to_string() const
{
    if (ip_.is_ipv4())
        return ca::str::format_std("{}:{}", ip_.to_string(), port_);

    if (scope_id_ != 0)
        return ca::str::format_std("[{}%{}]:{}", ip_.to_string(), scope_id_, port_);
    return ca::str::format_std("[{}]:{}", ip_.to_string(), port_);
}

bool operator==(const SocketAddress& lhs, const SocketAddress& rhs) noexcept
{
    return lhs.ip() == rhs.ip() && lhs.port() == rhs.port() && lhs.flow_info() == rhs.flow_info() &&
           lhs.scope_id() == rhs.scope_id();
}

bool operator!=(const SocketAddress& lhs, const SocketAddress& rhs) noexcept
{
    return !(lhs == rhs);
}

}   // namespace ca::net
