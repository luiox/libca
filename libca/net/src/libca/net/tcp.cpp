#include "libca/net/tcp.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <limits>
#include <utility>

#if !defined(_WIN32)
#    include <fcntl.h>
#    include <netinet/tcp.h>
#    include <unistd.h>
#endif

#include "libca/net/detail/socket_platform.hpp"
#include "libca/net/dns.hpp"

namespace ca::net {
namespace {

usize maximum_io_length() noexcept
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

io::IoResult<void> configure_stream_socket(RawSocket socket)
{
    // 禁 SIGPIPE 双保险：send_flags() 里 Linux 用 MSG_NOSIGNAL，macOS/BSD 用 socket 选项
    // SO_NOSIGPIPE；Windows 不产生 SIGPIPE 故无操作。
#if defined(SO_NOSIGPIPE)
    return detail::set_bool_option(
        socket, SOL_SOCKET, SO_NOSIGPIPE, true, "setsockopt(SO_NOSIGPIPE)");
#else
    static_cast<void>(socket);
    return ca::core::Ok();
#endif
}

bool connect_is_in_progress(i64 code) noexcept
{
#if defined(_WIN32)
    // 必须把 WSAEINVAL 算作"连接进行中"——某些 WinSock 版本在非阻塞 socket 首次 connect
    // 时返回 WSAEINVAL 而非 WSAEWOULDBLOCK。
    return code == WSAEWOULDBLOCK || code == WSAEINPROGRESS || code == WSAEINVAL;
#else
    return code == EINPROGRESS || code == EALREADY;
#endif
}

io::IoResult<void> connect_native(RawSocket socket, const SocketAddress& address)
{
    auto encoded = detail::encode_address(address);
    if (encoded.is_err())
        return ca::core::Err(encoded.unwrap_err());
    const auto native = encoded.unwrap();
    if (::connect(detail::to_native_socket(socket),
                  reinterpret_cast<const sockaddr*>(&native.storage),
                  native.length) != 0)
        return ca::core::Err(detail::last_socket_error("connect"));
    return ca::core::Ok();
}

io::IoResult<void> wait_for_connect(RawSocket                             socket,
                                    std::chrono::steady_clock::time_point deadline)
{
    const auto native = detail::to_native_socket(socket);
#if !defined(_WIN32)
    if (native >= FD_SETSIZE)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::Unsupported,
            "socket descriptor exceeds FD_SETSIZE limit for select"));
#endif
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::TimedOut, "TCP connect timed out"));

        auto    remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
        timeval timeout{};
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(remaining);
        const auto capped_seconds =
            std::min<i64>(seconds.count(), std::numeric_limits<long>::max());
        timeout.tv_sec = static_cast<long>(capped_seconds);
        remaining -= std::chrono::seconds(capped_seconds);
        timeout.tv_usec =
            static_cast<long>(std::min<i64>(remaining.count(), static_cast<i64>(999999)));

        fd_set writable;
        fd_set errors;
        FD_ZERO(&writable);
        FD_ZERO(&errors);
        FD_SET(native, &writable);
        FD_SET(native, &errors);
#if defined(_WIN32)
        // Windows 的 select 忽略第一参数（传 0）；POSIX 必须传最大 fd + 1。
        const int ready = select(0, nullptr, &writable, &errors, &timeout);
#else
        const int ready = select(native + 1, nullptr, &writable, &errors, &timeout);
#endif
        if (ready == 0)
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::TimedOut, "TCP connect timed out"));
        if (ready < 0) {
            // select 被信号中断时不应误报 connect 超时，按 Interrupted 重试。
            auto error = detail::last_socket_error("select(connect)");
            if (error.kind() == io::IoErrorKind::Interrupted)
                continue;
            return ca::core::Err(std::move(error));
        }

        int                         error_code = 0;
        detail::NativeAddressLength length =
            static_cast<detail::NativeAddressLength>(sizeof(error_code));
#if defined(_WIN32)
        if (getsockopt(
                native, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error_code), &length) != 0)
#else
        if (getsockopt(native, SOL_SOCKET, SO_ERROR, &error_code, &length) != 0)
#endif
            return ca::core::Err(detail::last_socket_error("getsockopt(SO_ERROR)"));
        if (error_code != 0)
            return ca::core::Err(detail::socket_error_from_code(error_code, "connect"));
        return ca::core::Ok();
    }
}

