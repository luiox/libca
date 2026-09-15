#include "libca/net/tls_stream.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <memory>
#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#endif

#include "libca/net/address.hpp"
#include "libca/str/format.hpp"

#if defined(LIBCA_NET_HAS_OPENSSL)

#    include <openssl/err.h>
#    include <openssl/ssl.h>
#    include <openssl/sslerr.h>
#    include <openssl/x509v3.h>

namespace ca::net {
namespace {

// OpenSSL 句柄 RAII：SSL_set_bio 之后 rbio/wbio 由 SSL 对象接管释放。
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

// 单次 memory BIO 搬运缓冲：一个 TLS record 上限 16KB，一次搬运足够。
constexpr usize kCipherChunkSize = 16384;

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

// 配置阶段的错误（建 ctx、加载证书等）：无网络语义，归为 InvalidInput。
io::IoError tls_configuration_error(const char* operation)
{
    const auto detail = openssl_error_message();
    return io::IoError::from_kind(
        io::IoErrorKind::InvalidInput,
        detail.empty() ? ca::str::format_std("{} failed", operation)
                       : ca::str::format_std("{} failed: {}", operation, detail));
}

// 把 SSL 操作失败翻译成稳定 IoErrorKind：SYSCALL 走原生错误码（连接重置等
// 语义由底层码映射），干净的 Unexpected EOF 与对端断开归 UnexpectedEof，
// 其余 SSL 层错误（协议告警、记录损坏等）归 InvalidData。
io::IoError ssl_io_error(SSL* ssl, int result, const char* operation)
{
    const auto ssl_error = SSL_get_error(ssl, result);
    if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
        return io::IoError::from_kind(io::IoErrorKind::WouldBlock,
                                      ca::str::format_std("{} would block", operation));
    if (ssl_error == SSL_ERROR_SYSCALL) {
        const auto native_error = last_native_socket_error();
        if (native_error != 0)
            return io::IoError::from_native_error(native_error, operation);
        const auto detail = openssl_error_message();
        if (!detail.empty())
            return io::IoError::from_kind(io::IoErrorKind::InvalidData,
                                          ca::str::format_std("{} failed: {}", operation, detail));
        return io::IoError::from_kind(io::IoErrorKind::UnexpectedEof,
                                      ca::str::format_std("{} reached an unclean TLS EOF", operation));
    }
#    if defined(SSL_R_UNEXPECTED_EOF_WHILE_READING)
    if (ssl_error == SSL_ERROR_SSL) {
        const auto code = ERR_peek_last_error();
        if (ERR_GET_LIB(code) == ERR_LIB_SSL &&
            ERR_GET_REASON(code) == SSL_R_UNEXPECTED_EOF_WHILE_READING) {
            ERR_clear_error();
            return io::IoError::from_kind(io::IoErrorKind::UnexpectedEof,
                                          ca::str::format_std("{} reached an unclean TLS EOF", operation));
        }
    }
#    endif

    const auto detail  = openssl_error_message();
    const auto message = detail.empty() ? ca::str::format_std("{} failed", operation)
                                        : ca::str::format_std("{} failed: {}", operation, detail);
    return io::IoError::from_kind(io::IoErrorKind::InvalidData, message);
}

bool would_block_kind(const io::IoError& error) noexcept
{
    const auto kind = error.kind();
    return kind == io::IoErrorKind::WouldBlock || kind == io::IoErrorKind::TimedOut ||
           kind == io::IoErrorKind::Interrupted;
}

}   // namespace

struct TlsStream::Impl
{
    ~Impl()
    {
        if (ssl != nullptr)
            SslDeleter{}(std::exchange(ssl, nullptr));
    }

