#include <gtest/gtest.h>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#endif

#if defined(LIBCA_HTTP_HAS_OPENSSL)

#    include <algorithm>
#    include <atomic>
#    include <chrono>
#    include <climits>
#    include <filesystem>
#    include <fstream>
#    include <future>
#    include <memory>
#    include <stdexcept>
#    include <string>
#    include <string_view>
#    include <thread>
#    include <utility>

#    include <openssl/err.h>
#    include <openssl/evp.h>
#    include <openssl/pem.h>
#    include <openssl/rsa.h>
#    include <openssl/ssl.h>
#    include <openssl/x509v3.h>

#    include "libca/http/http.hpp"

namespace ca::http::test {
namespace {

// ============================================================================
// 证书生成(与 tls_client_test.cpp 同款):用 OpenSSL API 现场生成 CA + leaf,
// 写到临时文件供 HttpServer 的 SSL_CTX_use_certificate_chain_file 加载。
// ============================================================================

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

std::filesystem::path write_to_temp_file(const std::string& suffix, const std::string& content)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto       path  = std::filesystem::temp_directory_path() /
                ("libca_http_server_test_" + suffix + "_" + std::to_string(stamp) + ".pem");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("open temporary file failed: " + path.string());
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file)
        throw std::runtime_error("write temporary file failed: " + path.string());
    return path;
}

std::string certificate_to_pem(X509* certificate)
{
    BioPtr output(BIO_new(BIO_s_mem()));
    require_openssl(output != nullptr, "create certificate memory BIO");
    require_openssl(PEM_write_bio_X509(output.get(), certificate) == 1, "encode certificate");
    char*      data = nullptr;
    const long size = BIO_get_mem_data(output.get(), &data);
    require_openssl(size > 0 && data != nullptr, "read encoded certificate");
    return std::string(data, static_cast<usize>(size));
}

std::string private_key_to_pem(EVP_PKEY* key)
{
    BioPtr output(BIO_new(BIO_s_mem()));
    require_openssl(output != nullptr, "create key memory BIO");
    require_openssl(PEM_write_bio_PrivateKey(output.get(), key, nullptr, nullptr, 0, nullptr,
                                              nullptr) == 1,
                    "encode private key");
    char*      data = nullptr;
    const long size = BIO_get_mem_data(output.get(), &data);
    require_openssl(size > 0 && data != nullptr, "read encoded private key");
    return std::string(data, static_cast<usize>(size));
}

/// @brief 测试用证书+私钥+CA 文件集合,析构时清理临时文件。
class TlsServerTestMaterials
{
public:
    explicit TlsServerTestMaterials(const char* subject_alt_name = "IP:127.0.0.1")
        : ca_key_(make_rsa_key())
        , ca_certificate_(make_certificate(ca_key_.get(), "libca server test CA", 1, nullptr,
                                           ca_key_.get(), nullptr, true))
        , server_key_(make_rsa_key())
        , server_certificate_(make_certificate(server_key_.get(), "localhost", 2,
                                               ca_certificate_.get(), ca_key_.get(),
                                               subject_alt_name, false))
        , cert_file_(write_to_temp_file("cert", certificate_to_pem(server_certificate_.get())))
        , key_file_(write_to_temp_file("key", private_key_to_pem(server_key_.get())))
        , ca_file_(write_to_temp_file("ca", certificate_to_pem(ca_certificate_.get())))
    {}

    TlsServerTestMaterials(const TlsServerTestMaterials&)            = delete;
    TlsServerTestMaterials& operator=(const TlsServerTestMaterials&) = delete;

    ~TlsServerTestMaterials()
    {
        std::error_code ignored;
        std::filesystem::remove(cert_file_, ignored);
        std::filesystem::remove(key_file_, ignored);
        std::filesystem::remove(ca_file_, ignored);
    }

    const std::filesystem::path& cert_file() const noexcept { return cert_file_; }
    const std::filesystem::path& key_file() const noexcept { return key_file_; }
    const std::filesystem::path& ca_file() const noexcept { return ca_file_; }

private:
    PkeyPtr               ca_key_;
    X509Ptr               ca_certificate_;
    PkeyPtr               server_key_;
    X509Ptr               server_certificate_;
    std::filesystem::path cert_file_;
    std::filesystem::path key_file_;
    std::filesystem::path ca_file_;
};

HttpTlsServerOptions server_tls_options(const TlsServerTestMaterials& materials)
{
    HttpTlsServerOptions options;
    options.certificate_chain_file = materials.cert_file().string();
    options.private_key_file       = materials.key_file().string();
    options.handshake_timeout      = std::chrono::milliseconds(2000);
    return options;
}

HttpClientOptions client_tls_options(const TlsServerTestMaterials& materials)
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