io::IoResult<std::chrono::steady_clock::duration> validate_connect_timeout(
    std::chrono::milliseconds timeout)
{
    if (timeout.count() <= 0)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP connect timeout must be greater than zero"));
    const auto maximum_clock_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::duration::max());
    if (timeout > maximum_clock_timeout)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP connect timeout exceeds steady clock range"));
    const auto clock_timeout =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout);
    if (clock_timeout <= std::chrono::steady_clock::duration::zero())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP connect timeout exceeds steady clock range"));
    return ca::core::Ok(clock_timeout);
}

io::IoResult<TcpStream> connect_until(const SocketAddress& address,
                                      std::chrono::steady_clock::time_point deadline)
{
    if (std::chrono::steady_clock::now() >= deadline)
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::TimedOut, "TCP connect timed out"));

    auto created = detail::create_socket(address.ip().version(), SOCK_STREAM, IPPROTO_TCP);
    if (created.is_err())
        return ca::core::Err(created.unwrap_err());
    auto socket = std::move(created).unwrap();

    auto configured = configure_stream_socket(socket.get());
    if (configured.is_err())
        return ca::core::Err(configured.unwrap_err());

    auto nonblocking = detail::set_nonblocking(socket.get(), true);
    if (nonblocking.is_err())
        return ca::core::Err(nonblocking.unwrap_err());

    auto encoded = detail::encode_address(address);
    if (encoded.is_err())
        return ca::core::Err(encoded.unwrap_err());
    const auto native_address = encoded.unwrap();
    const int  connect_result = ::connect(detail::to_native_socket(socket.get()),
                                         reinterpret_cast<const sockaddr*>(&native_address.storage),
                                         native_address.length);
    if (connect_result != 0) {
        const i64 error_code = detail::last_socket_error_code();
        if (!connect_is_in_progress(error_code)) {
            detail::set_nonblocking(socket.get(), false);
            return ca::core::Err(detail::socket_error_from_code(error_code, "connect"));
        }
        auto waited = wait_for_connect(socket.get(), deadline);
        if (waited.is_err()) {
            detail::set_nonblocking(socket.get(), false);
            return ca::core::Err(waited.unwrap_err());
        }
    }

    auto blocking = detail::set_nonblocking(socket.get(), false);
    if (blocking.is_err())
        return ca::core::Err(blocking.unwrap_err());
    return TcpStream::from_socket(std::move(socket));
}

io::IoResult<OwnedSocket> create_connected_socket(const SocketAddress& address)
{
    auto created = detail::create_socket(address.ip().version(), SOCK_STREAM, IPPROTO_TCP);
    if (created.is_err())
        return ca::core::Err(created.unwrap_err());
    auto socket = std::move(created).unwrap();

    auto configured = configure_stream_socket(socket.get());
    if (configured.is_err())
        return ca::core::Err(configured.unwrap_err());

    auto connected = connect_native(socket.get(), address);
    if (connected.is_err())
        return ca::core::Err(connected.unwrap_err());
    return ca::core::Ok(std::move(socket));
}

}   // namespace

TcpStream::TcpStream(OwnedSocket socket) noexcept
    : socket_(std::move(socket))
{}

io::IoResult<TcpStream> TcpStream::from_socket(OwnedSocket socket)
{
    if (!socket.is_valid())
        return ca::core::Err(detail::closed_socket_error("TCP from_socket"));
    auto configured = configure_stream_socket(socket.get());
    if (configured.is_err())
        return ca::core::Err(configured.unwrap_err());
    return ca::core::Ok(TcpStream(std::move(socket)));
}

io::IoResult<TcpStream> TcpStream::connect(const SocketAddress& address)
{
    auto connected = create_connected_socket(address);
    if (connected.is_err())
        return ca::core::Err(connected.unwrap_err());
    return ca::core::Ok(TcpStream(std::move(connected).unwrap()));
}

io::IoResult<TcpStream> TcpStream::connect(const std::string& host, u16 port)
{
    auto resolved =
        DnsResolver::resolve(host, port, AddressFamily::Unspecified, SocketKind::Stream);
    if (resolved.is_err())
        return ca::core::Err(resolved.unwrap_err());

    std::optional<io::IoError> last_error;
    auto                       addresses = std::move(resolved).unwrap();
    for (const auto& address : addresses) {
        auto connected = connect(address);
        if (connected.is_ok())
            return connected;
        last_error = connected.unwrap_err();
    }
    if (last_error.has_value())
        return ca::core::Err(std::move(*last_error));
    return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::NotFound,
                                                "DNS resolution returned no TCP addresses"));
}

