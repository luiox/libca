#pragma once

#include <chrono>
#include <optional>
#include <string>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    include <windows.h>
#else
#    include <arpa/inet.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <sys/types.h>
#endif

#include "libca/net/address.hpp"
#include "libca/net/socket.hpp"

namespace ca::net::detail {

#if defined(_WIN32)
using NativeSocket        = SOCKET;
using NativeAddressLength = int;
#else
using NativeSocket        = int;
using NativeAddressLength = socklen_t;
#endif

struct NativeAddress
{
    sockaddr_storage    storage{};
    NativeAddressLength length{0};
};

io::IoResult<void> ensure_socket_runtime();
io::IoError        last_socket_error(const char* operation);
io::IoError        socket_error_from_code(i64 code, const char* operation);
i64                last_socket_error_code() noexcept;

NativeSocket to_native_socket(RawSocket socket) noexcept;
RawSocket    from_native_socket(NativeSocket socket) noexcept;
bool         native_socket_is_valid(NativeSocket socket) noexcept;

int native_family(IpVersion version) noexcept;
int native_family(AddressFamily family) noexcept;
int native_socket_kind(SocketKind kind) noexcept;

io::IoResult<OwnedSocket>   create_socket(IpVersion version, int type, int protocol);
io::IoResult<NativeAddress> encode_address(const SocketAddress& address);
io::IoResult<SocketAddress> decode_address(const sockaddr* address, NativeAddressLength length);
io::IoResult<SocketAddress> local_address(RawSocket socket);
io::IoResult<SocketAddress> peer_address(RawSocket socket);

io::IoResult<void> set_nonblocking(RawSocket socket, bool enabled);
io::IoResult<void> set_timeout(RawSocket socket, int option,
                               std::optional<std::chrono::milliseconds> timeout);
io::IoResult<std::optional<std::chrono::milliseconds>> get_timeout(RawSocket socket, int option);
io::IoResult<void> set_bool_option(RawSocket socket, int level, int option, bool enabled,
                                   const char* operation);
io::IoResult<bool> get_bool_option(RawSocket socket, int level, int option, const char* operation);

io::IoError closed_socket_error(const char* operation);
io::IoError invalid_buffer_error(const char* operation);

}   // namespace ca::net::detail
