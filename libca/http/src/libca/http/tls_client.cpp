#include "libca/http/detail/client_transport.hpp"

#include "libca/http/client.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "libca/net/tls_stream.hpp"

namespace ca::http::detail {
namespace {

/// @brief 基于 net::TlsStream 的 HTTPS client transport。
/// @note 成员声明顺序即构造顺序、逆序即析构顺序：tls_ 借用 stream_（TlsStream
/// 不拥有底层流），必须保证 tls_ 先于 stream_ 析构。
class TlsClientTransport final : public ClientTransport
{
public:
    explicit TlsClientTransport(net::TcpStream stream) noexcept
        : stream_(std::move(stream))
    {}

    io::IoResult<usize> read(u8* buffer, usize capacity) override
    {
        if (!tls_.has_value())
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::NotConnected, "TLS transport is not ready"));
        return tls_->read(buffer, capacity);
    }

    io::IoResult<usize> write(const u8* data, usize length) override
    {
        if (!tls_.has_value())
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::NotConnected, "TLS transport is not ready"));
        return tls_->write(data, length);
    }

    io::IoResult<void> flush() override
    {
        if (!tls_.has_value())
            return ca::core::Ok();
        return tls_->flush();
    }

    net::TcpStream& tcp_stream() noexcept override { return stream_; }

    void attach(net::TlsStream stream) { tls_.emplace(std::move(stream)); }

private:
    net::TcpStream                stream_;
    std::optional<net::TlsStream> tls_;
};

net::TlsClientOptions to_net_options(const HttpTlsClientOptions& options)
{
    net::TlsClientOptions converted;
    converted.verify_peer     = options.verify_peer;
    converted.ca_file         = options.ca_file;
    converted.ca_directory    = options.ca_directory;
    converted.alpn_protocols  = {"http/1.1"};
    return converted;
}

}   // namespace

bool tls_client_available() noexcept
{
    return net::tls_stream_supported();
}

HttpResult<std::unique_ptr<ClientTransport>> make_tls_client_transport(
    net::TcpStream stream, const std::string& host, const HttpTlsClientOptions& options,
    std::chrono::milliseconds handshake_timeout)
{
    // 先把 TcpStream 落进 transport 拿到稳定地址，再对该地址做 TLS 握手：
    // TlsStream 借用底层流，握手与后续读写必须作用于最终持有的同一条 TcpStream。
    auto transport   = std::make_unique<TlsClientTransport>(std::move(stream));
    auto& underlying = transport->tcp_stream();

    net::TlsHandshakeControl control;
    control.timeout = handshake_timeout;
    // 每次握手重试前把 socket 读写超时收紧到剩余期限（对齐总期限语义）。
    control.before_retry = [&underlying](std::chrono::milliseconds remaining) -> io::IoResult<void> {
        auto read_timeout = underlying.set_read_timeout(remaining);
        if (read_timeout.is_err())
            return read_timeout;
        return underlying.set_write_timeout(remaining);
    };

    auto secured =
        net::TlsStream::connect(underlying, underlying, host, to_net_options(options), control);
    if (secured.is_err())
        return ca::core::Err(
            HttpError::from_io(std::move(secured).unwrap_err(), "perform TLS handshake"));
    auto tls_stream = std::move(secured).unwrap();

    // 行为等价保留：对端协商出 http/1.1 以外的 ALPN 视为不支持。
    const auto selected = tls_stream.alpn_selected();
    if (selected.has_value() && *selected != "http/1.1")
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                                  "TLS peer selected an unsupported ALPN"));

    transport->attach(std::move(tls_stream));
    return ca::core::Ok(std::unique_ptr<ClientTransport>(std::move(transport)));
}

}   // namespace ca::http::detail
