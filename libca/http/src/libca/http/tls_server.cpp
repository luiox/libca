#include "libca/http/detail/server_transport.hpp"

#include "libca/http/server.hpp"

#if defined(LIBCA_HTTP_HAS_OPENSSL)

#    include <algorithm>
#    include <cerrno>
#    include <climits>
#    include <cstring>
#    include <memory>
#    include <string>
#    include <string_view>
#    include <utility>

#    if defined(_WIN32)
#        define WIN32_LEAN_AND_MEAN
#        define NOMINMAX
#        include <winsock2.h>
#    endif

#    include <openssl/err.h>
#    include <openssl/ssl.h>
#    include <openssl/sslerr.h>
#    include <openssl/x509v3.h>

namespace ca::http::detail {
namespace {

struct SslContextDeleter
{
    void operator()(SSL_CTX* context) const noexcept
    {
        if (context != nullptr)
            SSL_CTX_free(context);
    }
};

struct SslDeleter
{
    void operator()(SSL* ssl) const noexcept
    {
        if (ssl != nullptr)
            SSL_free(ssl);
    }
};

using SslContextPtr = std::unique_ptr<SSL_CTX, SslContextDeleter>;
using SslPtr        = std::unique_ptr<SSL, SslDeleter>;

void clear_native_socket_error() noexcept
{
#    if defined(_WIN32)
    WSASetLastError(0);
#    else
    errno = 0;
#    endif
}

i64 last_native_socket_error() noexcept
{
#    if defined(_WIN32)
    return static_cast<i64>(WSAGetLastError());
#    else
    return static_cast<i64>(errno);
#    endif
}

std::string openssl_error_message()
{
    const auto code = ERR_get_error();
    if (code == 0)
        return {};
    char buffer[256]{};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return std::string(buffer);
}

io::IoError ssl_io_error(SSL* ssl, int result, const char* operation)
{
    const auto ssl_error = SSL_get_error(ssl, result);
    if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
        return io::IoError::from_kind(io::IoErrorKind::WouldBlock,
                                      std::string(operation) + " would block");
    if (ssl_error == SSL_ERROR_SYSCALL) {
        const auto native_error = last_native_socket_error();
        if (native_error != 0)
            return io::IoError::from_native_error(native_error, operation);
        const auto detail = openssl_error_message();
        if (!detail.empty())
            return io::IoError::from_kind(io::IoErrorKind::InvalidData,
                                          std::string(operation) + " failed: " + detail);
        return io::IoError::from_kind(io::IoErrorKind::UnexpectedEof,
                                      std::string(operation) + " reached an unclean TLS EOF");
    }
#    if defined(SSL_R_UNEXPECTED_EOF_WHILE_READING)
    if (ssl_error == SSL_ERROR_SSL) {
        const auto code = ERR_peek_last_error();
        if (ERR_GET_LIB(code) == ERR_LIB_SSL &&
            ERR_GET_REASON(code) == SSL_R_UNEXPECTED_EOF_WHILE_READING) {
            ERR_clear_error();
            return io::IoError::from_kind(io::IoErrorKind::UnexpectedEof,
                                          std::string(operation) + " reached an unclean TLS EOF");
        }
    }
#    endif

    const auto detail  = openssl_error_message();
    const auto message = detail.empty() ? std::string(operation) + " failed"
                                        : std::string(operation) + " failed: " + detail;
    return io::IoError::from_kind(
        ssl_error == SSL_ERROR_SSL ? io::IoErrorKind::InvalidData : io::IoErrorKind::Other,
        message);
}

HttpError tls_configuration_error(const char* operation)
{
    const auto detail = openssl_error_message();
    return HttpError::from_kind(HttpErrorKind::InvalidState,
                                detail.empty() ? std::string(operation) + " failed"
                                               : std::string(operation) + " failed: " + detail);
}

class OpenSslServerTransport final : public ServerTransport
{
public:
    OpenSslServerTransport(net::TcpStream stream, SslPtr ssl) noexcept
        : stream_(std::move(stream))
        , ssl_(std::move(ssl))
    {}

    io::IoResult<usize> read(u8* buffer, usize capacity) override
    {
        if (capacity == 0)
            return ca::core::Ok<usize>(0);
        const auto requested = static_cast<int>(std::min<usize>(capacity, INT_MAX));
        ERR_clear_error();
        clear_native_socket_error();
        const int result = SSL_read(ssl_.get(), buffer, requested);
        if (result > 0)
            return ca::core::Ok(static_cast<usize>(result));
        if (SSL_get_error(ssl_.get(), result) == SSL_ERROR_ZERO_RETURN)
            return ca::core::Ok<usize>(0);
        return ca::core::Err(ssl_io_error(ssl_.get(), result, "read TLS stream"));
    }

