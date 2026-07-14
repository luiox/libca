#include "libca/net/socket.hpp"

#include <cerrno>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>

#if !defined(_WIN32)
#    include <fcntl.h>
#    include <unistd.h>
#endif

#include "libca/net/detail/socket_platform.hpp"

namespace ca::net::detail {

io::IoResult<void> ensure_socket_runtime()
{
#if defined(_WIN32)
    // Windows 必须在首次使用任何 socket API 前调用一次 WSAStartup；
    // 用 call_once 保证进程级单次初始化并缓存失败码。POSIX 上是 no-op。
    static std::once_flag init_flag;
    static int            init_error = 0;
    std::call_once(init_flag, []() {
        WSADATA data{};
        init_error = WSAStartup(MAKEWORD(2, 2), &data);
    });
    if (init_error != 0)
        return ca::core::Err(socket_error_from_code(init_error, "WSAStartup"));
#endif
    return ca::core::Ok();
}

i64 last_socket_error_code() noexcept
{
    // 关键约定：Windows socket 错误码取自 WSAGetLastError() 而非 errno
    // （errno 不会被 winsock 设置）；POSIX 用 errno。
#if defined(_WIN32)
    return static_cast<i64>(WSAGetLastError());
#else
    return static_cast<i64>(errno);
#endif
}

io::IoError socket_error_from_code(i64 code, const char* operation)
{
    return io::IoError::from_native_error(code, operation);
}

io::IoError last_socket_error(const char* operation)
{
    return socket_error_from_code(last_socket_error_code(), operation);
}

NativeSocket to_native_socket(RawSocket socket) noexcept
{
    return static_cast<NativeSocket>(socket);
}

RawSocket from_native_socket(NativeSocket socket) noexcept
{
    return static_cast<RawSocket>(socket);
}

bool native_socket_is_valid(NativeSocket socket) noexcept
{
#if defined(_WIN32)
    return socket != INVALID_SOCKET;
#else
    return socket >= 0;
#endif
}

int native_family(IpVersion version) noexcept
{
    return version == IpVersion::V4 ? AF_INET : AF_INET6;
}

int native_family(AddressFamily family) noexcept
{
    switch (family) {
    case AddressFamily::Unspecified: return AF_UNSPEC;
    case AddressFamily::Ipv4: return AF_INET;
    case AddressFamily::Ipv6: return AF_INET6;
    }
    return AF_UNSPEC;
}

int native_socket_kind(SocketKind kind) noexcept
{
    return kind == SocketKind::Stream ? SOCK_STREAM : SOCK_DGRAM;
}

io::IoResult<OwnedSocket> create_socket(IpVersion version, int type, int protocol)
{
    auto runtime = ensure_socket_runtime();
    if (runtime.is_err())
        return ca::core::Err(runtime.unwrap_err());

#if defined(_WIN32)
    const NativeSocket socket =
        WSASocketW(native_family(version), type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (!native_socket_is_valid(socket))
        return ca::core::Err(last_socket_error("WSASocket"));
#else
    // 原子地置 CLOEXEC 以避开 fork 竞态；老内核不支持 SOCK_CLOEXEC 会返回 EINVAL，
    // 此时退回 fcntl 设置（有竞态但保证语义正确）。
    int  native_type     = type;
    bool cloexec_applied = false;
#    if defined(SOCK_CLOEXEC)
    native_type |= SOCK_CLOEXEC;
#    endif
    NativeSocket socket = ::socket(native_family(version), native_type, protocol);
#    if defined(SOCK_CLOEXEC)
    if (native_socket_is_valid(socket)) {
        cloexec_applied = true;
    }
    else if (errno == EINVAL) {
        socket = ::socket(native_family(version), type, protocol);
    }
#    endif
    if (!native_socket_is_valid(socket))
        return ca::core::Err(last_socket_error("socket"));

    const bool needs_cloexec = !cloexec_applied;
    if (needs_cloexec && fcntl(socket, F_SETFD, FD_CLOEXEC) != 0) {
        const auto error = last_socket_error("fcntl(FD_CLOEXEC)");
        ::close(socket);
        return ca::core::Err(error);
    }
#endif
    return OwnedSocket::adopt(from_native_socket(socket));
}

io::IoResult<NativeAddress> encode_address(const SocketAddress& address)
{
    NativeAddress result;
    if (address.ip().is_ipv4()) {
        sockaddr_in native{};
        native.sin_family = AF_INET;
        native.sin_port   = htons(address.port());
        std::memcpy(&native.sin_addr, address.ip().octets().data(), 4);
        std::memcpy(&result.storage, &native, sizeof(native));
        result.length = static_cast<NativeAddressLength>(sizeof(native));
        return ca::core::Ok(result);
    }

    sockaddr_in6 native{};
    native.sin6_family   = AF_INET6;
    native.sin6_port     = htons(address.port());
    native.sin6_flowinfo = htonl(address.flow_info());
    native.sin6_scope_id = address.scope_id();
    std::memcpy(&native.sin6_addr, address.ip().octets().data(), 16);
    std::memcpy(&result.storage, &native, sizeof(native));
    result.length = static_cast<NativeAddressLength>(sizeof(native));
    return ca::core::Ok(result);
}

io::IoResult<SocketAddress> decode_address(const sockaddr* address, NativeAddressLength length)
{
    if (address == nullptr)
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "native socket address must not be null"));

