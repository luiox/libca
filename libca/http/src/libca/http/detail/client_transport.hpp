#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "libca/http/http_error.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"
#include "libca/net/tcp.hpp"

namespace ca::http {

struct HttpTlsClientOptions;

namespace detail {

class ClientTransport : public io::Reader, public io::Writer
{
public:
    ~ClientTransport() override = default;

    virtual net::TcpStream& tcp_stream() noexcept = 0;
};

bool tls_client_available() noexcept;

HttpResult<std::unique_ptr<ClientTransport>> make_tls_client_transport(
    net::TcpStream stream, const std::string& host, const HttpTlsClientOptions& options,
    std::chrono::milliseconds handshake_timeout);

}   // namespace detail
}   // namespace ca::http
