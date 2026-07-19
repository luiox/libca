#include <gtest/gtest.h>

#if defined(LIBCA_HTTP_HAS_OPENSSL)

#    include <algorithm>
#    include <atomic>
#    include <chrono>
#    include <climits>
#    include <filesystem>
#    include <fstream>
#    include <future>
#    include <limits>
#    include <memory>
#    include <stdexcept>
#    include <string>
#    include <string_view>
#    include <thread>
#    include <utility>

#    if defined(_WIN32)
#        define WIN32_LEAN_AND_MEAN
#        define NOMINMAX
#    endif

#    include <openssl/err.h>
#    include <openssl/evp.h>
#    include <openssl/pem.h>
#    include <openssl/rsa.h>
#    include <openssl/ssl.h>
#    include <openssl/x509v3.h>

#    include "libca/http/http.hpp"
#    include "libca/http/detail/client_transport.hpp"
#    include "libca/net/tcp.hpp"

namespace ca::http::test {
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

struct PkeyContextDeleter
{
    void operator()(EVP_PKEY_CTX* context) const noexcept
    {
        if (context != nullptr)
            EVP_PKEY_CTX_free(context);
    }
};

struct PkeyDeleter
{
    void operator()(EVP_PKEY* key) const noexcept
    {
        if (key != nullptr)
            EVP_PKEY_free(key);
    }
};

struct X509Deleter
{
    void operator()(X509* certificate) const noexcept
    {
        if (certificate != nullptr)
            X509_free(certificate);
    }
};

struct X509ExtensionDeleter
{
    void operator()(X509_EXTENSION* extension) const noexcept
    {
        if (extension != nullptr)
            X509_EXTENSION_free(extension);
    }
};

struct BioDeleter
{
    void operator()(BIO* bio) const noexcept
    {
        if (bio != nullptr)
            BIO_free(bio);
    }
};

using SslContextPtr    = std::unique_ptr<SSL_CTX, SslContextDeleter>;
using SslPtr           = std::unique_ptr<SSL, SslDeleter>;
using PkeyContextPtr   = std::unique_ptr<EVP_PKEY_CTX, PkeyContextDeleter>;
using PkeyPtr          = std::unique_ptr<EVP_PKEY, PkeyDeleter>;
using X509Ptr          = std::unique_ptr<X509, X509Deleter>;
using X509ExtensionPtr = std::unique_ptr<X509_EXTENSION, X509ExtensionDeleter>;
using BioPtr           = std::unique_ptr<BIO, BioDeleter>;

std::string openssl_error()
{
    const auto code = ERR_get_error();
    if (code == 0)
        return "unknown OpenSSL error";
    char buffer[256]{};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return std::string(buffer);
}

void require_openssl(bool condition, const char* operation)
{
    if (!condition)
        throw std::runtime_error(std::string(operation) + ": " + openssl_error());
}

PkeyPtr make_rsa_key()
{
    PkeyContextPtr context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    require_openssl(context != nullptr, "create RSA key context");
    require_openssl(EVP_PKEY_keygen_init(context.get()) > 0, "initialize RSA key generation");
    require_openssl(EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048) > 0,
                    "configure RSA key size");
    EVP_PKEY* generated = nullptr;
    require_openssl(EVP_PKEY_keygen(context.get(), &generated) > 0, "generate RSA key");
    return PkeyPtr(generated);
}

void add_extension(X509* certificate, X509* issuer, int nid, const char* value)
{
    X509V3_CTX context{};
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, issuer, certificate, nullptr, nullptr, 0);
    X509ExtensionPtr extension(
        X509V3_EXT_conf_nid(nullptr, &context, nid, const_cast<char*>(value)));
    require_openssl(extension != nullptr, "create X509 extension");
    require_openssl(X509_add_ext(certificate, extension.get(), -1) == 1, "add X509 extension");
}