HttpUrl https_url(u16 port, const char* target)
{
    auto parsed = HttpUrl::parse("https://127.0.0.1:" + std::to_string(port) + target);
    if (parsed.is_err())
        throw std::runtime_error(parsed.unwrap_err().to_string());
    return std::move(parsed).unwrap();
}

std::string response_body(const HttpResponse& response)
{
    return std::string(reinterpret_cast<const char*>(response.body.as_ptr()),
                       response.body.remaining());
}

/// @brief 在独立线程里跑 HttpServer,析构时 stop + join。
class ScopedServer
{
public:
    explicit ScopedServer(HttpServer server)
        : server_(std::move(server))
        , completion_(promise_.get_future())
        , thread_([this] { promise_.set_value(server_.serve()); })
    {}

    ScopedServer(const ScopedServer&)            = delete;
    ScopedServer& operator=(const ScopedServer&) = delete;

    ~ScopedServer()
    {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    /// @brief 取回 serve() 的返回值(线程结束后)。
    HttpResult<void> serve_result()
    {
        return completion_.get();
    }

private:
    HttpServer                       server_;
    std::promise<HttpResult<void>>   promise_;
    std::future<HttpResult<void>>    completion_;
    std::thread                      thread_;
};

HttpResponse text_response(u16 status, std::string_view body)
{
    HttpResponse response;
    response.status = status;
    response.body   = ca::core::Bytes::copy_from_slice(
        reinterpret_cast<const u8*>(body.data()), body.size());
    response.headers.append("Content-Type", "text/plain; charset=utf-8");
    return response;
}

HttpServer make_server_with_route(const net::SocketAddress&  address,
                                  const HttpServerOptions&   options,
                                  std::string_view           path,
                                  u16                        status,
                                  std::string_view           body)
{
    auto bound = HttpServer::bind(address, options);
    if (bound.is_err())
        throw std::runtime_error(bound.unwrap_err().to_string());
    auto server = std::move(bound).unwrap();
    auto route  = server.route("GET", std::string(path),
                               [status, body](const HttpServerRequestContext&) {
                                   return ca::core::Ok(
                                       HttpServerResponse::buffered(text_response(status, body)));
                               });
    if (route.is_err())
        throw std::runtime_error(route.unwrap_err().to_string());
    return server;
}

// ============================================================================
// 测试用例
// ============================================================================

TEST(HttpTlsServerTest, ReportsOpensslAvailability)
{
    EXPECT_TRUE(HttpServer::supports_https());
}

TEST(HttpTlsServerTest, ServesHttpsRequestWithCustomCa)
{
    TlsServerTestMaterials materials;
    HttpServerOptions      options;
    options.tls              = server_tls_options(materials);
    options.worker_threads   = 1;
    options.idle_timeout     = std::chrono::milliseconds(1000);
    options.stop_poll_interval = std::chrono::milliseconds(5);
    const net::SocketAddress address(net::IpAddress::localhost_v4(), 0);
    auto server = make_server_with_route(address, options, "/hello", 200, "hello");
    auto bound_address = [&] {
        auto queried = server.local_address();
        if (queried.is_err())
            throw std::runtime_error(queried.unwrap_err().to_string());
        return queried.unwrap();
    }();
    ScopedServer scoped(std::move(server));

    auto client = HttpClient::create(client_tls_options(materials)).unwrap();
    auto response = client.get(https_url(bound_address.port(), "/hello"));
    ASSERT_TRUE(response.is_ok()) << response.unwrap_err().to_string();
    EXPECT_EQ(response.unwrap().status, 200);
    EXPECT_EQ(response_body(response.unwrap()), "hello");
}

TEST(HttpTlsServerTest, RejectsClientWithoutTrustedCa)
{
    TlsServerTestMaterials materials;
    HttpServerOptions      options;
    options.tls              = server_tls_options(materials);
    options.worker_threads   = 1;
    options.idle_timeout     = std::chrono::milliseconds(1000);
    options.stop_poll_interval = std::chrono::milliseconds(5);
    const net::SocketAddress address(net::IpAddress::localhost_v4(), 0);
    auto server = make_server_with_route(address, options, "/hello", 200, "hello");
    auto bound_address = [&] {
        auto queried = server.local_address();
        if (queried.is_err())
            throw std::runtime_error(queried.unwrap_err().to_string());
        return queried.unwrap();
    }();
    ScopedServer scoped(std::move(server));

    // 客户端不配置信任 CA(且默认 verify_peer=true),应拒绝服务端证书。
    HttpClientOptions client_options = client_tls_options(materials);
    client_options.tls.ca_file.clear();
    auto client = HttpClient::create(client_options).unwrap();
    auto response = client.get(https_url(bound_address.port(), "/hello"));
    EXPECT_TRUE(response.is_err());
}

TEST(HttpTlsServerTest, ServeFailsWhenCertificateFileMissing)
{
    HttpServerOptions options;
    options.tls                          = HttpTlsServerOptions{};
    options.tls->certificate_chain_file  = "/nonexistent/cert.pem";
    options.tls->private_key_file        = "/nonexistent/key.pem";
    const net::SocketAddress address(net::IpAddress::localhost_v4(), 0);
    auto bound = HttpServer::bind(address, options);
    ASSERT_TRUE(bound.is_ok()) << bound.unwrap_err().to_string();
    auto server = std::move(bound).unwrap();

    auto serve_result = server.serve();
    // serve() 在加载证书失败时直接返回错误,不进入 accept 循环。
    ASSERT_TRUE(serve_result.is_err());
}

TEST(HttpTlsServerTest, HandshakeTimeoutClosesConnection)
{
    TlsServerTestMaterials materials;
    HttpServerOptions      options;
    options.tls              = server_tls_options(materials);
    options.tls->handshake_timeout = std::chrono::milliseconds(50);   // 极短握手超时
    options.worker_threads   = 1;
    options.idle_timeout     = std::chrono::milliseconds(1000);
    options.stop_poll_interval = std::chrono::milliseconds(5);
    const net::SocketAddress address(net::IpAddress::localhost_v4(), 0);
    auto bound = HttpServer::bind(address, options);
    ASSERT_TRUE(bound.is_ok()) << bound.unwrap_err().to_string();
    auto server = std::move(bound).unwrap();
    auto bound_address = [&] {
        auto queried = server.local_address();
        if (queried.is_err())
            throw std::runtime_error(queried.unwrap_err().to_string());
        return queried.unwrap();
    }();
    ScopedServer scoped(std::move(server));

    // 建立 TCP 连接但不做 TLS 握手,触发服务端握手超时。
    auto connected =
        net::TcpStream::connect(net::SocketAddress(net::IpAddress::localhost_v4(),
                                                    bound_address.port()));
    ASSERT_TRUE(connected.is_ok());
    auto stream = std::move(connected).unwrap();
    // 不发送任何字节;等待超过 handshake_timeout 后,服务端应关闭连接。
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // 读应返回 EOF 或错误(连接已被服务端关闭)。
    u8 buffer[16]{};
    auto read = stream.read(buffer, sizeof(buffer));
    EXPECT_TRUE(read.is_err() || read.unwrap() == 0);
}

TEST(HttpTlsServerTest, SlowHandshakeDoesNotBlockLaterConnections)
{
    TlsServerTestMaterials materials;
    HttpServerOptions      options;
    options.tls                    = server_tls_options(materials);
    options.tls->handshake_timeout = std::chrono::milliseconds(1000);
    options.worker_threads         = 2;
    options.idle_timeout           = std::chrono::milliseconds(1000);
    options.stop_poll_interval     = std::chrono::milliseconds(5);
    const net::SocketAddress address(net::IpAddress::localhost_v4(), 0);
    auto         server        = make_server_with_route(address, options, "/hello", 200, "hello");
    auto         bound_address = server.local_address().unwrap();
    ScopedServer scoped(std::move(server));

    auto stalled = net::TcpStream::connect(
        net::SocketAddress(net::IpAddress::localhost_v4(), bound_address.port()));
    ASSERT_TRUE(stalled.is_ok());
    auto stalled_stream = std::move(stalled).unwrap();

    auto client_options                    = client_tls_options(materials);
    client_options.connect_timeout         = std::chrono::milliseconds(500);
    client_options.tls_handshake_timeout   = std::chrono::milliseconds(500);
    client_options.response_header_timeout = std::chrono::milliseconds(500);
    auto client                            = HttpClient::create(client_options).unwrap();
    auto response                          = client.get(https_url(bound_address.port(), "/hello"));
    ASSERT_TRUE(response.is_ok()) << response.unwrap_err().to_string();
    EXPECT_EQ(response.unwrap().status, 200);

    stalled_stream.shutdown(net::Shutdown::Both);
}

TEST(HttpTlsServerTest, RejectsNonPositiveHandshakeTimeout)
{
    HttpServerOptions options;
    options.tls                    = HttpTlsServerOptions{};
    options.tls->handshake_timeout = std::chrono::milliseconds(0);
    const net::SocketAddress address(net::IpAddress::localhost_v4(), 0);
    auto                     bound = HttpServer::bind(address, options);
    ASSERT_TRUE(bound.is_err());
    EXPECT_EQ(bound.unwrap_err().kind(), HttpErrorKind::InvalidState);
}

}   // namespace
}   // namespace ca::http::test

#else

// 未启用 OpenSSL 时,该测试文件为空(不参与编译)。
namespace ca::http::test {
TEST(HttpTlsServerTest, DisabledWhenOpensslNotEnabled)
{
    GTEST_SKIP() << "HTTPS server tests require a build with OpenSSL enabled";
}
}   // namespace ca::http::test

#endif
