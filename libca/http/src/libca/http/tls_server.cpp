#include "libca/http/detail/server_transport.hpp"

#include "libca/http/server.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "libca/net/tls_stream.hpp"
#include "libca/thread/stop_token.hpp"

namespace ca::http::detail {
namespace {

/// @brief 基于 net::TlsStream 的 HTTPS server transport。
/// @note 成员声明顺序即析构顺序：tls_ 借用 stream_（TlsStream 不拥有底层流），
/// 必须保证 tls_ 先于 stream_ 析构。
class TlsServerTransport final : public ServerTransport
{
public:
    explicit TlsServerTransport(net::TcpStream stream) noexcept
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

net::TlsServerOptions to_net_options(const HttpTlsServerOptions& options)
{
    net::TlsServerOptions converted;
    converted.certificate_chain_file = options.certificate_chain_file;
    converted.private_key_file       = options.private_key_file;
    converted.ca_file                = options.ca_file;
    converted.ca_directory           = options.ca_directory;
    converted.verify_client          = options.verify_client;
    return converted;
}

}   // namespace

// ServerTlsContext 的实质实现已下沉到 net::TlsServerContext，这里只做配置映射。
ServerTlsContext::ServerTlsContext() noexcept             = default;
ServerTlsContext::~ServerTlsContext()                     = default;
ServerTlsContext::ServerTlsContext(ServerTlsContext&&) noexcept = default;
ServerTlsContext& ServerTlsContext::operator=(ServerTlsContext&&) noexcept = default;

HttpResult<void> ServerTlsContext::load(const HttpTlsServerOptions& options)
{
    auto loaded = context_.load(to_net_options(options));
    if (loaded.is_err())
        return ca::core::Err(
            HttpError::from_io(std::move(loaded).unwrap_err(), "load TLS server context"));
    return ca::core::Ok();
}

void* ServerTlsContext::native_handle() const noexcept
{
    return context_.native_handle();
}

const net::TlsServerContext& ServerTlsContext::net_context() const noexcept
{
    return context_;
}

bool tls_server_available() noexcept
{
    return net::tls_stream_supported();
}

HttpResult<std::unique_ptr<ServerTransport>> make_tls_server_transport(
    net::TcpStream stream, const ServerTlsContext& context,
    std::chrono::milliseconds handshake_timeout, ca::thread::StopToken stop_token,
    std::chrono::milliseconds stop_poll_interval)
{
    // 先落 transport 拿到稳定的 TcpStream 地址，再对它做 TLS 握手（TlsStream 借用底层流）。
    auto transport   = std::make_unique<TlsServerTransport>(std::move(stream));
    auto& underlying = transport->tcp_stream();

    net::TlsHandshakeControl control;
    control.timeout = handshake_timeout;
    // 对称旧实现的握手循环：server 停止时立即以 ConnectionAborted 终止；否则把
    // socket 读写超时收紧到 min(剩余期限, stop 轮询间隔)，让阻塞读周期性超时，
    // 驱动握手循环回到顶部检查协作停止标记。
    control.before_retry = [&underlying, stop = stop_token,
                            poll = stop_poll_interval](std::chrono::milliseconds remaining)
        -> io::IoResult<void> {
        if (stop.stop_requested())
            return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::ConnectionAborted,
                                                        "TLS handshake cancelled by server stop"));
        const auto wait   = stop.stop_possible() ? std::min(remaining, poll) : remaining;
        auto read_timeout = underlying.set_read_timeout(wait);
        if (read_timeout.is_err())
            return read_timeout;
        return underlying.set_write_timeout(wait);
    };

    auto accepted = net::TlsStream::accept(context.net_context(), underlying, underlying, control);
    if (accepted.is_err())
        return ca::core::Err(
            HttpError::from_io(std::move(accepted).unwrap_err(), "perform TLS handshake"));

    transport->attach(std::move(accepted).unwrap());
    return ca::core::Ok(std::unique_ptr<ServerTransport>(std::move(transport)));
}

}   // namespace ca::http::detail