X509Ptr make_certificate(EVP_PKEY* subject_key, const char* common_name, i64 serial,
                         X509* issuer_certificate, EVP_PKEY* issuer_key,
                         const char* subject_alt_name, bool is_ca)
{
    X509Ptr certificate(X509_new());
    require_openssl(certificate != nullptr, "create X509 certificate");
    require_openssl(X509_set_version(certificate.get(), 2) == 1, "set X509 version");
    require_openssl(ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), serial) == 1,
                    "set X509 serial");
    require_openssl(X509_gmtime_adj(X509_get_notBefore(certificate.get()), -60) != nullptr,
                    "set X509 notBefore");
    require_openssl(X509_gmtime_adj(X509_get_notAfter(certificate.get()), 3600) != nullptr,
                    "set X509 notAfter");
    require_openssl(X509_set_pubkey(certificate.get(), subject_key) == 1, "set X509 public key");

    auto* subject = X509_get_subject_name(certificate.get());
    require_openssl(X509_NAME_add_entry_by_txt(subject,
                                               "CN",
                                               MBSTRING_ASC,
                                               reinterpret_cast<const unsigned char*>(common_name),
                                               -1,
                                               -1,
                                               0) == 1,
                    "set X509 common name");
    X509* issuer = issuer_certificate == nullptr ? certificate.get() : issuer_certificate;
    require_openssl(X509_set_issuer_name(certificate.get(), X509_get_subject_name(issuer)) == 1,
                    "set X509 issuer");

    if (is_ca) {
        add_extension(certificate.get(), issuer, NID_basic_constraints, "critical,CA:TRUE");
        add_extension(certificate.get(), issuer, NID_key_usage, "critical,keyCertSign,cRLSign");
    }
    else {
        add_extension(certificate.get(), issuer, NID_basic_constraints, "critical,CA:FALSE");
        add_extension(
            certificate.get(), issuer, NID_key_usage, "critical,digitalSignature,keyEncipherment");
        add_extension(certificate.get(), issuer, NID_ext_key_usage, "serverAuth");
        add_extension(certificate.get(), issuer, NID_subject_alt_name, subject_alt_name);
    }
    require_openssl(X509_sign(certificate.get(), issuer_key, EVP_sha256()) > 0,
                    "sign X509 certificate");
    return certificate;
}

std::filesystem::path write_ca_file(X509* certificate)
{
    BioPtr output(BIO_new(BIO_s_mem()));
    require_openssl(output != nullptr, "create CA memory BIO");
    require_openssl(PEM_write_bio_X509(output.get(), certificate) == 1, "encode CA certificate");

    char*      data = nullptr;
    const long size = BIO_get_mem_data(output.get(), &data);
    require_openssl(size > 0 && data != nullptr, "read encoded CA certificate");
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto       path  = std::filesystem::temp_directory_path() /
                ("libca_http_test_ca_" + std::to_string(stamp) + ".pem");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("open temporary CA file failed");
    file.write(data, size);
    if (!file)
        throw std::runtime_error("write temporary CA file failed");
    return path;
}

class TlsTestMaterials
{
public:
    explicit TlsTestMaterials(const char* subject_alt_name = "IP:127.0.0.1")
        : ca_key_(make_rsa_key())
        , ca_certificate_(make_certificate(ca_key_.get(), "libca test CA", 1, nullptr,
                                           ca_key_.get(), nullptr, true))
        , server_key_(make_rsa_key())
        , server_certificate_(make_certificate(server_key_.get(), "localhost", 2,
                                               ca_certificate_.get(), ca_key_.get(),
                                               subject_alt_name, false))
        , ca_file_(write_ca_file(ca_certificate_.get()))
    {}

    TlsTestMaterials(const TlsTestMaterials&)            = delete;
    TlsTestMaterials& operator=(const TlsTestMaterials&) = delete;

    ~TlsTestMaterials()
    {
        std::error_code ignored;
        std::filesystem::remove(ca_file_, ignored);
    }

    EVP_PKEY* server_key() const noexcept { return server_key_.get(); }
    X509*     server_certificate() const noexcept { return server_certificate_.get(); }
    const std::filesystem::path& ca_file() const noexcept { return ca_file_; }

private:
    PkeyPtr               ca_key_;
    X509Ptr               ca_certificate_;
    PkeyPtr               server_key_;
    X509Ptr               server_certificate_;
    std::filesystem::path ca_file_;
};

