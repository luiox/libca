#include "libca/net/udp.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "libca/net/detail/socket_platform.hpp"

namespace ca::net {
namespace {

usize maximum_datagram_length() noexcept
{
#if defined(_WIN32)
    return static_cast<usize>(std::numeric_limits<int>::max());
#else
    return static_cast<usize>(std::numeric_limits<ssize_t>::max());
#endif
}

int send_flags() noexcept
{
#if defined(MSG_NOSIGNAL)
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

u8* writable_datagram_buffer(u8* buffer, usize length) noexcept
{
    // send/recv 的缓冲区指针不能为 null，对零长度数据报用静态占位字节蒙混过去。
    static u8 empty_buffer = 0;
    return length == 0 ? &empty_buffer : buffer;
}

const u8* readable_datagram_buffer(const u8* data, usize length) noexcept
{
    static const u8 empty_buffer = 0;
    return length == 0 ? &empty_buffer : data;
}

}   // namespace

UdpSocket::UdpSocket(OwnedSocket socket) noexcept
    : socket_(std::move(socket))
{}

io::IoResult<UdpSocket> UdpSocket::from_socket(OwnedSocket socket)
{
    if (!socket.is_valid())
        return ca::core::Err(detail::closed_socket_error("UDP from_socket"));
    return ca::core::Ok(UdpSocket(std::move(socket)));
}

io::IoResult<UdpSocket> UdpSocket::bind(const SocketAddress& address)
{
    auto created = detail::create_socket(address.ip().version(), SOCK_DGRAM, IPPROTO_UDP);
    if (created.is_err())
        return ca::core::Err(created.unwrap_err());
    auto socket = std::move(created).unwrap();

    auto encoded = detail::encode_address(address);
    if (encoded.is_err())
        return ca::core::Err(encoded.unwrap_err());
    const auto native = encoded.unwrap();
    if (::bind(detail::to_native_socket(socket.get()),
               reinterpret_cast<const sockaddr*>(&native.storage),
               native.length) != 0)
        return ca::core::Err(detail::last_socket_error("bind UDP"));
    return ca::core::Ok(UdpSocket(std::move(socket)));
}

io::IoResult<void> UdpSocket::connect(const SocketAddress& address)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP connect"));
    auto encoded = detail::encode_address(address);
    if (encoded.is_err())
        return ca::core::Err(encoded.unwrap_err());
    const auto native = encoded.unwrap();
    if (::connect(detail::to_native_socket(socket_.get()),
                  reinterpret_cast<const sockaddr*>(&native.storage),
                  native.length) != 0)
        return ca::core::Err(detail::last_socket_error("connect UDP"));
    return ca::core::Ok();
}

io::IoResult<usize> UdpSocket::send(const u8* data, usize length)
{
    if (length != 0 && data == nullptr)
        return ca::core::Err(detail::invalid_buffer_error("UDP send"));
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP send"));
    if (length > maximum_datagram_length())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "UDP datagram exceeds the platform send length"));

    const usize request = length;
    const u8*   buffer  = readable_datagram_buffer(data, request);
#if defined(_WIN32)
    const int count = ::send(detail::to_native_socket(socket_.get()),
                             reinterpret_cast<const char*>(buffer),
                             static_cast<int>(request),
                             send_flags());
    if (count == SOCKET_ERROR)
#else
    const ssize_t count =
        ::send(detail::to_native_socket(socket_.get()), buffer, request, send_flags());
    if (count < 0)
#endif
        return ca::core::Err(detail::last_socket_error("send UDP"));
    return ca::core::Ok(static_cast<usize>(count));
}

io::IoResult<usize> UdpSocket::receive(u8* buffer, usize capacity)
{
    if (capacity != 0 && buffer == nullptr)
        return ca::core::Err(detail::invalid_buffer_error("UDP receive"));
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP receive"));

    const usize request = std::min(capacity, maximum_datagram_length());
    u8*         output  = writable_datagram_buffer(buffer, request);
#if defined(_WIN32)
    WSABUF native_buffer{};
    native_buffer.buf = reinterpret_cast<char*>(output);
    native_buffer.len = static_cast<ULONG>(request);
    DWORD count       = 0;
    DWORD flags       = 0;
    if (WSARecv(detail::to_native_socket(socket_.get()),
                &native_buffer,
                1,
                &count,
                &flags,
                nullptr,
                nullptr) == SOCKET_ERROR) {
        const i64 code = detail::last_socket_error_code();
        // Windows 收到超过缓冲区的报文时返回 WSAEMSGSIZE 但数据仍然投递（被截断）；
        // 此处忽略该错以匹配 POSIX recvfrom 的截断语义，返回实际读到的字节数。
        if (code != WSAEMSGSIZE)
            return ca::core::Err(detail::socket_error_from_code(code, "receive UDP"));
    }
#else
    const ssize_t count = ::recv(detail::to_native_socket(socket_.get()), output, request, 0);
    if (count < 0)
        return ca::core::Err(detail::last_socket_error("receive UDP"));
#endif
    return ca::core::Ok(static_cast<usize>(count));
}

io::IoResult<usize> UdpSocket::send_to(const u8* data, usize length, const SocketAddress& address)
{
    if (length != 0 && data == nullptr)
        return ca::core::Err(detail::invalid_buffer_error("UDP send_to"));
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP send_to"));
    if (length > maximum_datagram_length())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "UDP datagram exceeds the platform send length"));

    auto encoded = detail::encode_address(address);
    if (encoded.is_err())
        return ca::core::Err(encoded.unwrap_err());
    const auto  native  = encoded.unwrap();
    const usize request = length;
    const u8*   buffer  = readable_datagram_buffer(data, request);
