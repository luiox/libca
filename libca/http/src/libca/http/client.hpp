#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "libca/http/client_pool.hpp"
#include "libca/http/message.hpp"
#include "libca/http/url.hpp"
#include "libca/net/dns_cache.hpp"

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
    /// @brief 可选共享连接池；设置后请求完成时把仍可复用的 keep-alive 连接归还池中，
    /// 下次请求优先从池中借出（借出前做空闲超时与存活校验，失效即丢弃重建）。
    std::shared_ptr<HttpConnectionPool> pool;
    /// @brief 可选 DNS 缓存；设置后新连接经它解析主机名（TTL + LRU，减少重复解析），
    /// 不设置则行为不变（直接走系统解析）。多线程共享同一实例是安全的。
    std::shared_ptr<net::CachedDnsResolver> dns_cache;
};

/// @brief 流式响应：response head 已就绪，body 由调用方逐块消费。
/// @details 由 HttpClient::request_streaming / HttpChunkedRequest::finish 创建，
///          用于大文件下载、SSE 等场景——响应体不再整体驻留内存。move-only，
///          自持连接（独立于创建它的 client 生命周期）。
/// @note body 读取同样受 HttpClientOptions::limits::max_body_bytes 约束：Content-Length
///       响应在 head 阶段即超限报错，chunked / close-delimited 在累计消费超限时报错——
///       流式消费大 body 前应按需调高该上限。
/// @note 消费完成（finish）后，连接在配置了连接池时归还池中，否则直接关闭——
///       不再作为本 client 的 keep-alive 复用连接保留（流式场景连接复用价值低，
///       换取实现简单可靠）。
class HttpStreamingResponse
{
public:
    /// @brief 构造空对象（无连接，所有方法返回 InvalidState）。
    HttpStreamingResponse() noexcept;
    HttpStreamingResponse(HttpStreamingResponse&& other) noexcept;
    HttpStreamingResponse& operator=(HttpStreamingResponse&& other) noexcept;
    ~HttpStreamingResponse();

    /// @brief response status（head 已就绪）。
    u16 status() const noexcept;
    /// @brief response HTTP 版本。
    HttpVersion version() const noexcept;
    /// @brief response headers。
    const HttpHeaders& headers() const noexcept;
    /// @brief body 是否已读到消息边界（上次 read_body 返回 0）。
    bool body_finished() const noexcept;

    /// @brief 读取一块解码后的 body（chunked 已解码）。
    /// @return 写入 buffer 的字节数；0 表示 body 完成（此后可 finish）。
    /// @note 超时（response_body_timeout 窗口）与超限（max_body_bytes）返回 Err，
    ///       连接随即失效（本对象变为空）。
    HttpResult<usize> read_body(u8* buffer, usize capacity);

    /// @brief 完成响应消费：排空剩余 body、走到消息边界，归还连接（有池）或关闭。
    /// @note body 未读完时由本方法在有期限内排空；错误都会使连接失效。
    HttpResult<void> finish();

    /// @brief 提前放弃：直接关闭连接（不可复用）。幂等；调用后对象为空。
    void abort() noexcept;

private:
    class Impl;

    explicit HttpStreamingResponse(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;

    friend class HttpClient;
    friend class HttpChunkedRequest;
};

/// @brief chunked 请求流：请求头已发出，body 由调用方逐块写入（大文件上传等）。
/// @details 由 HttpClient::begin_chunked 创建，要求 HTTP/1.1；Transfer-Encoding:
///          chunked 无需调用方设置（缺失时自动补）。move-only，自持连接。
/// @note 流式路径不做 stale 连接重试（buffered request 有）；复用连接被服务器
///       回收时首次 write_chunk 返回底层错误。
class HttpChunkedRequest
{
public:
    /// @brief 构造空对象（无连接，所有方法返回 InvalidState）。
    HttpChunkedRequest() noexcept;
    HttpChunkedRequest(HttpChunkedRequest&& other) noexcept;
    HttpChunkedRequest& operator=(HttpChunkedRequest&& other) noexcept;
    ~HttpChunkedRequest();

    /// @brief 写入一个 chunk；空 data 为 no-op（不表示 body 结束）。
    /// @note 超时（request_write_timeout 窗口）返回 Err，连接失效。
    HttpResult<void> write_chunk(const u8* data, usize length);

    /// @brief flush 底层 writer，供 SSE 等低延迟上传提交事件。
    HttpResult<void> flush();

    /// @brief 写入 final chunk 与 trailers，读取响应 head，返回流式响应。
    HttpResult<HttpStreamingResponse> finish(const HttpHeaders& trailers = HttpHeaders());

    /// @brief 放弃请求：直接关闭连接。幂等；调用后对象为空。
    void abort() noexcept;

private:
    class Impl;

    explicit HttpChunkedRequest(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;

    friend class HttpClient;
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

    /// @brief 发送 request 并返回流式响应：head 就绪后即返回，body 由调用方
    ///        逐块消费（大文件 / SSE；对比 request() 的全量缓冲）。
    /// @details target 与 Host 同 request() 由 url 覆盖。流式路径不做 stale 连接
    ///          重试；max_body_bytes 语义见 HttpStreamingResponse。
    HttpResult<HttpStreamingResponse> request_streaming(const HttpUrl& url, HttpRequest request);

    /// @brief 开始一个 chunked 请求：request head（method/headers）即发出，body 由
    ///        调用方经 HttpChunkedRequest::write_chunk 逐块发送，finish 后取得响应。
    /// @param head 只需 method 与自定义 headers；target/Host 由 url 覆盖；版本须为
    ///        HTTP/1.1（chunked 要求）。不应携带 Content-Length / Transfer-Encoding
    ///        （后者缺失时自动补 chunked）。
    /// @note 请求期间该 client 的 keep-alive 连接被占用；流式路径不做 stale 重试。
    HttpResult<HttpChunkedRequest> begin_chunked(const HttpUrl& url, HttpRequestHead head);

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