SslContextPtr make_server_context(const TlsTestMaterials& materials)
{
    SslContextPtr context(SSL_CTX_new(TLS_server_method()));
    require_openssl(context != nullptr, "create TLS server context");
    require_openssl(SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) == 1,
                    "set TLS server minimum version");
    require_openssl(SSL_CTX_use_certificate(context.get(), materials.server_certificate()) == 1,
                    "configure TLS server certificate");
    require_openssl(SSL_CTX_use_PrivateKey(context.get(), materials.server_key()) == 1,
                    "configure TLS server key");
    require_openssl(SSL_CTX_check_private_key(context.get()) == 1, "check TLS server key");
    return context;
}

net::TcpListener bind_tls_listener()
{
    auto listener = net::TcpListener::bind(net::SocketAddress(net::IpAddress::localhost_v4(), 0));
    if (listener.is_err())
        throw std::runtime_error(listener.unwrap_err().to_string());
    return std::move(listener).unwrap();
}

net::SocketAddress listener_address(net::TcpListener& listener)
{
    auto address = listener.local_address();
    if (address.is_err())
        throw std::runtime_error(address.unwrap_err().to_string());
    return address.unwrap();
}

class TlsTestServer
{
public:
    enum class CloseMode
    {
        KeepAlive,
        CleanCloseDelimited,
        UncleanCloseDelimited
    };

    TlsTestServer(const TlsTestMaterials& materials, usize expected_requests,
                  CloseMode close_mode = CloseMode::KeepAlive)
        : context_(make_server_context(materials))
        , listener_(bind_tls_listener())
        , address_(listener_address(listener_))
        , expected_requests_(expected_requests)
        , close_mode_(close_mode)
        , completion_(promise_.get_future())
        , thread_([this] { promise_.set_value(serve()); })
    {}

    TlsTestServer(const TlsTestServer&)            = delete;
    TlsTestServer& operator=(const TlsTestServer&) = delete;

    ~TlsTestServer()
    {
        if (thread_.joinable())
            thread_.join();
    }

    u16   port() const noexcept { return address_.port(); }
    usize connection_count() const noexcept { return connection_count_.load(); }
    usize request_count() const noexcept { return request_count_.load(); }

    std::string finish()
    {
        if (thread_.joinable())
            thread_.join();
        return completion_.get();
    }

private:
    static bool read_request(SSL* ssl, std::string& buffered, std::string& error)
    {
        while (buffered.find("\r\n\r\n") == std::string::npos) {
            char      bytes[2048]{};
            const int count = SSL_read(ssl, bytes, sizeof(bytes));
            if (count <= 0) {
                error = "read TLS request failed: " + openssl_error();
                return false;
            }
            buffered.append(bytes, static_cast<usize>(count));
        }
        buffered.erase(0, buffered.find("\r\n\r\n") + 4);
        return true;
    }

    static bool write_response(SSL* ssl, CloseMode close_mode, std::string& error)
    {
        static constexpr std::string_view KEEP_ALIVE_RESPONSE =
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: keep-alive\r\n\r\nok";
        static constexpr std::string_view CLOSE_DELIMITED_RESPONSE =
            "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nclose-delimited";
        const auto response =
            close_mode == CloseMode::KeepAlive ? KEEP_ALIVE_RESPONSE : CLOSE_DELIMITED_RESPONSE;
        usize offset = 0;
        while (offset < response.size()) {
            const usize remaining = response.size() - offset;
            const int   count     = SSL_write(ssl,
                                        response.data() + offset,
                                        static_cast<int>(std::min<usize>(remaining, INT_MAX)));
            if (count <= 0) {
                error = "write TLS response failed: " + openssl_error();
                return false;
            }
            offset += static_cast<usize>(count);
        }
        return true;
    }

