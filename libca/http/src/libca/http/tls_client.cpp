#include "libca/http/detail/client_transport.hpp"

#include "libca/http/client.hpp"

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

#    include "libca/net/address.hpp"

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

class OpenSslClientTransport final : public ClientTransport
{
public:
    OpenSslClientTransport(net::TcpStream stream, SslContextPtr context, SslPtr ssl) noexcept
        : stream_(std::move(stream))
        , context_(std::move(context))
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
    SslContextPtr  context_;
    SslPtr         ssl_;
};

HttpResult<void> configure_trust(SSL_CTX* context, const HttpTlsClientOptions& options)
{
    if (!options.verify_peer) {
        SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
        return ca::core::Ok();
    }

    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
    ERR_clear_error();
    const bool custom_trust = !options.ca_file.empty() || !options.ca_directory.empty();
    const int  loaded =
        custom_trust ? SSL_CTX_load_verify_locations(
                           context,
                           options.ca_file.empty() ? nullptr : options.ca_file.c_str(),
                           options.ca_directory.empty() ? nullptr : options.ca_directory.c_str())
                      : SSL_CTX_set_default_verify_paths(context);
    if (loaded != 1)
        return ca::core::Err(tls_configuration_error("configure TLS trust store"));

    return ca::core::Ok();
}

HttpResult<void> configure_peer_identity(SSL* ssl, const std::string& host)
{
    auto* parameters = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(parameters, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    auto      parsed_ip  = net::IpAddress::parse(host);
    const int configured = parsed_ip.is_ok()
                               ? X509_VERIFY_PARAM_set1_ip_asc(parameters, host.c_str())
                               : SSL_set1_host(ssl, host.c_str());
    if (configured != 1)
        return ca::core::Err(tls_configuration_error("configure TLS peer identity"));
    return ca::core::Ok();
}

HttpResult<void> perform_handshake(SSL* ssl, net::TcpStream& stream,
                                   std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            return ca::core::Err(
                HttpError::from_io(io::IoError::from_kind(io::IoErrorKind::TimedOut,
                                                          "TLS handshake deadline exceeded"),
                                   "perform TLS handshake"));
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        if (remaining.count() == 0)
            remaining = std::chrono::milliseconds(1);
        auto configured = stream.set_read_timeout(remaining);
        if (configured.is_err())
            return ca::core::Err(HttpError::from_io(std::move(configured).unwrap_err(),
                                                    "configure TLS handshake read timeout"));
        auto write_timeout = stream.set_write_timeout(remaining);
        if (write_timeout.is_err())
            return ca::core::Err(HttpError::from_io(std::move(write_timeout).unwrap_err(),
                                                    "configure TLS handshake write timeout"));

        ERR_clear_error();
        clear_native_socket_error();
        const int result = SSL_connect(ssl);
        if (result == 1)
            return ca::core::Ok();
        const auto kind = SSL_get_error(ssl, result);
        if (kind == SSL_ERROR_WANT_READ || kind == SSL_ERROR_WANT_WRITE)
            continue;
        auto error = ssl_io_error(ssl, result, "TLS handshake");
        if (error.kind() == io::IoErrorKind::Interrupted ||
            error.kind() == io::IoErrorKind::WouldBlock)
            continue;
        return ca::core::Err(HttpError::from_io(std::move(error), "perform TLS handshake"));
    }
}

}   // namespace

bool tls_client_available() noexcept
{
    return true;
}

HttpResult<std::unique_ptr<ClientTransport>> make_tls_client_transport(
    net::TcpStream stream, const std::string& host, const HttpTlsClientOptions& options,
    std::chrono::milliseconds handshake_timeout)
{
    ERR_clear_error();
    SslContextPtr context(SSL_CTX_new(TLS_client_method()));
    if (context == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS client context"));
    if (SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) != 1)
        return ca::core::Err(tls_configuration_error("set minimum TLS version"));
    auto trust = configure_trust(context.get(), options);
    if (trust.is_err())
        return ca::core::Err(std::move(trust).unwrap_err());

    SslPtr ssl(SSL_new(context.get()));
    if (ssl == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS connection"));
    SSL_set_mode(ssl.get(), SSL_MODE_AUTO_RETRY);

    if (options.verify_peer) {
        auto identity = configure_peer_identity(ssl.get(), host);
        if (identity.is_err())
            return ca::core::Err(std::move(identity).unwrap_err());
    }

    auto parsed_ip = net::IpAddress::parse(host);
    if (parsed_ip.is_err() && SSL_set_tlsext_host_name(ssl.get(), host.c_str()) != 1)
        return ca::core::Err(tls_configuration_error("configure TLS SNI"));

    static constexpr unsigned char HTTP11_ALPN[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
    if (SSL_set_alpn_protos(ssl.get(), HTTP11_ALPN, sizeof(HTTP11_ALPN)) != 0)
        return ca::core::Err(tls_configuration_error("configure TLS ALPN"));

    const auto raw_socket = stream.native_socket();
    if (raw_socket > static_cast<net::RawSocket>(INT_MAX))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::Unsupported, "native socket value cannot be represented by OpenSSL"));
    if (SSL_set_fd(ssl.get(), static_cast<int>(raw_socket)) != 1)
        return ca::core::Err(tls_configuration_error("attach TLS socket"));

    auto handshaken = perform_handshake(ssl.get(), stream, handshake_timeout);
    if (handshaken.is_err())
        return ca::core::Err(std::move(handshaken).unwrap_err());
    if (options.verify_peer && SSL_get_verify_result(ssl.get()) != X509_V_OK)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "TLS peer certificate verification failed"));

    const unsigned char* selected_protocol = nullptr;
    unsigned int         selected_length   = 0;
    SSL_get0_alpn_selected(ssl.get(), &selected_protocol, &selected_length);
    constexpr std::string_view HTTP11 = "http/1.1";
    if (selected_length != 0 && (selected_length != HTTP11.size() ||
                                 std::memcmp(selected_protocol, HTTP11.data(), HTTP11.size()) != 0))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                                  "TLS peer selected an unsupported ALPN"));

    std::unique_ptr<ClientTransport> transport = std::make_unique<OpenSslClientTransport>(
        std::move(stream), std::move(context), std::move(ssl));
    return ca::core::Ok(std::move(transport));
}

}   // namespace ca::http::detail

#else

namespace ca::http::detail {

bool tls_client_available() noexcept
{
    return false;
}

HttpResult<std::unique_ptr<ClientTransport>> make_tls_client_transport(net::TcpStream,
                                                                       const std::string&,
                                                                       const HttpTlsClientOptions&,
                                                                       std::chrono::milliseconds)
{
    return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                              "https requires a build with OpenSSL enabled"));
}

}   // namespace ca::http::detail

#endif