    SSL* ssl{nullptr};
};

TlsStream::TlsStream() noexcept = default;

TlsServerContext::TlsServerContext() noexcept  = default;
TlsServerContext::~TlsServerContext()          = default;
TlsServerContext::TlsServerContext(TlsServerContext&&) noexcept            = default;
TlsServerContext& TlsServerContext::operator=(TlsServerContext&&) noexcept = default;

struct TlsServerContext::Impl
{
    SslContextPtr context;
};

io::IoResult<void> TlsServerContext::load(const TlsServerOptions& options)
{
    if (!tls_stream_supported())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::Unsupported,
            "TLS requires a build with OpenSSL enabled (xmake f --with_openssl=y)"));
    ERR_clear_error();
    auto context = SslContextPtr(SSL_CTX_new(TLS_server_method()));
    if (context == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS server context"));
    if (SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) != 1)
        return ca::core::Err(tls_configuration_error("set minimum TLS version"));

    // 证书链 + 私钥：PEM 格式。use_certificate_chain_file 要求 leaf 在前。
    if (SSL_CTX_use_certificate_chain_file(context.get(),
                                          options.certificate_chain_file.c_str()) != 1)
        return ca::core::Err(tls_configuration_error("load TLS certificate chain"));
    if (SSL_CTX_use_PrivateKey_file(context.get(), options.private_key_file.c_str(),
                                    SSL_FILETYPE_PEM) != 1)
        return ca::core::Err(tls_configuration_error("load TLS private key"));
    if (SSL_CTX_check_private_key(context.get()) != 1)
        return ca::core::Err(tls_configuration_error("verify TLS private key matches certificate"));

    if (options.verify_client) {
        ERR_clear_error();
        const bool custom_trust = !options.ca_file.empty() || !options.ca_directory.empty();
        const int  loaded =
            custom_trust ? SSL_CTX_load_verify_locations(
                               context.get(),
                               options.ca_file.empty() ? nullptr : options.ca_file.c_str(),
                               options.ca_directory.empty() ? nullptr : options.ca_directory.c_str())
                         : SSL_CTX_set_default_verify_paths(context.get());
        if (loaded != 1)
            return ca::core::Err(tls_configuration_error("configure TLS client CA"));
        SSL_CTX_set_verify(context.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    }
    else {
        SSL_CTX_set_verify(context.get(), SSL_VERIFY_NONE, nullptr);
    }

    impl_                 = std::make_unique<Impl>();
    impl_->context        = std::move(context);
    return ca::core::Ok();
}

void* TlsServerContext::native_handle() const noexcept
{
    return impl_ != nullptr ? impl_->context.get() : nullptr;
}

bool tls_stream_supported() noexcept
{
    return true;
}

TlsStream::TlsStream(TlsStream&& other) noexcept
    : input_(std::exchange(other.input_, nullptr))
    , output_(std::exchange(other.output_, nullptr))
    , impl_(std::move(other.impl_))
{}

TlsStream& TlsStream::operator=(TlsStream&& other) noexcept
{
    if (this != &other) {
        input_  = std::exchange(other.input_, nullptr);
        output_ = std::exchange(other.output_, nullptr);
        impl_   = std::move(other.impl_);
    }
    return *this;
}

TlsStream::~TlsStream() = default;

bool TlsStream::is_open() const noexcept
{
    return impl_ != nullptr;
}

std::optional<std::string> TlsStream::alpn_selected() const
{
    if (impl_ == nullptr)
        return std::nullopt;
    const unsigned char* protocol = nullptr;
    unsigned int         length   = 0;
    SSL_get0_alpn_selected(impl_->ssl, &protocol, &length);
    if (length == 0)
        return std::nullopt;
    return std::string(reinterpret_cast<const char*>(protocol), length);
}

namespace {

// 把写 BIO 里累积的密文全部刷入底层流。密文不出去，对端就收不到任何 TLS 消息，
// 因此所有调用点都把刷写失败当作致命错误向上传播。
io::IoResult<void> flush_write_bio(SSL* ssl, io::Writer& output)
{
    u8 chunk[kCipherChunkSize];
    while (BIO_ctrl_pending(SSL_get_wbio(ssl)) > 0) {
        const int count = BIO_read(SSL_get_wbio(ssl), chunk, static_cast<int>(sizeof(chunk)));
        if (count <= 0)
            return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidData,
                                                        "flush TLS cipher unexpectedly failed"));
        auto written = output.write_all(chunk, static_cast<usize>(count));
        if (written.is_err())
            return ca::core::Err(std::move(written).unwrap_err());
    }
    return ca::core::Ok();
}

}   // namespace