    std::string serve()
    {
        auto accepted = listener_.accept();
        if (accepted.is_err())
            return accepted.unwrap_err().to_string();
        ++connection_count_;
        auto stream = std::move(accepted).unwrap().stream;
        if (stream.native_socket() > static_cast<net::RawSocket>(std::numeric_limits<int>::max()))
            return "native test socket exceeds OpenSSL int range";

        SslPtr ssl(SSL_new(context_.get()));
        if (ssl == nullptr)
            return "create TLS server connection failed: " + openssl_error();
        if (SSL_set_fd(ssl.get(), static_cast<int>(stream.native_socket())) != 1)
            return "attach TLS server socket failed: " + openssl_error();
        if (SSL_accept(ssl.get()) != 1)
            return {};

        std::string buffered;
        for (usize index = 0; index < expected_requests_; ++index) {
            std::string error;
            if (!read_request(ssl.get(), buffered, error))
                return error;
            ++request_count_;
            if (!write_response(ssl.get(), close_mode_, error))
                return error;
        }
        if (close_mode_ != CloseMode::UncleanCloseDelimited)
            SSL_shutdown(ssl.get());
        return {};
    }

    SslContextPtr             context_;
    net::TcpListener          listener_;
    net::SocketAddress        address_;
    usize                     expected_requests_{0};
    CloseMode                 close_mode_{CloseMode::KeepAlive};
    std::promise<std::string> promise_;
    std::future<std::string>  completion_;
    std::thread               thread_;
    std::atomic<usize>        connection_count_{0};
    std::atomic<usize>        request_count_{0};
};

HttpClientOptions tls_options(const TlsTestMaterials& materials)
{
    HttpClientOptions options;
    options.connect_timeout         = std::chrono::milliseconds(2000);
    options.tls_handshake_timeout   = std::chrono::milliseconds(2000);
    options.request_write_timeout   = std::chrono::milliseconds(2000);
    options.response_header_timeout = std::chrono::milliseconds(2000);
    options.response_body_timeout   = std::chrono::milliseconds(2000);
    options.tls.ca_file             = materials.ca_file().string();
    return options;
}

HttpUrl tls_url(const char* host, u16 port, const char* target)
{
    auto parsed =
        HttpUrl::parse("https://" + std::string(host) + ":" + std::to_string(port) + target);
    if (parsed.is_err())
        throw std::runtime_error(parsed.unwrap_err().to_string());
    return std::move(parsed).unwrap();
}

std::string response_body(const HttpResponse& response)
{
    return std::string(reinterpret_cast<const char*>(response.body.as_ptr()),
                       response.body.remaining());
}

TEST(HttpTlsClientTest, VerifiesCustomCaAndReusesConnection)
{
    TlsTestMaterials materials;
    TlsTestServer    server(materials, 2);
    auto             created = HttpClient::create(tls_options(materials));
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto client = std::move(created).unwrap();

    auto first = client.get(tls_url("127.0.0.1", server.port(), "/first"));
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(first.unwrap().status, 200);
    EXPECT_EQ(response_body(first.unwrap()), "ok");

    auto second = client.get(tls_url("127.0.0.1", server.port(), "/second"));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(response_body(second.unwrap()), "ok");
    EXPECT_TRUE(client.has_open_connection());

    EXPECT_TRUE(server.finish().empty());
    EXPECT_EQ(server.connection_count(), 1U);
    EXPECT_EQ(server.request_count(), 2U);
}

TEST(HttpTlsClientTest, RejectsUntrustedCertificate)
{
    TlsTestMaterials  materials;
    TlsTestServer     server(materials, 1);
    HttpClientOptions options = tls_options(materials);
    options.tls.ca_file.clear();
    auto client = HttpClient::create(options).unwrap();

    auto response = client.get(tls_url("127.0.0.1", server.port(), "/untrusted"));
    EXPECT_TRUE(response.is_err());
    EXPECT_FALSE(client.has_open_connection());
    EXPECT_TRUE(server.finish().empty());
}

TEST(HttpTlsClientTest, RejectsWrongHostname)
{
    TlsTestMaterials materials("DNS:localhost");
    TlsTestServer    server(materials, 1);
    auto             client = HttpClient::create(tls_options(materials)).unwrap();

    auto response = client.get(tls_url("127.0.0.1", server.port(), "/wrong-host"));
    EXPECT_TRUE(response.is_err());
    EXPECT_FALSE(client.has_open_connection());
    EXPECT_TRUE(server.finish().empty());
}

