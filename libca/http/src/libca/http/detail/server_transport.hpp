#pragma once

#include <chrono>
#include <memory>

#include "libca/http/http_error.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"
#include "libca/net/tcp.hpp"

namespace ca::http {

struct HttpTlsServerOptions;

namespace detail {

/// @brief HTTP server 端连接传输层抽象,与 ClientTransport 对称,统一纯 TCP 与 TLS。
/// @details 读写部分由 io::Reader/io::Writer 提供;tcp_stream() 暴露底层 socket,
/// 供 DeadlineReader/DeadlineWriter 设置 socket 级超时(TLS 连接下,socket 超时
/// 通过 OpenSSL 的 SSL_ERROR_WANT_READ/WRITE + 循环重试响应)。
class ServerTransport : public io::Reader, public io::Writer
{
public:
    ~ServerTransport() override = default;

    /// @brief 返回底层 TcpStream,供 DeadlineReader/DeadlineWriter 设超时用。
    /// @note TLS transport 返回的是被 SSL 包裹的同一个 socket(SSL_set_fd 注入的那个)。
    virtual net::TcpStream& tcp_stream() noexcept = 0;
};

/// @brief 纯 TCP server transport,直接转发到 TcpStream。
class TcpServerTransport final : public ServerTransport
{
public:
    explicit TcpServerTransport(net::TcpStream stream) noexcept
        : stream_(std::move(stream))
    {}

    io::IoResult<usize> read(u8* buffer, usize capacity) override
    {
        return stream_.read(buffer, capacity);
    }

    io::IoResult<usize> write(const u8* data, usize length) override
    {
        return stream_.write(data, length);
    }

    io::IoResult<void> flush() override { return stream_.flush(); }

    net::TcpStream& tcp_stream() noexcept override { return stream_; }

private:
    net::TcpStream stream_;
};

/// @brief 判断当前构建是否包含可选 OpenSSL HTTPS server transport。
bool tls_server_available() noexcept;

/// @brief 把已 accept 的 TcpStream 包装成 TLS server transport。
/// @details 仅在启用 with_openssl 时返回有效 transport;否则返回 Unsupported 错误。
HttpResult<std::unique_ptr<ServerTransport>> make_tls_server_transport(
    net::TcpStream stream, const HttpTlsServerOptions& options,
    std::chrono::milliseconds handshake_timeout);

}   // namespace detail
}   // namespace ca::http