namespace {

// TLS 握手循环：SSL_connect/SSL_accept 返回 WANT_READ/WANT_WRITE 时搬运密文后重试。
// 底层流的 TimedOut/WouldBlock 不终止握手（server 的 stop 轮询依赖周期性超时），
// 只在迭代边界受总期限约束；before_retry 钩子用于收紧 socket 超时与检查协作停止。
io::IoResult<void> perform_handshake(SSL* ssl, io::Reader& input, io::Writer& output,
                                     const TlsHandshakeControl& control, bool is_server)
{
    const auto deadline = std::chrono::steady_clock::now() + control.timeout;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::TimedOut,
                                                        "TLS handshake deadline exceeded"));
        if (control.before_retry) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            if (remaining.count() == 0)
                remaining = std::chrono::milliseconds(1);
            auto hooked = control.before_retry(remaining);
            if (hooked.is_err())
                return ca::core::Err(std::move(hooked).unwrap_err());
        }

        ERR_clear_error();
        clear_native_socket_error();
        const int result = is_server ? SSL_accept(ssl) : SSL_connect(ssl);
        if (result == 1)
            return flush_write_bio(ssl, output);
        const auto kind = SSL_get_error(ssl, result);
        if (kind == SSL_ERROR_WANT_READ || kind == SSL_ERROR_WANT_WRITE) {
            auto flushed = flush_write_bio(ssl, output);
            if (flushed.is_err())
                return flushed;
            u8 chunk[kCipherChunkSize];
            auto received = input.read(chunk, sizeof(chunk));
            if (received.is_ok() && received.unwrap() > 0) {
                BIO_write(SSL_get_rbio(ssl), chunk, static_cast<int>(received.unwrap()));
                continue;
            }
            if (received.is_ok())
                return ca::core::Err(io::IoError::from_kind(
                    io::IoErrorKind::UnexpectedEof,
                    "TLS handshake reached an unclean EOF on the underlying stream"));
            auto error = std::move(received).unwrap_err();
            // 底层暂时无数据：回到循环顶部重设超时并重试，直到总期限到期。
            if (would_block_kind(error))
                continue;
            return ca::core::Err(std::move(error));
        }
        return ca::core::Err(ssl_io_error(ssl, result, is_server ? "TLS server handshake"
                                                                 : "TLS client handshake"));
    }
}

// 配置信任库：verify_peer 开启时优先使用自定义 CA，否则走 OpenSSL 默认信任路径。
io::IoResult<void> configure_trust(SSL_CTX* context, const TlsClientOptions& options)
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

// 主机名 / IP 校验身份：X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS 对齐主流 client 行为。
io::IoResult<void> configure_peer_identity(SSL* ssl, const std::string& host)
{
    auto* parameters = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(parameters, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    const auto parsed_ip  = IpAddress::parse(host);
    const int  configured = parsed_ip.is_ok()
                                ? X509_VERIFY_PARAM_set1_ip_asc(parameters, host.c_str())
                                : SSL_set1_host(ssl, host.c_str());
    if (configured != 1)
        return ca::core::Err(tls_configuration_error("configure TLS peer identity"));
    return ca::core::Ok();
}

}   // namespace

