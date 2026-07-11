#pragma once

#include <chrono>
#include <optional>

#include "libca/net/address.hpp"
#include "libca/net/socket.hpp"

namespace ca::net {

/// @brief UdpSocket::receive_from() 返回的数据报长度和对端地址。
struct UdpReceiveResult
{
    usize         length{0};
    SocketAddress peer_address;
};

/// @brief 保留消息边界的同步 UDP socket。
///
/// UDP 不是连续字节流，因此本类型不实现 io::Reader / io::Writer。
class UdpSocket
{
public:
    /// @brief 接管外部 UDP socket。
    static io::IoResult<UdpSocket> from_socket(OwnedSocket socket);
    static io::IoResult<UdpSocket> bind(const SocketAddress& address);

    UdpSocket(const UdpSocket&)                = delete;
    UdpSocket& operator=(const UdpSocket&)     = delete;
    UdpSocket(UdpSocket&&) noexcept            = default;
    UdpSocket& operator=(UdpSocket&&) noexcept = default;
    ~UdpSocket()                               = default;

    io::IoResult<void>  connect(const SocketAddress& address);
    io::IoResult<usize> send(const u8* data, usize length);
    io::IoResult<usize> receive(u8* buffer, usize capacity);
    io::IoResult<usize> send_to(const u8* data, usize length, const SocketAddress& address);
    io::IoResult<UdpReceiveResult> receive_from(u8* buffer, usize capacity);

    io::IoResult<SocketAddress> local_address() const;
    io::IoResult<SocketAddress> peer_address() const;
    io::IoResult<void>          set_nonblocking(bool enabled);
    io::IoResult<void>          set_read_timeout(std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<void>          set_write_timeout(std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<std::optional<std::chrono::milliseconds>> read_timeout() const;
    io::IoResult<std::optional<std::chrono::milliseconds>> write_timeout() const;
    io::IoResult<void>                                     set_broadcast(bool enabled);
    io::IoResult<bool>                                     broadcast() const;
    io::IoResult<UdpSocket>                                try_clone() const;

    bool        is_open() const noexcept;
    RawSocket   native_socket() const noexcept;
    OwnedSocket into_socket() noexcept;

private:
    explicit UdpSocket(OwnedSocket socket) noexcept;

    OwnedSocket socket_;
};

}   // namespace ca::net
