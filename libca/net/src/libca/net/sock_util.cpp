#include "libca/net/sock_util.hpp"

#include <array>
#include <climits>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "libca/net/detail/socket_platform.hpp"
#include "libca/net/socket_error.hpp"

#if defined(_WIN32)
// socket_platform.hpp 已定义 WIN32_LEAN_AND_MEAN / NOMINMAX 并引入 winsock2.h；
// mstcpip.h 提供 SIO_KEEPALIVE_VALS，iphlpapi.h 必须在其之后包含。
#    include <mstcpip.h>
#    include <iphlpapi.h>
#else
#    include <ifaddrs.h>
#    include <net/if.h>
#endif

namespace ca::net {
namespace {

/// @brief 地址族对应的 loopback 回落地址。
IpAddress fallback_loopback(AddressFamily family) noexcept
{
    return family == AddressFamily::Ipv6 ? IpAddress::localhost_v6() : IpAddress::localhost_v4();
}

/// @brief 地址族对应的默认探测目标；Unspecified 按 IPv4 处理。
SocketAddress default_probe_peer(AddressFamily family) noexcept
{
    if (family == AddressFamily::Ipv6) {
        // 2001:4860:4860::8888（Google Public DNS IPv6）
        std::array<u8, 16> octets{
            0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88};
        return SocketAddress(IpAddress::v6(octets), 80);
    }
    // 8.8.8.8:80（Google Public DNS IPv4）
    return SocketAddress(IpAddress::v4(8, 8, 8, 8), 80);
}

io::IoResult<SocketAddress> connect_and_read_local(RawSocket                    socket,
                                                   const detail::NativeAddress& native)
{
    if (::connect(detail::to_native_socket(socket),
                  reinterpret_cast<const sockaddr*>(&native.storage),
                  native.length) != 0)
        return ca::core::Err(detail::last_socket_error("connect(UDP probe)"));
    return detail::local_address(socket);
}

#if defined(_WIN32)

/// @brief 把 UTF-16 的适配器 FriendlyName 转为 UTF-8；空串表示转换失败。
std::string wide_to_utf8(const wchar_t* text)
{
    if (text == nullptr)
        return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1)
        return {};
    std::string output(static_cast<usize>(bytes - 1), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), bytes, nullptr, nullptr) !=
        bytes - 1)
        return {};
    return output;
}

#endif

}   // namespace

IpAddress get_local_ip(const SocketAddress& peer)
{
    const AddressFamily family = peer.ip().is_ipv6() ? AddressFamily::Ipv6 : AddressFamily::Ipv4;

    auto runtime = detail::ensure_socket_runtime();
    if (runtime.is_err())
        return fallback_loopback(family);

    // UDP socket 只用于让内核选择出口地址，不发送任何数据。
    auto created = detail::create_socket(peer.ip().version(), SOCK_DGRAM, IPPROTO_UDP);
    if (created.is_err())
        return fallback_loopback(family);
    auto socket = std::move(created).unwrap();

    auto encoded = detail::encode_address(peer);
    if (encoded.is_err())
        return fallback_loopback(family);

    auto local = connect_and_read_local(socket.get(), encoded.unwrap());
    if (local.is_err())
        return fallback_loopback(family);
    return local.unwrap().ip();
}

IpAddress get_local_ip(AddressFamily family)
{
    const AddressFamily probe_family =
        family == AddressFamily::Ipv6 ? AddressFamily::Ipv6 : AddressFamily::Ipv4;
    return get_local_ip(default_probe_peer(probe_family));
}

io::IoResult<void> set_tcp_keepalive(RawSocket socket, const TcpKeepaliveConfig& config)
{
    // 三参数统一校验（含 Windows 不支持的 count），保证跨平台语义一致。
    if (config.idle.count() <= 0 || config.interval.count() <= 0 || config.count == 0)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput,
            "TCP keepalive idle, interval and count must be greater than zero"));

    auto enabled =
        detail::set_bool_option(socket, SOL_SOCKET, SO_KEEPALIVE, true, "setsockopt(SO_KEEPALIVE)");
    if (enabled.is_err())
        return enabled;