io::IoResult<TlsStream> TlsStream::connect(io::Reader& input, io::Writer& output,
                                           const std::string&      host,
                                           const TlsClientOptions& options,
                                           const TlsHandshakeControl& control)
{
    if (!tls_stream_supported())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::Unsupported,
            "TLS requires a build with OpenSSL enabled (xmake f --with_openssl=y)"));
    ERR_clear_error();
    auto context = SslContextPtr(SSL_CTX_new(TLS_client_method()));
    if (context == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS client context"));
    // 最低版本固定 TLS 1.2；如后续需要配置化，扩展 TlsClientOptions 即可。
    if (SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) != 1)
        return ca::core::Err(tls_configuration_error("set minimum TLS version"));
    auto trust = configure_trust(context.get(), options);
    if (trust.is_err())
        return ca::core::Err(std::move(trust).unwrap_err());

    auto ssl = SslPtr(SSL_new(context.get()));
    if (ssl == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS connection"));
    SSL_set_mode(ssl.get(), SSL_MODE_AUTO_RETRY);

    const auto parsed_ip = IpAddress::parse(host);
    if (options.verify_peer) {
        auto identity = configure_peer_identity(ssl.get(), host);
        if (identity.is_err())
            return ca::core::Err(std::move(identity).unwrap_err());
    }
    // SNI 默认取 host；IP 字面量不发 SNI（对齐主流 client 语义）。
    if (parsed_ip.is_err() && SSL_set_tlsext_host_name(ssl.get(), host.c_str()) != 1)
        return ca::core::Err(tls_configuration_error("configure TLS SNI"));

    if (!options.alpn_protocols.empty()) {
        // ALPN wire format：每个协议前缀 1 字节长度。
        std::string wire;
        for (const auto& protocol : options.alpn_protocols) {
            if (protocol.empty() || protocol.size() > 255)
                return ca::core::Err(io::IoError::from_kind(
                    io::IoErrorKind::InvalidInput, "configure TLS ALPN: invalid protocol length"));
            wire.push_back(static_cast<char>(protocol.size()));
            wire.append(protocol);
        }
        // 注意：SSL_set_alpn_protos 成功返回 0。
        if (SSL_set_alpn_protos(ssl.get(), reinterpret_cast<const unsigned char*>(wire.data()),
                                static_cast<unsigned int>(wire.size())) != 0)
            return ca::core::Err(tls_configuration_error("configure TLS ALPN"));
    }

    auto read_bio  = BIO_new(BIO_s_mem());
    auto write_bio = BIO_new(BIO_s_mem());
    if (read_bio == nullptr || write_bio == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS memory BIO"));
    // SSL 接管两个 memory BIO 的所有权。
    SSL_set_bio(ssl.get(), read_bio, write_bio);

    TlsStream stream;
    stream.input_ = &input;
    stream.output_ = &output;
    stream.impl_   = std::make_unique<Impl>();
    stream.impl_->ssl = ssl.release();

    auto handshaken = perform_handshake(stream.impl_->ssl, input, output, control, false);
    if (handshaken.is_err())
        return ca::core::Err(std::move(handshaken).unwrap_err());
    if (options.verify_peer && SSL_get_verify_result(stream.impl_->ssl) != X509_V_OK) {
        const auto reason = X509_verify_cert_error_string(SSL_get_verify_result(stream.impl_->ssl));
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidData,
            ca::str::format_std("TLS peer certificate verification failed: {}", reason)));
    }
    return ca::core::Ok(std::move(stream));
}

io::IoResult<TlsStream> TlsStream::accept(const TlsServerContext&    context,
                                          io::Reader& input, io::Writer& output,
                                          const TlsHandshakeControl& control)
{
    if (!tls_stream_supported())
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::Unsupported,
            "TLS requires a build with OpenSSL enabled (xmake f --with_openssl=y)"));
    const auto* server_context = static_cast<const SSL_CTX*>(context.native_handle());
    if (server_context == nullptr)
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "TLS server context is not initialized"));

    ERR_clear_error();
    // SSL_new 的签名不接收 const；上下文在握手期间只被共享读取（OpenSSL 约定）。
    auto ssl = SslPtr(SSL_new(const_cast<SSL_CTX*>(server_context)));
    if (ssl == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS connection"));
    SSL_set_mode(ssl.get(), SSL_MODE_AUTO_RETRY);

    auto read_bio  = BIO_new(BIO_s_mem());
    auto write_bio = BIO_new(BIO_s_mem());
    if (read_bio == nullptr || write_bio == nullptr)
        return ca::core::Err(tls_configuration_error("create TLS memory BIO"));
    SSL_set_bio(ssl.get(), read_bio, write_bio);

    TlsStream stream;
    stream.input_ = &input;
    stream.output_ = &output;
    stream.impl_   = std::make_unique<Impl>();
    stream.impl_->ssl = ssl.release();

    auto handshaken = perform_handshake(stream.impl_->ssl, input, output, control, true);
    if (handshaken.is_err())
        return ca::core::Err(std::move(handshaken).unwrap_err());
    return ca::core::Ok(std::move(stream));
}

