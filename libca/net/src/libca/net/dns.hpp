#pragma once

#include <string>
#include <vector>

#include "libca/net/address.hpp"
#include "libca/net/socket.hpp"

namespace ca::net {

/// @brief 基于 getaddrinfo() 的同步 DNS 解析器。
class DnsResolver
{
public:
    /// @brief 解析主机名并保持系统返回顺序，重复地址会被移除。
    static io::IoResult<std::vector<SocketAddress>> resolve(
        const std::string& host, u16 port, AddressFamily family = AddressFamily::Unspecified,
        SocketKind kind = SocketKind::Stream);
};

}   // namespace ca::net