io::IoResult<TcpStream> TcpStream::connect_timeout(const SocketAddress&      address,
                                                   std::chrono::milliseconds timeout)
{
    auto validated = validate_connect_timeout(timeout);
    if (validated.is_err())
        return ca::core::Err(validated.unwrap_err());
    const auto clock_timeout = validated.unwrap();
    const auto now = std::chrono::steady_clock::now();
    if (clock_timeout > std::chrono::steady_clock::time_point::max() - now)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP connect timeout exceeds steady clock range"));
    return connect_until(address, now + clock_timeout);
}

io::IoResult<TcpStream> TcpStream::connect_timeout(const std::string& host, u16 port,
                                                   std::chrono::milliseconds timeout)
{
    auto validated = validate_connect_timeout(timeout);
    if (validated.is_err())
        return ca::core::Err(validated.unwrap_err());

    auto resolved =
        DnsResolver::resolve(host, port, AddressFamily::Unspecified, SocketKind::Stream);
    if (resolved.is_err())
        return ca::core::Err(resolved.unwrap_err());

    const auto clock_timeout = validated.unwrap();
    const auto now           = std::chrono::steady_clock::now();
    if (clock_timeout > std::chrono::steady_clock::time_point::max() - now)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP connect timeout exceeds steady clock range"));
    const auto deadline = now + clock_timeout;

    std::optional<io::IoError> last_error;
    auto                       addresses = std::move(resolved).unwrap();
    for (const auto& address : addresses) {
        auto connected = connect_until(address, deadline);
        if (connected.is_ok())
            return connected;
        last_error = connected.unwrap_err();
        if (last_error->kind() == io::IoErrorKind::TimedOut)
            return ca::core::Err(std::move(*last_error));
    }
    if (last_error.has_value())
        return ca::core::Err(std::move(*last_error));
    return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::NotFound,
                                                "DNS resolution returned no TCP addresses"));
}

io::IoResult<usize> TcpStream::read(u8* buffer, usize capacity)
{
    if (capacity == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (buffer == nullptr)
        return ca::core::Err(detail::invalid_buffer_error("TCP read"));
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP read"));

    const usize request = std::min(capacity, maximum_io_length());
#if defined(_WIN32)
    const int count = ::recv(detail::to_native_socket(socket_.get()),
                             reinterpret_cast<char*>(buffer),
                             static_cast<int>(request),
                             0);
    if (count == SOCKET_ERROR)
#else
    const ssize_t count = ::recv(detail::to_native_socket(socket_.get()), buffer, request, 0);
    if (count < 0)
#endif
        return ca::core::Err(detail::last_socket_error("recv"));
    return ca::core::Ok(static_cast<usize>(count));
}

io::IoResult<usize> TcpStream::write(const u8* data, usize length)
{
    if (length == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (data == nullptr)
        return ca::core::Err(detail::invalid_buffer_error("TCP write"));
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP write"));

    const usize request = std::min(length, maximum_io_length());
#if defined(_WIN32)
    const int count = ::send(detail::to_native_socket(socket_.get()),
                             reinterpret_cast<const char*>(data),
                             static_cast<int>(request),
                             send_flags());
    if (count == SOCKET_ERROR)
#else
    const ssize_t count =
        ::send(detail::to_native_socket(socket_.get()), data, request, send_flags());
    if (count < 0)
#endif
        return ca::core::Err(detail::last_socket_error("send"));
    return ca::core::Ok(static_cast<usize>(count));
}

io::IoResult<void> TcpStream::flush()
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP flush"));
    return ca::core::Ok();
}

io::IoResult<SocketAddress> TcpStream::local_address() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP local_address"));
    return detail::local_address(socket_.get());
}

io::IoResult<SocketAddress> TcpStream::peer_address() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP peer_address"));
    return detail::peer_address(socket_.get());
}

io::IoResult<void> TcpStream::shutdown(Shutdown direction)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP shutdown"));

    int how = 0;