io::IoResult<usize> TlsStream::read(u8* buffer, usize capacity)
{
    if (capacity == 0)
        return ca::core::Ok<usize>(0);
    if (impl_ == nullptr)
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::NotConnected, "TLS stream is not connected"));
    auto* ssl = impl_->ssl;
    for (;;) {
        ERR_clear_error();
        clear_native_socket_error();
        const auto requested = static_cast<int>(std::min<usize>(capacity, INT_MAX));
        const int  result    = SSL_read(ssl, buffer, requested);
        if (result > 0) {
            auto flushed = flush_write_bio(ssl, *output_);
            if (flushed.is_err())
                return ca::core::Err(std::move(flushed).unwrap_err());
            return ca::core::Ok(static_cast<usize>(result));
        }
        // 对端发送 close_notify：干净关闭，语义为 EOF。
        if (result <= 0 && SSL_get_error(ssl, result) == SSL_ERROR_ZERO_RETURN)
            return ca::core::Ok<usize>(0);
        const auto kind = SSL_get_error(ssl, result);
        if (kind == SSL_ERROR_WANT_READ || kind == SSL_ERROR_WANT_WRITE) {
            auto flushed = flush_write_bio(ssl, *output_);
            if (flushed.is_err())
                return ca::core::Err(std::move(flushed).unwrap_err());
            u8 chunk[kCipherChunkSize];
            auto received = input_->read(chunk, sizeof(chunk));
            if (received.is_ok() && received.unwrap() > 0) {
                BIO_write(SSL_get_rbio(ssl), chunk, static_cast<int>(received.unwrap()));
                continue;
            }
            if (received.is_ok())
                return ca::core::Err(io::IoError::from_kind(
                    io::IoErrorKind::UnexpectedEof,
                    "read TLS stream reached an unclean TLS EOF"));
            return ca::core::Err(std::move(received).unwrap_err());
        }
        return ca::core::Err(ssl_io_error(ssl, result, "read TLS stream"));
    }
}

io::IoResult<usize> TlsStream::write(const u8* data, usize length)
{
    if (length == 0)
        return ca::core::Ok<usize>(0);
    if (impl_ == nullptr)
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::NotConnected, "TLS stream is not connected"));
    auto* ssl = impl_->ssl;
    for (;;) {
        ERR_clear_error();
        clear_native_socket_error();
        const auto requested = static_cast<int>(std::min<usize>(length, INT_MAX));
        const int  result    = SSL_write(ssl, data, requested);
        if (result > 0) {
            auto flushed = flush_write_bio(ssl, *output_);
            if (flushed.is_err())
                return ca::core::Err(std::move(flushed).unwrap_err());
            return ca::core::Ok(static_cast<usize>(result));
        }
        const auto kind = SSL_get_error(ssl, result);
        if (kind == SSL_ERROR_WANT_WRITE || kind == SSL_ERROR_WANT_READ) {
            // WANT_READ 出现在 renegotiation：读入对端握手密文后重试。
            auto flushed = flush_write_bio(ssl, *output_);
            if (flushed.is_err())
                return ca::core::Err(std::move(flushed).unwrap_err());
            if (kind == SSL_ERROR_WANT_READ) {
                u8 chunk[kCipherChunkSize];
                auto received = input_->read(chunk, sizeof(chunk));
                if (received.is_ok() && received.unwrap() > 0) {
                    BIO_write(SSL_get_rbio(ssl), chunk, static_cast<int>(received.unwrap()));
                    continue;
                }
                if (received.is_ok())
                    return ca::core::Err(io::IoError::from_kind(
                        io::IoErrorKind::UnexpectedEof,
                        "write TLS stream reached an unclean TLS EOF"));
                return ca::core::Err(std::move(received).unwrap_err());
            }
            continue;
        }
        return ca::core::Err(ssl_io_error(ssl, result, "write TLS stream"));
    }
}