    if (address->sa_family == AF_INET &&
        length >= static_cast<NativeAddressLength>(sizeof(sockaddr_in))) {
        const auto* native = reinterpret_cast<const sockaddr_in*>(address);
        const auto* bytes  = reinterpret_cast<const u8*>(&native->sin_addr);
        return ca::core::Ok(SocketAddress(IpAddress::v4(bytes[0], bytes[1], bytes[2], bytes[3]),
                                          ntohs(native->sin_port)));
    }
    if (address->sa_family == AF_INET6 &&
        length >= static_cast<NativeAddressLength>(sizeof(sockaddr_in6))) {
        const auto*        native = reinterpret_cast<const sockaddr_in6*>(address);
        std::array<u8, 16> octets{};
        std::memcpy(octets.data(), &native->sin6_addr, octets.size());
        return ca::core::Ok(SocketAddress(IpAddress::v6(octets),
                                          ntohs(native->sin6_port),
                                          ntohl(native->sin6_flowinfo),
                                          native->sin6_scope_id));
    }
    return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::Unsupported,
                                                "unsupported native socket address family"));
}

io::IoResult<SocketAddress> local_address(RawSocket socket)
{
    sockaddr_storage    storage{};
    NativeAddressLength length = static_cast<NativeAddressLength>(sizeof(storage));
    if (getsockname(to_native_socket(socket), reinterpret_cast<sockaddr*>(&storage), &length) != 0)
        return ca::core::Err(last_socket_error("getsockname"));
    return decode_address(reinterpret_cast<const sockaddr*>(&storage), length);
}

io::IoResult<SocketAddress> peer_address(RawSocket socket)
{
    sockaddr_storage    storage{};
    NativeAddressLength length = static_cast<NativeAddressLength>(sizeof(storage));
    if (getpeername(to_native_socket(socket), reinterpret_cast<sockaddr*>(&storage), &length) != 0)
        return ca::core::Err(last_socket_error("getpeername"));
    return decode_address(reinterpret_cast<const sockaddr*>(&storage), length);
}

io::IoResult<void> set_nonblocking(RawSocket socket, bool enabled)
{
#if defined(_WIN32)
    u_long value = enabled ? 1UL : 0UL;
    if (ioctlsocket(to_native_socket(socket), FIONBIO, &value) != 0)
        return ca::core::Err(last_socket_error("ioctlsocket(FIONBIO)"));
#else
    const int native = to_native_socket(socket);
    const int flags  = fcntl(native, F_GETFL, 0);
    if (flags < 0)
        return ca::core::Err(last_socket_error("fcntl(F_GETFL)"));
    const int updated = enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
    if (fcntl(native, F_SETFL, updated) != 0)
        return ca::core::Err(last_socket_error("fcntl(F_SETFL)"));
#endif
    return ca::core::Ok();
}