#if defined(_WIN32)
    switch (direction) {
    case Shutdown::Read: how = SD_RECEIVE; break;
    case Shutdown::Write: how = SD_SEND; break;
    case Shutdown::Both: how = SD_BOTH; break;
    }
#else
    switch (direction) {
    case Shutdown::Read: how = SHUT_RD; break;
    case Shutdown::Write: how = SHUT_WR; break;
    case Shutdown::Both: how = SHUT_RDWR; break;
    }
#endif
    if (::shutdown(detail::to_native_socket(socket_.get()), how) != 0)
        return ca::core::Err(detail::last_socket_error("shutdown"));
    return ca::core::Ok();
}

io::IoResult<void> TcpStream::set_nonblocking(bool enabled)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP set_nonblocking"));
    return detail::set_nonblocking(socket_.get(), enabled);
}

io::IoResult<void> TcpStream::set_read_timeout(std::optional<std::chrono::milliseconds> timeout)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP set_read_timeout"));
    return detail::set_timeout(socket_.get(), SO_RCVTIMEO, timeout);
}

io::IoResult<void> TcpStream::set_write_timeout(std::optional<std::chrono::milliseconds> timeout)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP set_write_timeout"));
    return detail::set_timeout(socket_.get(), SO_SNDTIMEO, timeout);
}

io::IoResult<std::optional<std::chrono::milliseconds>> TcpStream::read_timeout() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP read_timeout"));
    return detail::get_timeout(socket_.get(), SO_RCVTIMEO);
}

io::IoResult<std::optional<std::chrono::milliseconds>> TcpStream::write_timeout() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP write_timeout"));
    return detail::get_timeout(socket_.get(), SO_SNDTIMEO);
}

io::IoResult<void> TcpStream::set_nodelay(bool enabled)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP set_nodelay"));
    return detail::set_bool_option(
        socket_.get(), IPPROTO_TCP, TCP_NODELAY, enabled, "setsockopt(TCP_NODELAY)");
}

io::IoResult<bool> TcpStream::nodelay() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP nodelay"));
    return detail::get_bool_option(
        socket_.get(), IPPROTO_TCP, TCP_NODELAY, "getsockopt(TCP_NODELAY)");
}

io::IoResult<TcpStream> TcpStream::try_clone() const
{
    auto duplicated = socket_.duplicate();
    if (duplicated.is_err())
        return ca::core::Err(duplicated.unwrap_err());
    return ca::core::Ok(TcpStream(std::move(duplicated).unwrap()));
}

bool TcpStream::is_open() const noexcept
{
    return socket_.is_valid();
}

RawSocket TcpStream::native_socket() const noexcept
{
    return socket_.get();
}

OwnedSocket& TcpStream::socket() noexcept
{
    return socket_;
}

const OwnedSocket& TcpStream::socket() const noexcept
{
    return socket_;
}

OwnedSocket TcpStream::into_socket() noexcept
{
    return std::move(socket_);
}

TcpListener::TcpListener(OwnedSocket socket) noexcept
    : socket_(std::move(socket))
{}

io::IoResult<TcpListener> TcpListener::from_socket(OwnedSocket socket)
{
    if (!socket.is_valid())
        return ca::core::Err(detail::closed_socket_error("TCP listener from_socket"));
    auto configured = configure_stream_socket(socket.get());
    if (configured.is_err())
        return ca::core::Err(configured.unwrap_err());
    return ca::core::Ok(TcpListener(std::move(socket)));
}