io::IoResult<void> TlsStream::flush()
{
    if (impl_ == nullptr)
        return ca::core::Ok();
    auto flushed = flush_write_bio(impl_->ssl, *output_);
    if (flushed.is_err())
        return flushed;
    return output_->flush();
}

io::IoResult<void> TlsStream::shutdown()
{
    if (impl_ == nullptr)
        return ca::core::Ok();
    auto* ssl = impl_->ssl;
    ERR_clear_error();
    const int result = SSL_shutdown(ssl);
    if (result < 0) {
        // 单向关闭不等待对端：WANT_READ/WANT_WRITE 表示 close_notify 已排队待发。
        const auto kind = SSL_get_error(ssl, result);
        if (kind != SSL_ERROR_WANT_READ && kind != SSL_ERROR_WANT_WRITE)
            return ca::core::Err(ssl_io_error(ssl, result, "shutdown TLS stream"));
    }
    return flush_write_bio(ssl, *output_);
}

}   // namespace ca::net

#else

namespace ca::net {

bool tls_stream_supported() noexcept
{
    return false;
}

TlsServerContext::TlsServerContext() noexcept = default;
TlsServerContext::~TlsServerContext()         = default;
TlsServerContext::TlsServerContext(TlsServerContext&&) noexcept             = default;
TlsServerContext& TlsServerContext::operator=(TlsServerContext&&) noexcept  = default;

struct TlsServerContext::Impl
{
};

struct TlsStream::Impl
{
};

io::IoResult<void> TlsServerContext::load(const TlsServerOptions&)
{
    return ca::core::Err(io::IoError::from_kind(
        io::IoErrorKind::Unsupported,
        "TLS requires a build with OpenSSL enabled (xmake f --with_openssl=y)"));
}

void* TlsServerContext::native_handle() const noexcept
{
    return nullptr;
}

TlsStream::TlsStream(TlsStream&&) noexcept            = default;
TlsStream& TlsStream::operator=(TlsStream&&) noexcept = default;
TlsStream::~TlsStream()                               = default;

io::IoResult<TlsStream> TlsStream::connect(io::Reader&, io::Writer&, const std::string&,
                                           const TlsClientOptions&, const TlsHandshakeControl&)
{
    return ca::core::Err(io::IoError::from_kind(
        io::IoErrorKind::Unsupported,
        "TLS requires a build with OpenSSL enabled (xmake f --with_openssl=y)"));
}

io::IoResult<TlsStream> TlsStream::accept(const TlsServerContext&, io::Reader&, io::Writer&,
                                          const TlsHandshakeControl&)
{
    return ca::core::Err(io::IoError::from_kind(
        io::IoErrorKind::Unsupported,
        "TLS requires a build with OpenSSL enabled (xmake f --with_openssl=y)"));
}

io::IoResult<usize> TlsStream::read(u8*, usize)
{
    return ca::core::Err(
        io::IoError::from_kind(io::IoErrorKind::Unsupported, "TLS stream is unavailable"));
}

io::IoResult<usize> TlsStream::write(const u8*, usize)
{
    return ca::core::Err(
        io::IoError::from_kind(io::IoErrorKind::Unsupported, "TLS stream is unavailable"));
}

io::IoResult<void> TlsStream::flush()
{
    return ca::core::Ok();
}

io::IoResult<void> TlsStream::shutdown()
{
    return ca::core::Ok();
}

bool TlsStream::is_open() const noexcept
{
    return false;
}

std::optional<std::string> TlsStream::alpn_selected() const
{
    return std::nullopt;
}

}   // namespace ca::net

#endif
