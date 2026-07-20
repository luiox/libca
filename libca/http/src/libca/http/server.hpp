#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "libca/http/http1_codec.hpp"
#include "libca/net/tcp.hpp"
#include "libca/thread/stop_token.hpp"

namespace ca::http {

class HttpServerImpl;

/// @brief 单次 route handler 调用期间有效的 server request 上下文。
class HttpServerRequestContext
{
public:
    /// @brief 返回完整缓冲的 request。
    const HttpRequest& request() const noexcept;

    /// @brief 返回 TCP 对端地址。
    const net::SocketAddress& peer_address() const noexcept;

    /// @brief 返回 server stop 时收到请求的协作停止令牌。
    const ca::thread::StopToken& stop_token() const noexcept;

private:
    HttpServerRequestContext(const HttpRequest& request, const net::SocketAddress& peer_address,
                             ca::thread::StopToken stop_token) noexcept;

    const HttpRequest*        request_{nullptr};
    const net::SocketAddress* peer_address_{nullptr};
    ca::thread::StopToken     stop_token_;

    friend class HttpServerImpl;
};

/// @brief 生成未知长度 chunked response body 的同步回调。
/// @note 返回 Ok 后 server 显式写入 final chunk；返回错误时连接直接关闭，不伪装成完整响应。
/// @note 回调占用当前 connection worker；长连接应等待业务事件并协作检查 StopToken。
using HttpChunkedBodyProducer =
    std::function<HttpResult<void>(Http1ChunkedBodyWriter&, const ca::thread::StopToken&)>;

/// @brief route handler 返回的完整缓冲或 chunked streaming response。
class HttpServerResponse
{
public:
    /// @brief 创建完整缓冲 response。
    static HttpServerResponse buffered(HttpResponse response);

    /// @brief 创建由 producer 逐块生成 body 的 HTTP/1.1 chunked response。
    static HttpServerResponse chunked(HttpResponseHead head, HttpChunkedBodyProducer producer,
                                      HttpHeaders trailers = HttpHeaders());

    /// @brief 判断当前 response 是否使用 chunk producer。
    bool is_chunked() const noexcept;

private:
    explicit HttpServerResponse(HttpResponse response);
    HttpServerResponse(HttpResponseHead head, HttpChunkedBodyProducer producer,
                       HttpHeaders trailers);

    bool                    chunked_{false};
    HttpResponse            response_;
    HttpResponseHead        head_;
    HttpChunkedBodyProducer producer_;
    HttpHeaders             trailers_;

    friend class HttpServerImpl;
};

/// @brief 处理一条完整 request 并返回 response 的 route 回调。
/// @note 不同连接上的回调可并发执行；共享业务状态由调用方同步。
using HttpRouteHandler =
    std::function<HttpResult<HttpServerResponse>(const HttpServerRequestContext&)>;

/// @brief 在 route 解析前检查一条完整 request 的 middleware。
/// @details 返回空 optional 时继续执行后续 middleware 与 route；返回 response 时立即短路。
/// @note 不同连接上的 middleware 可并发执行；共享业务状态由调用方同步。
using HttpRequestMiddleware =
    std::function<HttpResult<std::optional<HttpServerResponse>>(const HttpServerRequestContext&)>;

/// @brief 可选 OpenSSL HTTPS server 的证书与客户端校验配置。
struct HttpTlsServerOptions
{
    /// @brief PEM 证书链文件路径(leaf 证书在前,后跟 intermediate)。
    std::string certificate_chain_file;

    /// @brief PEM 私钥文件路径;私钥可与证书链同文件,也可独立。
    std::string private_key_file;

    /// @brief 可选 PEM CA bundle 文件,用于校验客户端证书;与 ca_directory 同时为空时,
    /// verify_client=true 会回退到 OpenSSL 默认 trust paths。
    std::string ca_file;

    /// @brief 可选 OpenSSL hashed CA directory,用于校验客户端证书。
    std::string ca_directory;