TEST(HttpTlsClientTest, VerifiesDnsIdentityAtTransportBoundary)
{
    TlsTestMaterials materials("DNS:localhost");
    TlsTestServer    server(materials, 0);
    auto             connected =
        net::TcpStream::connect(net::SocketAddress(net::IpAddress::localhost_v4(), server.port()));
    ASSERT_TRUE(connected.is_ok()) << connected.unwrap_err().to_string();

    const auto options = tls_options(materials);
    auto       secured = detail::make_tls_client_transport(
        std::move(connected).unwrap(), "localhost", options.tls, options.tls_handshake_timeout);
    ASSERT_TRUE(secured.is_ok()) << secured.unwrap_err().to_string();
    EXPECT_TRUE(server.finish().empty());
}

TEST(HttpTlsClientTest, AllowsExplicitlyDisabledPeerVerification)
{
    TlsTestMaterials  materials;
    TlsTestServer     server(materials, 1);
    HttpClientOptions options = tls_options(materials);
    options.tls.verify_peer   = false;
    options.tls.ca_file.clear();
    auto client = HttpClient::create(options).unwrap();

    auto response = client.get(tls_url("127.0.0.1", server.port(), "/insecure"));
    ASSERT_TRUE(response.is_ok()) << response.unwrap_err().to_string();
    EXPECT_EQ(response_body(response.unwrap()), "ok");
    EXPECT_TRUE(server.finish().empty());
}

TEST(HttpTlsClientTest, AcceptsCloseNotifyAsCloseDelimitedEof)
{
    TlsTestMaterials materials;
    TlsTestServer    server(materials, 1, TlsTestServer::CloseMode::CleanCloseDelimited);
    auto             client = HttpClient::create(tls_options(materials)).unwrap();

    auto response = client.get(tls_url("127.0.0.1", server.port(), "/clean-eof"));
    ASSERT_TRUE(response.is_ok()) << response.unwrap_err().to_string();
    EXPECT_EQ(response_body(response.unwrap()), "close-delimited");
    EXPECT_FALSE(client.has_open_connection());
    EXPECT_TRUE(server.finish().empty());
}

TEST(HttpTlsClientTest, RejectsUncleanTlsEof)
{
    TlsTestMaterials materials;
    TlsTestServer    server(materials, 1, TlsTestServer::CloseMode::UncleanCloseDelimited);
    auto             client = HttpClient::create(tls_options(materials)).unwrap();

    auto response = client.get(tls_url("127.0.0.1", server.port(), "/unclean-eof"));
    ASSERT_TRUE(response.is_err());
    ASSERT_NE(response.unwrap_err().io_error(), nullptr);
    EXPECT_EQ(response.unwrap_err().io_error()->kind(), io::IoErrorKind::UnexpectedEof);
    EXPECT_FALSE(client.has_open_connection());
    EXPECT_TRUE(server.finish().empty());
}

TEST(HttpTlsClientTest, EnforcesHandshakeDeadline)
{
    auto        listener = bind_tls_listener();
    const auto  address  = listener_address(listener);
    std::thread stalled_peer([listener = std::move(listener)]() mutable {
        auto accepted = listener.accept();
        if (accepted.is_ok())
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
    });

    HttpClientOptions options;
    options.tls.verify_peer       = false;
    options.tls_handshake_timeout = std::chrono::milliseconds(50);
    auto client                   = HttpClient::create(options).unwrap();
    auto response                 = client.get(tls_url("127.0.0.1", address.port(), "/timeout"));
    stalled_peer.join();

    ASSERT_TRUE(response.is_err());
    ASSERT_NE(response.unwrap_err().io_error(), nullptr);
    EXPECT_EQ(response.unwrap_err().io_error()->kind(), io::IoErrorKind::TimedOut);
    EXPECT_FALSE(client.has_open_connection());
}

}   // namespace
}   // namespace ca::http::test

#endif