    io::IoResult<usize> write(const u8* data, usize length) override
    {
        if (length == 0)
            return ca::core::Ok<usize>(0);
        const auto requested = static_cast<int>(std::min<usize>(length, INT_MAX));
        ERR_clear_error();
        clear_native_socket_error();
        const int result = SSL_write(ssl_.get(), data, requested);
        if (result > 0)
            return ca::core::Ok(static_cast<usize>(result));
        return ca::core::Err(ssl_io_error(ssl_.get(), result, "write TLS stream"));
    }

    io::IoResult<void> flush() override { return ca::core::Ok(); }

    net::TcpStream& tcp_stream() noexcept override { return stream_; }

private:
    net::TcpStream stream_;
    SslPtr         ssl_;
};

HttpResult<void> perform_handshake(SSL* ssl, net::TcpStream& stream,
                                   std::chrono::milliseconds timeout,
                                   ca::thread::StopToken     stop_token,
                                   std::chrono::milliseconds stop_poll_interval)
{
    // 对称 tls_client.cpp 的 perform_handshake:用底层 socket 超时驱动,
    // SSL_ERROR_WANT_READ/WRITE 时循环重试。SSL_connect 在此换为 SSL_accept。
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        if (stop_token.stop_requested())
            return ca::core::Err(
                HttpError::from_io(io::IoError::from_kind(io::IoErrorKind::ConnectionAborted,
                                                          "TLS handshake cancelled by server stop"),
                                   "perform TLS handshake"));
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            return ca::core::Err(
                HttpError::from_io(io::IoError::from_kind(io::IoErrorKind::TimedOut,
                                                          "TLS handshake deadline exceeded"),
                                   "perform TLS handshake"));
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        if (remaining.count() == 0)
            remaining = std::chrono::milliseconds(1);
        const auto wait =
            stop_token.stop_possible() ? std::min(remaining, stop_poll_interval) : remaining;
        auto configured = stream.set_read_timeout(wait);
        if (configured.is_err())
            return ca::core::Err(HttpError::from_io(std::move(configured).unwrap_err(),
                                                    "configure TLS handshake read timeout"));
        auto write_timeout = stream.set_write_timeout(wait);
        if (write_timeout.is_err())
            return ca::core::Err(HttpError::from_io(std::move(write_timeout).unwrap_err(),
                                                    "configure TLS handshake write timeout"));

        ERR_clear_error();
        clear_native_socket_error();
        const int result = SSL_accept(ssl);
        if (result == 1)
            return ca::core::Ok();
        const auto kind = SSL_get_error(ssl, result);
        if (kind == SSL_ERROR_WANT_READ || kind == SSL_ERROR_WANT_WRITE)
            continue;
        auto error = ssl_io_error(ssl, result, "TLS handshake");
        if (error.kind() == io::IoErrorKind::Interrupted ||
            error.kind() == io::IoErrorKind::WouldBlock ||
            error.kind() == io::IoErrorKind::TimedOut)
            continue;
        return ca::core::Err(HttpError::from_io(std::move(error), "perform TLS handshake"));
    }
}

}   // namespace

// ServerTlsContext 在启用 OpenSSL 时,impl_ 是 SSL_CTX*。
ServerTlsContext::ServerTlsContext() noexcept = default;

ServerTlsContext::~ServerTlsContext()
{
    if (impl_ != nullptr) {
        SslContextDeleter{}(static_cast<SSL_CTX*>(impl_));
        impl_ = nullptr;
    }
}

ServerTlsContext::ServerTlsContext(ServerTlsContext&& other) noexcept
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

