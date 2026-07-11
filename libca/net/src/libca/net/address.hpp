#pragma once

#include <array>
#include <string>

#include "libca/io/error.hpp"

namespace ca::net {

/// @brief IP 地址版本。
enum class IpVersion
{
    V4,
    V6
};

/// @brief IPv4 或 IPv6 地址值类型。
///
/// IPv4 使用 octets() 的前 4 字节，其余字节为 0。IPv6 使用全部 16 字节。
class IpAddress
{
public:
    /// @brief 创建 IPv4 地址。
    static IpAddress v4(u8 a, u8 b, u8 c, u8 d) noexcept;

    /// @brief 创建 IPv6 地址。
    static IpAddress v6(const std::array<u8, 16>& octets) noexcept;

    static IpAddress localhost_v4() noexcept;
    static IpAddress localhost_v6() noexcept;
    static IpAddress unspecified_v4() noexcept;
    static IpAddress unspecified_v6() noexcept;

    /// @brief 解析不带端口的 IPv4 或 IPv6 文本。
    static io::IoResult<IpAddress> parse(const std::string& value);

    IpVersion version() const noexcept;
    bool      is_ipv4() const noexcept;
    bool      is_ipv6() const noexcept;

    /// @brief 返回地址字节；IPv4 只使用前 4 字节。
    const std::array<u8, 16>& octets() const noexcept;

    /// @brief 返回规范化的 IP 地址文本。
    std::string to_string() const;

private:
    IpAddress(IpVersion version, std::array<u8, 16> octets) noexcept;

    IpVersion          version_{IpVersion::V4};
    std::array<u8, 16> octets_{};
};

bool operator==(const IpAddress& lhs, const IpAddress& rhs) noexcept;
bool operator!=(const IpAddress& lhs, const IpAddress& rhs) noexcept;

/// @brief IP 地址、端口和 IPv6 flow/scope 元数据的 socket 地址值类型。
class SocketAddress
{
public:
    SocketAddress(IpAddress ip, u16 port, u32 flow_info = 0, u32 scope_id = 0) noexcept;

    /// @brief 解析 IPv4 `address:port` 或 IPv6 `[address%scope]:port`。
    static io::IoResult<SocketAddress> parse(const std::string& value);

    const IpAddress& ip() const noexcept;
    u16              port() const noexcept;
    u32              flow_info() const noexcept;
    u32              scope_id() const noexcept;

    /// @brief 返回可再次 parse 的 socket 地址文本。
    std::string to_string() const;

private:
    IpAddress ip_;
    u16       port_{0};
    u32       flow_info_{0};
    u32       scope_id_{0};
};

bool operator==(const SocketAddress& lhs, const SocketAddress& rhs) noexcept;
bool operator!=(const SocketAddress& lhs, const SocketAddress& rhs) noexcept;

}   // namespace ca::net