    /// @brief 是否要求并校验客户端证书(mTLS)。
    /// @note 为 true 时,无客户端证书的连接会在 TLS 握手阶段被拒绝。
    bool verify_client{false};

    /// @brief TLS 握手总期限;超时直接关闭连接,不进入 HTTP 处理。
    std::chrono::milliseconds handshake_timeout{10000};
};

/// @brief 同步 HTTP server 的 listener、期限和有界并发配置。
struct HttpServerOptions
{
    net::TcpListenerOptions listener;         ///< bind/listen 选项。
    usize worker_threads{0};                  ///< worker 数量；0 使用 hardware_concurrency。
    usize pending_connections{64};            ///< 等待 worker 的连接队列容量。
    usize max_requests_per_connection{100};   ///< 单连接最多处理的 request 数。
    std::chrono::milliseconds idle_timeout{30000};   ///< 等待下一条 request 首字节的期限。
    std::chrono::milliseconds request_header_timeout{10000};   ///< 首字节后的 head 总期限。
    std::chrono::milliseconds request_body_timeout{30000};     ///< request body 总期限。
    std::chrono::milliseconds response_write_timeout{30000};   ///< 每条 response 写入总期限。
    std::chrono::milliseconds overload_response_timeout{1000};   ///< 过载响应与关闭的总期限。
    std::chrono::milliseconds stop_poll_interval{10};   ///< accept 与阻塞 IO 的 stop 检查间隔。
    HttpLimits request_limits;                          ///< request 解析限制。
    bool       tcp_nodelay{true};   ///< 是否为 accepted stream 启用 TCP_NODELAY。
    /// @brief 启用 HTTPS server 的配置;空 optional 时为纯 HTTP(默认)。
    /// @note 仅在构建启用 with_openssl 时实际生效;未启用时配置了也会握手失败。
    std::optional<HttpTlsServerOptions> tls;
};

/// @brief 精确路由、有界并发且可协作停止的同步 HTTP/1 server。
class HttpServer
{
public:
    /// @brief bind listener 并创建尚未进入 accept loop 的 server。
    static HttpResult<HttpServer> bind(const net::SocketAddress& address,
                                       const HttpServerOptions&  options = HttpServerOptions());

    /// @brief 返回当前构建是否包含可选 OpenSSL HTTPS server transport。
    static bool supports_https() noexcept;

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&& other) noexcept;
    HttpServer& operator=(HttpServer&& other) noexcept;
    ~HttpServer();

    /// @brief 注册区分大小写的 method 与 origin-form path 精确路由。
    /// @note 只能在 serve() 开始前注册；重复 method/path 返回 InvalidState。
    HttpResult<void> route(std::string method, std::string path, HttpRouteHandler handler);

    /// @brief 追加一个按注册顺序执行的 pre-routing middleware。
    /// @note middleware 会处理包括最终返回 404/405 在内的全部完整 request，只能在 serve() 前追加。
    HttpResult<void> add_middleware(HttpRequestMiddleware middleware);

    /// @brief 设置 path 未匹配时的 fallback；未设置时返回 404。
    /// @note path 已匹配但 method 不匹配仍返回 405。
    HttpResult<void> set_fallback(HttpRouteHandler handler);

    /// @brief 在当前线程运行 accept loop，直到 stop() 或不可恢复错误。
    /// @note 一个 server 实例只能调用一次；退出前会停止并 join 全部 worker。
    HttpResult<void> serve();

    /// @brief 线程安全地请求停止；活动 IO 最多一个 stop_poll_interval 后退出。
    void stop() noexcept;

    /// @brief 返回 bind 后的实际本地地址；移动后的对象返回 InvalidState。
    HttpResult<net::SocketAddress> local_address() const;

    /// @brief 判断 accept loop 当前是否正在运行。
    bool is_running() const noexcept;

    /// @brief 判断是否已收到停止请求。
    bool stop_requested() const noexcept;

private:
    explicit HttpServer(std::shared_ptr<HttpServerImpl> impl) noexcept;

    std::shared_ptr<HttpServerImpl> impl_;
};

}   // namespace ca::http