ServerTlsContext& ServerTlsContext::operator=(ServerTlsContext&& other) noexcept
{
    if (this != &other) {
        if (impl_ != nullptr)
            SslContextDeleter{}(static_cast<SSL_CTX*>(impl_));
        impl_       = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

HttpResult<void> ServerTlsContext::load(const HttpTlsServerOptions& options)
{
    ERR_clear_error();
    SslContextPtr context(SSL_CTX_new(TLS_server_method()));
    if (context == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS server context"));
    if (SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) != 1)
        return ca::core::Err(tls_configuration_error("set minimum TLS version"));

    // 证书链 + 私钥:PEM 格式。SSL_CTX_use_certificate_chain_file 会加载 leaf
    // 及其后的 intermediate,要求证书文件按 leaf-first 顺序排列。
    if (SSL_CTX_use_certificate_chain_file(context.get(),
                                            options.certificate_chain_file.c_str()) != 1)
        return ca::core::Err(tls_configuration_error("load TLS certificate chain"));
    if (SSL_CTX_use_PrivateKey_file(context.get(), options.private_key_file.c_str(),
                                     SSL_FILETYPE_PEM) != 1)
        return ca::core::Err(tls_configuration_error("load TLS private key"));
    if (SSL_CTX_check_private_key(context.get()) != 1)
        return ca::core::Err(tls_configuration_error("verify TLS private key matches certificate"));

    // 可选:校验客户端证书。verify_client=true 时要求客户端提供证书;
    // 配合 ca_file/ca_directory 指定信任 CA。
    if (options.verify_client) {
        ERR_clear_error();
        const bool custom_trust = !options.ca_file.empty() || !options.ca_directory.empty();
        const int  loaded =
            custom_trust
                 ? SSL_CTX_load_verify_locations(
                      context.get(),
                      options.ca_file.empty() ? nullptr : options.ca_file.c_str(),
                      options.ca_directory.empty() ? nullptr : options.ca_directory.c_str())
                 : SSL_CTX_set_default_verify_paths(context.get());
        if (loaded != 1)
            return ca::core::Err(tls_configuration_error("configure TLS client CA"));
        SSL_CTX_set_verify(
            context.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    }
    else {
        SSL_CTX_set_verify(context.get(), SSL_VERIFY_NONE, nullptr);
    }

    // 释放旧 ctx(若有),保存新的。load 只应调用一次。
    if (impl_ != nullptr)
        SslContextDeleter{}(static_cast<SSL_CTX*>(impl_));
    impl_ = context.release();
    return ca::core::Ok();
}

void* ServerTlsContext::native_handle() const noexcept
{
    return impl_;
}

bool tls_server_available() noexcept
{
    return true;
}

HttpResult<std::unique_ptr<ServerTransport>> make_tls_server_transport(
    net::TcpStream stream, const ServerTlsContext& context,
    std::chrono::milliseconds handshake_timeout, ca::thread::StopToken stop_token,
    std::chrono::milliseconds stop_poll_interval)
{
    auto* ctx = static_cast<SSL_CTX*>(context.native_handle());
    if (ctx == nullptr)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "TLS server context is not initialized"));

    ERR_clear_error();
    SslPtr ssl(SSL_new(ctx));
    if (ssl == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS connection"));
    SSL_set_mode(ssl.get(), SSL_MODE_AUTO_RETRY);

    // 把已 accept 的 socket 绑定到 SSL 对象。socket 数值范围检查对齐 tls_client.cpp。
    const auto raw_socket = stream.native_socket();
    if (raw_socket > static_cast<net::RawSocket>(INT_MAX))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::Unsupported, "native socket value cannot be represented by OpenSSL"));
    if (SSL_set_fd(ssl.get(), static_cast<int>(raw_socket)) != 1)
        return ca::core::Err(tls_configuration_error("attach TLS socket"));

    auto handshaken = perform_handshake(
        ssl.get(), stream, handshake_timeout, std::move(stop_token), stop_poll_interval);
    if (handshaken.is_err())
        return ca::core::Err(std::move(handshaken).unwrap_err());

    std::unique_ptr<ServerTransport> transport =
        std::make_unique<OpenSslServerTransport>(std::move(stream), std::move(ssl));
    return ca::core::Ok(std::move(transport));
}

}   // namespace ca::http::detail

#else

namespace ca::http::detail {

ServerTlsContext::ServerTlsContext() noexcept                      = default;
ServerTlsContext::~ServerTlsContext()                              = default;
ServerTlsContext::ServerTlsContext(ServerTlsContext&&) noexcept    = default;
ServerTlsContext& ServerTlsContext::operator=(ServerTlsContext&&) noexcept = default;

HttpResult<void> ServerTlsContext::load(const HttpTlsServerOptions&)
{
    return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                              "https requires a build with OpenSSL enabled"));
}

void* ServerTlsContext::native_handle() const noexcept
{
    return nullptr;
}

bool tls_server_available() noexcept
{
    return false;
}

HttpResult<std::unique_ptr<ServerTransport>> make_tls_server_transport(
    net::TcpStream, const ServerTlsContext&, std::chrono::milliseconds, ca::thread::StopToken,
    std::chrono::milliseconds)
{
    return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                              "https requires a build with OpenSSL enabled"));
}

}   // namespace ca::http::detail

#endif