#if defined(_WIN32)
    const int count = ::sendto(detail::to_native_socket(socket_.get()),
                               reinterpret_cast<const char*>(buffer),
                               static_cast<int>(request),
                               send_flags(),
                               reinterpret_cast<const sockaddr*>(&native.storage),
                               native.length);
    if (count == SOCKET_ERROR)
#else
    const ssize_t count = ::sendto(detail::to_native_socket(socket_.get()),
                                   buffer,
                                   request,
                                   send_flags(),
                                   reinterpret_cast<const sockaddr*>(&native.storage),
                                   native.length);
    if (count < 0)
#endif
        return ca::core::Err(detail::last_socket_error("sendto UDP"));
    return ca::core::Ok(static_cast<usize>(count));
}

io::IoResult<UdpReceiveResult> UdpSocket::receive_from(u8* buffer, usize capacity)
{
    if (capacity != 0 && buffer == nullptr)
        return ca::core::Err(detail::invalid_buffer_error("UDP receive_from"));
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP receive_from"));

    const usize                 request = std::min(capacity, maximum_datagram_length());
    u8*                         output  = writable_datagram_buffer(buffer, request);
    sockaddr_storage            storage{};
    detail::NativeAddressLength address_length =
        static_cast<detail::NativeAddressLength>(sizeof(storage));
#if defined(_WIN32)
    WSABUF native_buffer{};
    native_buffer.buf = reinterpret_cast<char*>(output);
    native_buffer.len = static_cast<ULONG>(request);
    DWORD count       = 0;
    DWORD flags       = 0;
    if (WSARecvFrom(detail::to_native_socket(socket_.get()),
                    &native_buffer,
                    1,
                    &count,
                    &flags,
                    reinterpret_cast<sockaddr*>(&storage),
                    &address_length,
                    nullptr,
                    nullptr) == SOCKET_ERROR) {
        const i64 code = detail::last_socket_error_code();
        if (code != WSAEMSGSIZE)
            return ca::core::Err(detail::socket_error_from_code(code, "recvfrom UDP"));
    }
#else
    const ssize_t count = ::recvfrom(detail::to_native_socket(socket_.get()),
                                     output,
                                     request,
                                     0,
                                     reinterpret_cast<sockaddr*>(&storage),
                                     &address_length);
    if (count < 0)
        return ca::core::Err(detail::last_socket_error("recvfrom UDP"));
#endif

    auto peer = detail::decode_address(reinterpret_cast<const sockaddr*>(&storage), address_length);
    if (peer.is_err())
        return ca::core::Err(peer.unwrap_err());
    return ca::core::Ok(UdpReceiveResult{static_cast<usize>(count), std::move(peer).unwrap()});
}

io::IoResult<SocketAddress> UdpSocket::local_address() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP local_address"));
    return detail::local_address(socket_.get());
}

io::IoResult<SocketAddress> UdpSocket::peer_address() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP peer_address"));
    return detail::peer_address(socket_.get());
}

io::IoResult<void> UdpSocket::set_nonblocking(bool enabled)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP set_nonblocking"));
    return detail::set_nonblocking(socket_.get(), enabled);
}

io::IoResult<void> UdpSocket::set_read_timeout(std::optional<std::chrono::milliseconds> timeout)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP set_read_timeout"));
    return detail::set_timeout(socket_.get(), SO_RCVTIMEO, timeout);
}

io::IoResult<void> UdpSocket::set_write_timeout(std::optional<std::chrono::milliseconds> timeout)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP set_write_timeout"));
    return detail::set_timeout(socket_.get(), SO_SNDTIMEO, timeout);
}

io::IoResult<std::optional<std::chrono::milliseconds>> UdpSocket::read_timeout() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP read_timeout"));
    return detail::get_timeout(socket_.get(), SO_RCVTIMEO);
}

io::IoResult<std::optional<std::chrono::milliseconds>> UdpSocket::write_timeout() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP write_timeout"));
    return detail::get_timeout(socket_.get(), SO_SNDTIMEO);
}

io::IoResult<void> UdpSocket::set_broadcast(bool enabled)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP set_broadcast"));
    return detail::set_bool_option(
        socket_.get(), SOL_SOCKET, SO_BROADCAST, enabled, "setsockopt(SO_BROADCAST)");
}

io::IoResult<bool> UdpSocket::broadcast() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("UDP broadcast"));
    return detail::get_bool_option(
        socket_.get(), SOL_SOCKET, SO_BROADCAST, "getsockopt(SO_BROADCAST)");
}

io::IoResult<UdpSocket> UdpSocket::try_clone() const
{
    auto duplicated = socket_.duplicate();
    if (duplicated.is_err())
        return ca::core::Err(duplicated.unwrap_err());
    return ca::core::Ok(UdpSocket(std::move(duplicated).unwrap()));
}

bool UdpSocket::is_open() const noexcept
{
    return socket_.is_valid();
}

RawSocket UdpSocket::native_socket() const noexcept
{
    return socket_.get();
}

OwnedSocket UdpSocket::into_socket() noexcept
{
    return std::move(socket_);
}

}   // namespace ca::net