#if defined(_WIN32)
    // Windows 用 WSAIoctl(SIO_KEEPALIVE_VALS)，毫秒精度且没有 count 参数；
    // 该 ioctl 只写不可读，参数无法回读。缓冲区为调用方栈内存，不涉及 FreeMibTable。
    constexpr i64 maximum_milliseconds = static_cast<i64>(std::numeric_limits<u_long>::max());
    const i64     idle_ms              = config.idle.count() * 1000;
    const i64     interval_ms          = config.interval.count() * 1000;
    if (idle_ms > maximum_milliseconds || interval_ms > maximum_milliseconds)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "TCP keepalive parameter exceeds Windows range"));

    tcp_keepalive values{};
    values.onoff             = 1;
    values.keepalivetime     = static_cast<u_long>(idle_ms);
    values.keepaliveinterval = static_cast<u_long>(interval_ms);
    DWORD returned           = 0;
    if (WSAIoctl(detail::to_native_socket(socket),
                 SIO_KEEPALIVE_VALS,
                 &values,
                 sizeof(values),
                 nullptr,
                 0,
                 &returned,
                 nullptr,
                 nullptr) == SOCKET_ERROR)
        return ca::core::Err(detail::last_socket_error("WSAIoctl(SIO_KEEPALIVE_VALS)"));
    static_cast<void>(config.count);   // Windows 不支持探测次数配置，忽略但已校验
    return ca::core::Ok();
#else
    const auto to_int_seconds = [](std::chrono::seconds value) -> io::IoResult<int> {
        if (value.count() > static_cast<i64>(INT_MAX))
            return ca::core::Err(io::IoError::from_kind(
                io::IoErrorKind::InvalidInput, "TCP keepalive parameter exceeds platform range"));
        return ca::core::Ok(static_cast<int>(value.count()));
    };

    auto idle_value = to_int_seconds(config.idle);
    if (idle_value.is_err())
        return ca::core::Err(idle_value.unwrap_err());
    auto interval_value = to_int_seconds(config.interval);
    if (interval_value.is_err())
        return ca::core::Err(interval_value.unwrap_err());

    const detail::NativeSocket native = detail::to_native_socket(socket);
    // Linux 用 TCP_KEEPIDLE；macOS 没有 TCP_KEEPIDLE，等价选项是 TCP_KEEPALIVE。
    int idle_option = 0;
#    if defined(TCP_KEEPIDLE)
    idle_option = TCP_KEEPIDLE;
#    elif defined(TCP_KEEPALIVE)
    idle_option = TCP_KEEPALIVE;
#    endif
    if (idle_option == 0)
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::Unsupported,
                                                    "TCP keepalive idle option is not available"));

    const int idle_seconds     = idle_value.unwrap();
    const int interval_seconds = interval_value.unwrap();
    const int probe_count      = static_cast<int>(config.count);
    if (::setsockopt(native,
                     IPPROTO_TCP,
                     idle_option,
                     reinterpret_cast<const char*>(&idle_seconds),
                     sizeof(idle_seconds)) != 0)
        return ca::core::Err(detail::last_socket_error("setsockopt(TCP_KEEPIDLE)"));
    if (::setsockopt(native,
                     IPPROTO_TCP,
                     TCP_KEEPINTVL,
                     reinterpret_cast<const char*>(&interval_seconds),
                     sizeof(interval_seconds)) != 0)
        return ca::core::Err(detail::last_socket_error("setsockopt(TCP_KEEPINTVL)"));
    if (::setsockopt(native,
                     IPPROTO_TCP,
                     TCP_KEEPCNT,
                     reinterpret_cast<const char*>(&probe_count),
                     sizeof(probe_count)) != 0)
        return ca::core::Err(detail::last_socket_error("setsockopt(TCP_KEEPCNT)"));
    return ca::core::Ok();
#endif
}

