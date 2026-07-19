#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "libca/http/message.hpp"
#include "libca/http/url.hpp"

namespace ca::http {

/// @brief 可选 OpenSSL HTTPS client 的证书校验配置。
struct HttpTlsClientOptions
{
    /// @brief 是否校验证书链及 URL hostname；生产环境应保持开启。
    bool verify_peer{true};

    /// @brief 可选 PEM CA bundle 文件；为空时使用 OpenSSL 默认 trust paths。
    std::string ca_file;

    /// @brief 可选 OpenSSL hashed CA directory；为空时使用 OpenSSL 默认 trust paths。
    std::string ca_directory;
};

/// @brief 同步 HTTP client 的连接与响应限制。
struct HttpClientOptions
{
    std::chrono::milliseconds connect_timeout{10000};           ///< TCP connect 总期限。
    std::chrono::milliseconds tls_handshake_timeout{10000};     ///< TLS handshake 总期限。
    std::chrono::milliseconds request_write_timeout{30000};     ///< request 写入总期限。
    std::chrono::milliseconds response_header_timeout{30000};   ///< response head 总期限。
    std::chrono::milliseconds response_body_timeout{60000};     ///< response body 总期限。
    HttpLimits                limits;                           ///< response 解析限制。
    HttpTlsClientOptions      tls;                              ///< HTTPS 证书校验配置。
    usize max_informational_responses{8};   ///< 单次 request 最多接受的 1xx response 数量。
    bool tcp_nodelay{true};                 ///< 是否为新连接启用 TCP_NODELAY。
};

/// @brief 同步、单调用线程使用并复用同源 keep-alive 连接的 HTTP/HTTPS client。
/// @details HTTPS 仅在构建时启用 with_openssl 后可用，TLS 类型不会进入公开接口。
class HttpClient
{
public:
    /// @brief 校验 options 并创建尚未连接的 client。
    static HttpResult<HttpClient> create(const HttpClientOptions& options = HttpClientOptions());

    /// @brief 返回当前构建是否包含可选 OpenSSL HTTPS transport。
    static bool supports_https() noexcept;

    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&& other) noexcept;
    HttpClient& operator=(HttpClient&& other) noexcept;
    ~HttpClient();

    /// @brief 发送 request 并完整缓冲 response。
    /// @details target 与 Host 总是由 url 覆盖，避免连接 origin 与 wire authority 分歧。
    HttpResult<HttpResponse> request(const HttpUrl& url, HttpRequest request);

    /// @brief 发送无 body 的 GET request。
    HttpResult<HttpResponse> get(const HttpUrl& url);

    /// @brief 关闭当前 keep-alive 连接；重复调用无副作用。
    void close() noexcept;

    /// @brief 判断当前是否保存一条可尝试复用的连接。
    bool has_open_connection() const noexcept;

private:
    class Impl;

    explicit HttpClient(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::http