io::IoResult<void> set_timeout(RawSocket socket, int option,
                               std::optional<std::chrono::milliseconds> timeout)
{
    // timeout<=0 直接拒绝：setsockopt 中 0 表示"用系统默认值"而非"禁用超时"，会被误解。
    if (timeout.has_value() && timeout->count() <= 0)
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "socket timeout must be greater than zero"));

#if defined(_WIN32)
    DWORD value = 0;
    if (timeout.has_value()) {
        if (static_cast<u64>(timeout->count()) > std::numeric_limits<DWORD>::max())
            return ca::core::Err(io::IoError::from_kind(
                io::IoErrorKind::InvalidInput, "socket timeout exceeds Windows DWORD range"));
        value = static_cast<DWORD>(timeout->count());
    }
    if (setsockopt(to_native_socket(socket),
                   SOL_SOCKET,
                   option,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) != 0)
        return ca::core::Err(last_socket_error("setsockopt(timeout)"));
#else
    timeval value{};
    if (timeout.has_value()) {
        const auto seconds      = timeout->count() / 1000;
        const auto milliseconds = timeout->count() % 1000;
        if (seconds > std::numeric_limits<decltype(value.tv_sec)>::max())
            return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                        "socket timeout exceeds timeval range"));
        value.tv_sec  = static_cast<decltype(value.tv_sec)>(seconds);
        value.tv_usec = static_cast<decltype(value.tv_usec)>(milliseconds * 1000);
    }
    if (setsockopt(to_native_socket(socket), SOL_SOCKET, option, &value, sizeof(value)) != 0)
        return ca::core::Err(last_socket_error("setsockopt(timeout)"));
#endif
    return ca::core::Ok();
}

io::IoResult<std::optional<std::chrono::milliseconds>> get_timeout(RawSocket socket, int option)
{
#if defined(_WIN32)
    DWORD value  = 0;
    int   length = sizeof(value);
    if (getsockopt(to_native_socket(socket),
                   SOL_SOCKET,
                   option,
                   reinterpret_cast<char*>(&value),
                   &length) != 0)
        return ca::core::Err(last_socket_error("getsockopt(timeout)"));
    if (value == 0)
        return ca::core::Ok(std::optional<std::chrono::milliseconds>{});
    return ca::core::Ok(std::optional<std::chrono::milliseconds>(std::chrono::milliseconds(value)));
#else
    timeval             value{};
    NativeAddressLength length = static_cast<NativeAddressLength>(sizeof(value));
    if (getsockopt(to_native_socket(socket), SOL_SOCKET, option, &value, &length) != 0)
        return ca::core::Err(last_socket_error("getsockopt(timeout)"));
    if (value.tv_sec == 0 && value.tv_usec == 0)
        return ca::core::Ok(std::optional<std::chrono::milliseconds>{});
    const auto duration =
        std::chrono::seconds(value.tv_sec) + std::chrono::microseconds(value.tv_usec);
    return ca::core::Ok(std::optional<std::chrono::milliseconds>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration)));
#endif
}

io::IoResult<void> set_bool_option(RawSocket socket, int level, int option, bool enabled,
                                   const char* operation)
{
    const int value = enabled ? 1 : 0;
#if defined(_WIN32)
    if (setsockopt(to_native_socket(socket),
                   level,
                   option,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) != 0)
#else
    if (setsockopt(to_native_socket(socket), level, option, &value, sizeof(value)) != 0)
#endif
        return ca::core::Err(last_socket_error(operation));
    return ca::core::Ok();
}

io::IoResult<bool> get_bool_option(RawSocket socket, int level, int option, const char* operation)
{
    int                 value  = 0;
    NativeAddressLength length = static_cast<NativeAddressLength>(sizeof(value));
#if defined(_WIN32)
    if (getsockopt(
            to_native_socket(socket), level, option, reinterpret_cast<char*>(&value), &length) != 0)
#else
    if (getsockopt(to_native_socket(socket), level, option, &value, &length) != 0)
#endif
        return ca::core::Err(last_socket_error(operation));
    return ca::core::Ok(value != 0);
}

io::IoError closed_socket_error(const char* operation)
{
    return io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                  std::string(operation) + " on a closed socket");
}