io::IoResult<bool> tcp_keepalive_enabled(RawSocket socket)
{
    return detail::get_bool_option(socket, SOL_SOCKET, SO_KEEPALIVE, "getsockopt(SO_KEEPALIVE)");
}

io::IoResult<std::vector<NetworkInterfaceAddress>> interface_list()
{
#if defined(_WIN32)
    auto runtime = detail::ensure_socket_runtime();
    if (runtime.is_err())
        return ca::core::Err(runtime.unwrap_err());

    // GetAdaptersAddresses 的推荐初始缓冲区为 15KB，缓冲区不足返回
    // ERROR_BUFFER_OVERFLOW 并由调用方重试；缓冲区是自管内存，无需 FreeMibTable。
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    using AdapterBuffer = std::vector<unsigned long>;
    AdapterBuffer buffer(15 * 1024 / sizeof(unsigned long));
    for (;;) {
        ULONG       size = static_cast<ULONG>(buffer.size() * sizeof(unsigned long));
        const ULONG result =
            ::GetAdaptersAddresses(AF_UNSPEC,
                                   flags,
                                   nullptr,
                                   reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),
                                   &size);
        if (result == NO_ERROR)
            break;
        if (result != ERROR_BUFFER_OVERFLOW || buffer.size() > (1U << 22))
            return ca::core::Err(
                io::IoError::from_native_error(static_cast<i64>(result), "GetAdaptersAddresses"));
        buffer.resize(buffer.size() * 2 + 1024);
    }

    try {
        std::vector<NetworkInterfaceAddress> output;
        const auto* adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buffer.data());
        for (; adapter != nullptr; adapter = adapter->Next) {
            std::string name = wide_to_utf8(adapter->FriendlyName);
            if (name.empty() && adapter->AdapterName != nullptr)
                name = adapter->AdapterName;
            const bool up       = adapter->OperStatus == IfOperStatusUp;
            const bool loopback = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
            for (const auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr;
                 unicast             = unicast->Next) {
                auto decoded = detail::decode_address(
                    unicast->Address.lpSockaddr,
                    static_cast<detail::NativeAddressLength>(unicast->Address.iSockaddrLength));
                if (decoded.is_err())
                    continue;
                output.push_back(
                    NetworkInterfaceAddress{name, decoded.unwrap().ip(), up, loopback});
            }
        }
        return ca::core::Ok(std::move(output));
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::OutOfMemory,
            std::string("interface list allocation failed: ") + error.what()));
    }
#else
    // getifaddrs 结果用 RAII freeifaddrs 释放，避免错误路径泄漏。
    struct IfaddrsDeleter
    {
        void operator()(ifaddrs* value) const noexcept
        {
            if (value != nullptr)
                freeifaddrs(value);
        }
    };

    ifaddrs* raw = nullptr;
    if (::getifaddrs(&raw) != 0)
        return ca::core::Err(detail::last_socket_error("getifaddrs"));
    std::unique_ptr<ifaddrs, IfaddrsDeleter> interfaces(raw);

    try {
        std::vector<NetworkInterfaceAddress> output;
        for (const ifaddrs* entry = interfaces.get(); entry != nullptr; entry = entry->ifa_next) {
            if (entry->ifa_addr == nullptr || entry->ifa_name == nullptr)
                continue;
            // 只保留 IPv4 / IPv6 单播地址条目，其余族（如 AF_PACKET/AF_LINK）跳过。
            auto decoded = detail::decode_address(
                entry->ifa_addr,
                static_cast<detail::NativeAddressLength>(sizeof(sockaddr_storage)));
            if (decoded.is_err())
                continue;
            output.push_back(NetworkInterfaceAddress{entry->ifa_name,
                                                     decoded.unwrap().ip(),
                                                     (entry->ifa_flags & IFF_UP) != 0,
                                                     (entry->ifa_flags & IFF_LOOPBACK) != 0});
        }
        return ca::core::Ok(std::move(output));
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::OutOfMemory,
            std::string("interface list allocation failed: ") + error.what()));
    }
#endif
}

}   // namespace ca::net
