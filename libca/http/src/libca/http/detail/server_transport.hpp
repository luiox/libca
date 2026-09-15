#pragma once

#include <chrono>
#include <memory>

#include "libca/http/http_error.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"
#include "libca/net/tcp.hpp"
#include "libca/net/tls_stream.hpp"
#include "libca/thread/stop_token.hpp"

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

/// @brief 长期持有的 server 端 TLS 配置上下文。
/// @details 实质实现是 net::TlsServerContext（包装 OpenSSL SSL_CTX，只在启用
/// with_openssl 时有效）。server 启动时构造一次，后续所有 accept 出的连接共享
/// 此上下文做 TLS 握手。
class ServerTlsContext
{
public:
    ServerTlsContext() noexcept;
    ~ServerTlsContext();
    ServerTlsContext(const ServerTlsContext&)            = delete;
    ServerTlsContext& operator=(const ServerTlsContext&) = delete;
    ServerTlsContext(ServerTlsContext&&) noexcept;
    ServerTlsContext& operator=(ServerTlsContext&&) noexcept;

    /// @brief 加载 PEM 证书链与私钥,并按 options 配置客户端证书校验。
    /// @details 仅在启用 with_openssl 时执行实质加载;否则返回 Unsupported。
    HttpResult<void> load(const HttpTlsServerOptions& options);

    /// @brief 内部 SSL_CTX 句柄(未启用 with_openssl 时返回 nullptr)。
    void* native_handle() const noexcept;

    /// @brief 内部 net 层上下文(detail 内部握手入口使用)。
    const net::TlsServerContext& net_context() const noexcept;

private:
    net::TlsServerContext context_;   // TLS 实质配置见 libca/net/tls_stream.hpp。
};

/// @brief 判断当前构建是否包含可选 OpenSSL HTTPS server transport。
bool tls_server_available() noexcept;

/// @brief 在已 accept 的 TcpStream 上执行 TLS 服务端握手,返回 TLS transport。
/// @details 共享 server 启动时构造的 ServerTlsContext(只读 SSL_CTX,线程安全)。
/// 仅在启用 with_openssl 时返回有效 transport;否则返回 Unsupported 错误。
HttpResult<std::unique_ptr<ServerTransport>> make_tls_server_transport(
    net::TcpStream stream, const ServerTlsContext& context,
    std::chrono::milliseconds handshake_timeout, ca::thread::StopToken stop_token,
    std::chrono::milliseconds stop_poll_interval);

}   // namespace detail
}   // namespace ca::http