io::IoError invalid_buffer_error(const char* operation)
{
    return io::IoError::from_kind(
        io::IoErrorKind::InvalidInput,
        std::string(operation) + " buffer must not be null when length is nonzero");
}

}   // namespace ca::net::detail

namespace ca::net {

OwnedSocket::OwnedSocket(RawSocket socket) noexcept
    : socket_(socket)
{}

OwnedSocket::~OwnedSocket()
{
    close_noexcept();
}

OwnedSocket::OwnedSocket(OwnedSocket&& other) noexcept
    : socket_(other.release())
{}

OwnedSocket& OwnedSocket::operator=(OwnedSocket&& other) noexcept
{
    if (this != &other) {
        close_noexcept();
        socket_ = other.release();
    }
    return *this;
}

io::IoResult<OwnedSocket> OwnedSocket::adopt(RawSocket socket)
{
    auto runtime = detail::ensure_socket_runtime();
    if (runtime.is_err())
        return ca::core::Err(runtime.unwrap_err());
#if !defined(_WIN32)
    if (socket > static_cast<RawSocket>(std::numeric_limits<int>::max()))
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "native socket value exceeds POSIX int range"));
#endif
    if (!detail::native_socket_is_valid(detail::to_native_socket(socket)))
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "cannot adopt an invalid native socket"));
    return ca::core::Ok(OwnedSocket(socket));
}

io::IoResult<OwnedSocket> OwnedSocket::duplicate() const
{
    if (!is_valid())
        return ca::core::Err(detail::closed_socket_error("duplicate"));

#if defined(_WIN32)
    WSAPROTOCOL_INFOW information{};
    if (WSADuplicateSocketW(
            detail::to_native_socket(socket_), GetCurrentProcessId(), &information) != 0)
        return ca::core::Err(detail::last_socket_error("WSADuplicateSocket"));
    const auto duplicate = WSASocketW(FROM_PROTOCOL_INFO,
                                      FROM_PROTOCOL_INFO,
                                      FROM_PROTOCOL_INFO,
                                      &information,
                                      0,
                                      WSA_FLAG_OVERLAPPED);
    if (!detail::native_socket_is_valid(duplicate))
        return ca::core::Err(detail::last_socket_error("WSASocket(duplicate)"));
#else
    int duplicate = -1;
#    if defined(F_DUPFD_CLOEXEC)
    duplicate = fcntl(detail::to_native_socket(socket_), F_DUPFD_CLOEXEC, 0);
#    else
    duplicate = ::dup(detail::to_native_socket(socket_));
    if (duplicate >= 0 && fcntl(duplicate, F_SETFD, FD_CLOEXEC) != 0) {
        const auto error = detail::last_socket_error("fcntl(FD_CLOEXEC)");
        ::close(duplicate);
        return ca::core::Err(error);
    }
#    endif
    if (duplicate < 0)
        return ca::core::Err(detail::last_socket_error("duplicate socket"));
#endif
    return ca::core::Ok(OwnedSocket(detail::from_native_socket(duplicate)));
}

bool OwnedSocket::is_valid() const noexcept
{
    return detail::native_socket_is_valid(detail::to_native_socket(socket_));
}

RawSocket OwnedSocket::get() const noexcept
{
    return socket_;
}

io::IoResult<void> OwnedSocket::close()
{
    if (!is_valid())
        return ca::core::Ok();
    const auto closing = detail::to_native_socket(release());
#if defined(_WIN32)
    if (closesocket(closing) != 0)
        return ca::core::Err(detail::last_socket_error("closesocket"));
#else
    if (::close(closing) != 0)
        return ca::core::Err(detail::last_socket_error("close socket"));
#endif
    return ca::core::Ok();
}

RawSocket OwnedSocket::release() noexcept
{
    const RawSocket released = socket_;
    socket_                  = invalid_raw_socket();
    return released;
}

void OwnedSocket::close_noexcept() noexcept
{
    if (!is_valid())
        return;
    const auto closing = detail::to_native_socket(release());
#if defined(_WIN32)
    closesocket(closing);
#else
    ::close(closing);
#endif
}

}   // namespace ca::net