io::IoResult<TcpListener> TcpListener::bind(const SocketAddress&       address,
                                            const TcpListenerOptions& options)
{
    if (options.backlog <= 0)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP listener backlog must be greater than zero"));
    if (options.ipv6_only.has_value() && address.ip().is_ipv4())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "IPV6_V6ONLY cannot be configured for an IPv4 listener"));

    auto created = detail::create_socket(address.ip().version(), SOCK_STREAM, IPPROTO_TCP);
    if (created.is_err())
        return ca::core::Err(created.unwrap_err());
    auto socket = std::move(created).unwrap();

    auto configured = configure_stream_socket(socket.get());
    if (configured.is_err())
        return ca::core::Err(configured.unwrap_err());

    if (options.reuse_address) {
        auto reused = detail::set_bool_option(
            socket.get(), SOL_SOCKET, SO_REUSEADDR, true, "setsockopt(SO_REUSEADDR)");
        if (reused.is_err())
            return ca::core::Err(reused.unwrap_err());
    }
    if (options.ipv6_only.has_value()) {
        auto ipv6_only = detail::set_bool_option(socket.get(),
                                                 IPPROTO_IPV6,
                                                 IPV6_V6ONLY,
                                                 *options.ipv6_only,
                                                 "setsockopt(IPV6_V6ONLY)");
        if (ipv6_only.is_err())
            return ca::core::Err(ipv6_only.unwrap_err());
    }

    auto encoded = detail::encode_address(address);
    if (encoded.is_err())
        return ca::core::Err(encoded.unwrap_err());
    const auto native = encoded.unwrap();
    if (::bind(detail::to_native_socket(socket.get()),
               reinterpret_cast<const sockaddr*>(&native.storage),
               native.length) != 0)
        return ca::core::Err(detail::last_socket_error("bind"));
    if (::listen(detail::to_native_socket(socket.get()), static_cast<int>(options.backlog)) != 0)
        return ca::core::Err(detail::last_socket_error("listen"));
    return ca::core::Ok(TcpListener(std::move(socket)));
}

io::IoResult<TcpAcceptResult> TcpListener::accept()
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP accept"));

    sockaddr_storage            storage{};
    detail::NativeAddressLength length = static_cast<detail::NativeAddressLength>(sizeof(storage));
    detail::NativeSocket accepted;
    bool                 needs_inheritance_fix = true;
#if defined(__linux__) && defined(SOCK_CLOEXEC)
    accepted = ::accept4(detail::to_native_socket(socket_.get()),
                         reinterpret_cast<sockaddr*>(&storage),
                         &length,
                         SOCK_CLOEXEC);
    if (!detail::native_socket_is_valid(accepted) && (errno == ENOSYS || errno == EINVAL)) {
        accepted = ::accept(
            detail::to_native_socket(socket_.get()), reinterpret_cast<sockaddr*>(&storage), &length);
    }
    else {
        needs_inheritance_fix = false;
    }
#else
    accepted = ::accept(
        detail::to_native_socket(socket_.get()), reinterpret_cast<sockaddr*>(&storage), &length);
#endif
    if (!detail::native_socket_is_valid(accepted))
        return ca::core::Err(detail::last_socket_error("accept"));

    auto owned_result = OwnedSocket::adopt(detail::from_native_socket(accepted));
    if (owned_result.is_err()) {
#if defined(_WIN32)
        closesocket(accepted);
#else
        ::close(accepted);
#endif
        return ca::core::Err(owned_result.unwrap_err());
    }
    auto owned = std::move(owned_result).unwrap();

    auto configured = configure_stream_socket(owned.get());
    if (configured.is_err())
        return ca::core::Err(configured.unwrap_err());

    if (needs_inheritance_fix) {
        auto inheritance = detail::set_socket_not_inheritable(owned.get());
        if (inheritance.is_err())
            return ca::core::Err(inheritance.unwrap_err());
    }
    auto peer = detail::decode_address(reinterpret_cast<const sockaddr*>(&storage), length);
    if (peer.is_err())
        return ca::core::Err(peer.unwrap_err());
    return ca::core::Ok(TcpAcceptResult{TcpStream(std::move(owned)), std::move(peer).unwrap()});
}

io::IoResult<SocketAddress> TcpListener::local_address() const
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP listener local_address"));
    return detail::local_address(socket_.get());
}

io::IoResult<void> TcpListener::set_nonblocking(bool enabled)
{
    if (!is_open())
        return ca::core::Err(detail::closed_socket_error("TCP listener set_nonblocking"));
    return detail::set_nonblocking(socket_.get(), enabled);
}

io::IoResult<TcpListener> TcpListener::try_clone() const
{
    auto duplicated = socket_.duplicate();
    if (duplicated.is_err())
        return ca::core::Err(duplicated.unwrap_err());
    return ca::core::Ok(TcpListener(std::move(duplicated).unwrap()));
}

io::IoResult<void> TcpListener::close()
{
    return socket_.close();
}

bool TcpListener::is_open() const noexcept
{
    return socket_.is_valid();
}

RawSocket TcpListener::native_socket() const noexcept
{
    return socket_.get();
}

OwnedSocket TcpListener::into_socket() noexcept
{
    return std::move(socket_);
}

}   // namespace ca::net
