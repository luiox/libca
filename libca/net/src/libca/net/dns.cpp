#include "libca/net/dns.hpp"

#include <algorithm>
#include <memory>
#include <new>

#include "libca/net/detail/socket_platform.hpp"

namespace ca::net {
namespace {

io::IoError dns_error(int code, const std::string& host)
{
    io::IoErrorKind kind = io::IoErrorKind::Other;
    switch (code) {
    case EAI_AGAIN: kind = io::IoErrorKind::WouldBlock; break;
    case EAI_MEMORY: kind = io::IoErrorKind::OutOfMemory; break;
    case EAI_NONAME: kind = io::IoErrorKind::NotFound; break;
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
    case EAI_NODATA: kind = io::IoErrorKind::NotFound; break;
#endif
    default: break;
    }

#if defined(_WIN32)
    const char* message = gai_strerrorA(code);
#else
    const char* message = gai_strerror(code);
#endif
    return io::IoError::from_kind(
        kind,
        "DNS resolution failed for " + host + ": " +
            (message == nullptr ? std::string("error ") + std::to_string(code) : message));
}

struct AddressInfoDeleter
{
    void operator()(addrinfo* value) const noexcept
    {
        if (value != nullptr)
            freeaddrinfo(value);
    }
};

}   // namespace

io::IoResult<std::vector<SocketAddress>> DnsResolver::resolve(const std::string& host, u16 port,
                                                              AddressFamily family, SocketKind kind)
{
    if (host.empty())
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::InvalidInput, "DNS host must not be empty"));

    auto runtime = detail::ensure_socket_runtime();
    if (runtime.is_err())
        return ca::core::Err(runtime.unwrap_err());

    addrinfo hints{};
    hints.ai_family   = detail::native_family(family);
    hints.ai_socktype = detail::native_socket_kind(kind);
    hints.ai_protocol = kind == SocketKind::Stream ? IPPROTO_TCP : IPPROTO_UDP;

    addrinfo*         raw     = nullptr;
    const std::string service = std::to_string(port);
    const int         result  = getaddrinfo(host.c_str(), service.c_str(), &hints, &raw);
    if (result != 0)
        return ca::core::Err(dns_error(result, host));
    std::unique_ptr<addrinfo, AddressInfoDeleter> addresses(raw);

    try {
        std::vector<SocketAddress> output;
        for (const addrinfo* entry = addresses.get(); entry != nullptr; entry = entry->ai_next) {
            auto decoded = detail::decode_address(
                entry->ai_addr, static_cast<detail::NativeAddressLength>(entry->ai_addrlen));
            if (decoded.is_err())
                continue;
            auto address = decoded.unwrap();
            if (std::find(output.begin(), output.end(), address) == output.end())
                output.push_back(std::move(address));
        }
        if (output.empty())
            return ca::core::Err(io::IoError::from_kind(
                io::IoErrorKind::NotFound, "DNS resolution returned no usable IP addresses"));
        return ca::core::Ok(std::move(output));
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::OutOfMemory,
                                   std::string("DNS result allocation failed: ") + error.what()));
    }
}

}   // namespace ca::net
