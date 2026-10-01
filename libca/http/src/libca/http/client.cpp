#include "libca/http/client.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "libca/core/bytes.hpp"
#include "libca/http/detail/client_connection.hpp"
#include "libca/http/detail/client_transport.hpp"
#include "libca/http/detail/deadline_io.hpp"
#include "libca/http/http1_codec.hpp"
#include "libca/net/tcp.hpp"

namespace ca::http {
namespace {

bool ascii_equals(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;
    for (usize index = 0; index < lhs.size(); ++index) {
        auto left  = static_cast<unsigned char>(lhs[index]);
        auto right = static_cast<unsigned char>(rhs[index]);
        if (left >= 'A' && left <= 'Z')
            left = static_cast<unsigned char>(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z')
            right = static_cast<unsigned char>(right + ('a' - 'A'));
        if (left != right)
            return false;
    }
    return true;
}

HttpResult<void> validate_options(const HttpClientOptions& options)
{
    if (options.connect_timeout.count() <= 0 || options.tls_handshake_timeout.count() <= 0 ||
        options.request_write_timeout.count() <= 0 ||
        options.response_header_timeout.count() <= 0 ||
        options.response_body_timeout.count() <= 0 || options.max_informational_responses == 0)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidState, "all HTTP client timeouts and limits must be positive"));
    if (options.limits.max_start_line_bytes == 0 || options.limits.max_header_bytes == 0 ||
        options.limits.max_header_count == 0 || options.limits.max_body_bytes == 0)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidState, "all HTTP response parsing limits must be positive"));
    if (options.tls.ca_file.find('\0') != std::string::npos ||
        options.tls.ca_directory.find('\0') != std::string::npos)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "TLS CA paths must not contain NUL bytes"));
    return ca::core::Ok();
}

bool is_informational(u16 status) noexcept
{
    return status >= 100 && status < 200;
}

bool is_idempotent_method(std::string_view method) noexcept
{
    return method == "GET" || method == "HEAD" || method == "PUT" ||
           method == "DELETE" || method == "OPTIONS" || method == "TRACE";
}

bool is_reset_error(const HttpError& error) noexcept
{
    if (error.kind() != HttpErrorKind::Io || error.io_error() == nullptr)
        return false;
    const auto kind = error.io_error()->kind();
    // 向已被服务器整体关闭的 stale 连接写入：Linux 表现为 EPIPE（BrokenPipe），
    // Windows 常见为 WSAECONNRESET（ConnectionReset）；两者都意味着连接已死，
    // 而非请求本身的问题。
    return kind == io::IoErrorKind::ConnectionReset ||
           kind == io::IoErrorKind::ConnectionAborted ||
           kind == io::IoErrorKind::BrokenPipe;
}

bool is_connect_tunnel(std::string_view method, u16 status) noexcept
{
    return method == "CONNECT" && status >= 200 && status < 300;
}

class PlainClientTransport final : public detail::ClientTransport
{
public:
    explicit PlainClientTransport(net::TcpStream stream) noexcept
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

// 流式路径共用的响应头读取：处理 1xx（跳过、上限、拒绝 101），返回最终 head。
HttpResult<HttpResponseHead> read_streaming_head(detail::ClientConnection& connection,
                                                 const HttpClientOptions& options,
                                                 const std::string& method)
{
    connection.deadline_reader.start(options.response_header_timeout);
    HttpResponseHead head;
    usize informational_count = 0;
    for (;;) {
        auto received = connection.codec_reader.read_response_head(method);
        if (received.is_err())
            return ca::core::Err(std::move(received).unwrap_err());
        auto optional_head = std::move(received).unwrap();
        if (!optional_head.has_value())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage, "HTTP connection closed before response head"));
        head = std::move(*optional_head);
        if (!is_informational(head.status))
            return ca::core::Ok(std::move(head));
        auto finished = connection.codec_reader.finish_body();
        if (finished.is_err())
            return ca::core::Err(std::move(finished).unwrap_err());
        ++informational_count;
        if (head.status == 101)
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                                      "HTTP protocol upgrades are not supported"));
        if (informational_count > options.max_informational_responses)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage,
                "HTTP response contains too many informational responses"));
    }
}

}   // namespace

// ---- 流式公开类型的实现（pImpl；依赖本 TU 可见的 detail::ClientConnection）----

class HttpStreamingResponse::Impl
{
public:
    std::unique_ptr<detail::ClientConnection> connection;
    HttpClientOptions                         options;
    HttpResponseHead                          head;
    bool                                      body_done = false;
    bool                                      released  = false;
};

class HttpChunkedRequest::Impl
{
public:
    // 成员析构顺序（逆序）：chunked_writer 先于 connection 析构——writer 持有
    // connection 内 codec_writer 的指针，不得在连接销毁后再触碰。
    std::unique_ptr<detail::ClientConnection> connection;
    HttpClientOptions                         options;
    std::string                               method;
    std::optional<Http1ChunkedBodyWriter>     chunked_writer;
    bool                                      finished = false;
};

class HttpClient::Impl
{
public:
    using Connection = detail::ClientConnection;

    explicit Impl(HttpClientOptions value)
        : options(std::move(value))
    {}

    bool same_origin(const HttpUrl& url) const noexcept
    {
        return connection != nullptr && connection->scheme == url.scheme() &&
               connection->port == url.port() && ascii_equals(connection->host, url.host());
    }

    HttpResult<void> connect(const HttpUrl& url)
    {
        if (url.scheme() == HttpScheme::Https && !detail::tls_client_available())
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                                      "https requires the optional TLS transport"));
        if (same_origin(url))
            return ca::core::Ok();

        connection.reset();
        // 可选连接池：checkout 内部已完成空闲超时与探活校验，失效连接直接丢弃。
        if (options.pool != nullptr) {
            auto pooled = detail::pool_checkout(*options.pool, url);
            if (pooled != nullptr) {
                connection = std::move(pooled);
                return ca::core::Ok();
            }
        }
        // 可选 DNS 缓存：仅新连接解析时生效；包装成 DnsResolveFn 借给 TCP 注入点。
        const net::DnsResolveFn* injected = nullptr;
        net::DnsResolveFn        cached_resolve;
        if (options.dns_cache != nullptr) {
            cached_resolve = [cache = options.dns_cache](const std::string& resolve_host, u16 resolve_port,
                                                         net::AddressFamily family,
                                                         net::SocketKind    kind) {
                return cache->resolve(resolve_host, resolve_port, family, kind);
            };
            injected = &cached_resolve;
        }
        auto connected = net::TcpStream::connect_timeout(url.host(), url.port(),
                                                         options.connect_timeout, injected);
        if (connected.is_err())
            return ca::core::Err(HttpError::from_io(connected.unwrap_err(), "connect HTTP origin"));
        auto stream  = std::move(connected).unwrap();
        auto nodelay = stream.set_nodelay(options.tcp_nodelay);
        if (nodelay.is_err())
            return ca::core::Err(
                HttpError::from_io(nodelay.unwrap_err(), "configure HTTP connection"));

        std::unique_ptr<detail::ClientTransport> transport;
        if (url.scheme() == HttpScheme::Https) {
            auto secured = detail::make_tls_client_transport(
                std::move(stream), url.host(), options.tls, options.tls_handshake_timeout);
            if (secured.is_err())
                return ca::core::Err(std::move(secured).unwrap_err());
            transport = std::move(secured).unwrap();
        }
        else {
            transport = std::make_unique<PlainClientTransport>(std::move(stream));
        }
        connection = std::make_unique<Connection>(
            std::move(transport), url.scheme(), url.host(), url.port(), options.limits);
        return ca::core::Ok();
    }

    HttpResult<HttpResponse> request(const HttpUrl& url, HttpRequest request)
    {
        const bool reused = same_origin(url);
        auto connected = connect(url);
        if (connected.is_err())
            return ca::core::Err(connected.unwrap_err());

        request.target = url.target();
        auto host      = request.headers.set("Host", url.authority());
        if (host.is_err())
            return fail(host.unwrap_err());

        HttpResponseHead head;
        usize            informational_count = 0;
        bool             stale_close         = false;
        // stale 判定若源于 reset 类错误（EPIPE/RST），保留首个原始错误：
        // 不可重试请求最终报错时把底层诊断带上，不再被笼统的
        // "closed before response head" 吞掉（issue #228）。
        std::optional<HttpError> stale_reset_error;
        for (usize attempt = 0;; ++attempt) {
            stale_close         = false;
            informational_count = 0;
            stale_reset_error.reset();
            connection->deadline_writer.start(options.request_write_timeout);
            auto written = connection->codec_writer.write_request(request);
            if (written.is_err()) {
                // 服务器已整体关闭复用连接时，写入可能直接以 reset 类错误失败，
                // 与读到 EOF 同样是 stale 连接而非请求本身的问题。
                if (!is_reset_error(written.unwrap_err()))
                    return fail(std::move(written).unwrap_err());
                stale_reset_error = std::move(written).unwrap_err();
                stale_close = true;
            }

            if (!stale_close) {
                connection->deadline_reader.start(options.response_header_timeout);
                for (;;) {
                    auto received = connection->codec_reader.read_response_head(request.method);
                    if (received.is_err()) {
                        // 服务器已整体关闭复用连接时，本次写入会触发 RST，
                        // 与读到 EOF 同样是 stale 连接而非协议错误。
                        if (!is_reset_error(received.unwrap_err()))
                            return fail(std::move(received).unwrap_err());
                        stale_reset_error = std::move(received).unwrap_err();
                        stale_close = true;
                        break;
                    }
                    auto optional_head = std::move(received).unwrap();
                    if (!optional_head.has_value()) {
                        stale_close = true;
                        break;
                    }
                    head = std::move(*optional_head);
                    if (!is_informational(head.status))
                        break;
                    auto finished = connection->codec_reader.finish_body();
                    if (finished.is_err())
                        return fail(finished.unwrap_err());
                    ++informational_count;
                    if (head.status == 101)
                        return fail(HttpError::from_kind(HttpErrorKind::Unsupported,
                                                         "HTTP protocol upgrades are not supported"));
                    if (informational_count > options.max_informational_responses)
                        return fail(HttpError::from_kind(
                            HttpErrorKind::InvalidMessage,
                            "HTTP response contains too many informational responses"));
                }
            }

            // 复用的 keep-alive 连接在响应任何字节前被服务器关闭（idle 超时回收是
            // 常态）：幂等方法按 RFC 9110 §9.2.2 可在新连接上安全重试一次。
            const bool retryable = stale_close && informational_count == 0 && reused &&
                                   attempt == 0 && is_idempotent_method(request.method);
            if (!retryable)
                break;
            connection.reset();
            auto reconnected = connect(url);
            if (reconnected.is_err())
                return ca::core::Err(reconnected.unwrap_err());
        }
        if (stale_close) {
            if (stale_reset_error.has_value()) {
                auto original = std::move(*stale_reset_error);
                return fail(HttpError::from_kind(
                    HttpErrorKind::InvalidMessage,
                    "HTTP connection closed before response head: " + original.message()));
            }
            return fail(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                             "HTTP connection closed before response head"));
        }

        const HttpBodyInfo   body_info          = connection->codec_reader.body_info();
        const usize          expected_body_size = body_info.kind == HttpBodyKind::ContentLength
                                                      ? body_info.content_length
                                                      : options.limits.max_body_bytes;
        const usize          initial_capacity   = std::min<usize>(expected_body_size, 8192);
        auto                 output = ca::core::BytesMut::with_capacity(initial_capacity);
        std::array<u8, 8192> buffer{};
        connection->deadline_reader.start(options.response_body_timeout);
        while (!connection->codec_reader.body_finished()) {
            auto read = connection->codec_reader.read_body(buffer.data(), buffer.size());
            if (read.is_err())
                return fail(read.unwrap_err());
            if (read.unwrap() != 0)
                output.put_slice(buffer.data(), read.unwrap());
        }
        auto trailers = connection->codec_reader.finish_body();
        if (trailers.is_err())
            return fail(trailers.unwrap_err());

        HttpResponse response;
        response.version  = head.version;
        response.status   = head.status;
        response.reason   = std::move(head.reason);
        response.headers  = std::move(head.headers);
        response.body     = output.freeze();
        response.trailers = std::move(trailers).unwrap();

        const bool reusable = body_info.kind != HttpBodyKind::CloseDelimited &&
                              !is_connect_tunnel(request.method, response.status) &&
                              should_keep_alive(request.version, request.headers) &&
                              should_keep_alive(response.version, response.headers);
        // 响应体消费完且 framing 允许 keep-alive 才算可复用：无池时保留为本 client 的
        // 复用连接，有池时归还池中；不可复用的连接直接丢弃。
        if (!reusable)
            connection.reset();
        else if (options.pool != nullptr)
            detail::pool_checkin(*options.pool, std::move(connection));
        return ca::core::Ok(std::move(response));
    }

    // 流式请求：head 就绪即返回，body 留给 HttpStreamingResponse 消费。
    // 不做 stale 重试（连接被流式对象接管前生命周期复杂化，收益低——文档化取舍）。
    HttpResult<HttpStreamingResponse> request_streaming(const HttpUrl& url, HttpRequest request)
    {
        auto connected = connect(url);
        if (connected.is_err())
            return ca::core::Err(connected.unwrap_err());
        request.target = url.target();
        auto host      = request.headers.set("Host", url.authority());
        if (host.is_err()) {
            connection.reset();
            return ca::core::Err(std::move(host).unwrap_err());
        }

        connection->deadline_writer.start(options.request_write_timeout);
        auto written = connection->codec_writer.write_request(request);
        if (written.is_err()) {
            connection.reset();
            return ca::core::Err(std::move(written).unwrap_err());
        }

        auto head = read_streaming_head(*connection, options, request.method);
        if (head.is_err()) {
            connection.reset();
            return ca::core::Err(std::move(head).unwrap_err());
        }

        auto response_impl          = std::make_unique<HttpStreamingResponse::Impl>();
        response_impl->connection   = std::move(connection);
        response_impl->options      = options;
        response_impl->head         = std::move(head).unwrap();
        return ca::core::Ok(HttpStreamingResponse(std::move(response_impl)));
    }

    // chunked 请求：发出 head 后交出 Http1ChunkedBodyWriter，body 由调用方写。
    HttpResult<HttpChunkedRequest> begin_chunked(const HttpUrl& url, HttpRequestHead head)
    {
        auto connected = connect(url);
        if (connected.is_err())
            return ca::core::Err(connected.unwrap_err());
        head.target = url.target();
        auto host   = head.headers.set("Host", url.authority());
        if (host.is_err()) {
            connection.reset();
            return ca::core::Err(std::move(host).unwrap_err());
        }

        connection->deadline_writer.start(options.request_write_timeout);
        auto chunked = connection->codec_writer.begin_chunked_request(head);
        if (chunked.is_err()) {
            connection.reset();
            return ca::core::Err(std::move(chunked).unwrap_err());
        }

        auto impl          = std::make_unique<HttpChunkedRequest::Impl>();
        impl->connection   = std::move(connection);
        impl->options      = options;
        impl->method       = head.method;
        impl->chunked_writer.emplace(std::move(chunked).unwrap());
        return ca::core::Ok(HttpChunkedRequest(std::move(impl)));
    }

    HttpResult<HttpResponse> fail(HttpError error)
    {
        connection.reset();
        return ca::core::Err(std::move(error));
    }

    HttpClientOptions           options;
    std::unique_ptr<Connection> connection;
};

HttpResult<HttpClient> HttpClient::create(const HttpClientOptions& options)
{
    auto valid = validate_options(options);
    if (valid.is_err())
        return ca::core::Err(valid.unwrap_err());
    return ca::core::Ok(HttpClient(std::make_unique<Impl>(options)));
}

bool HttpClient::supports_https() noexcept
{
    return detail::tls_client_available();
}

HttpClient::HttpClient(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

HttpClient::HttpClient(HttpClient&& other) noexcept = default;

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept = default;

HttpClient::~HttpClient() = default;

HttpResult<HttpResponse> HttpClient::request(const HttpUrl& url, HttpRequest request)
{
    if (impl_ == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP client has been moved from"));
    return impl_->request(url, std::move(request));
}

HttpResult<HttpResponse> HttpClient::get(const HttpUrl& url)
{
    HttpRequest request;
    return this->request(url, std::move(request));
}

void HttpClient::close() noexcept
{
    if (impl_ != nullptr)
        impl_->connection.reset();
}

bool HttpClient::has_open_connection() const noexcept
{
    return impl_ != nullptr && impl_->connection != nullptr;
}

// ==================== HttpStreamingResponse ====================

HttpStreamingResponse::HttpStreamingResponse() noexcept = default;

HttpStreamingResponse::HttpStreamingResponse(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

HttpStreamingResponse::HttpStreamingResponse(HttpStreamingResponse&& other) noexcept = default;

HttpStreamingResponse& HttpStreamingResponse::operator=(HttpStreamingResponse&& other) noexcept = default;

HttpStreamingResponse::~HttpStreamingResponse() = default;

u16 HttpStreamingResponse::status() const noexcept
{
    return impl_ != nullptr ? impl_->head.status : u16{0};
}

HttpVersion HttpStreamingResponse::version() const noexcept
{
    return impl_ != nullptr ? impl_->head.version : HttpVersion::Http11;
}

const HttpHeaders& HttpStreamingResponse::headers() const noexcept
{
    static const HttpHeaders empty{};
    return impl_ != nullptr ? impl_->head.headers : empty;
}

bool HttpStreamingResponse::body_finished() const noexcept
{
    return impl_ != nullptr && impl_->body_done;
}

HttpResult<usize> HttpStreamingResponse::read_body(u8* buffer, usize capacity)
{
    if (impl_ == nullptr || impl_->connection == nullptr || impl_->released)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP streaming response is empty"));
    if (impl_->body_done)
        return ca::core::Ok(usize{0});
    impl_->connection->deadline_reader.start(impl_->options.response_body_timeout);
    auto read = impl_->connection->codec_reader.read_body(buffer, capacity);
    if (read.is_err()) {
        impl_->connection.reset();
        return ca::core::Err(std::move(read).unwrap_err());
    }
    if (read.unwrap() == 0)
        impl_->body_done = true;
    return read;
}

HttpResult<void> HttpStreamingResponse::finish()
{
    if (impl_ == nullptr || impl_->connection == nullptr || impl_->released)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP streaming response is empty"));
    auto& connection = *impl_->connection;
    if (!impl_->body_done) {
        // 未读完由本方法在期限内排空（大文件场景调用方一般读到 0 再 finish，
        // 排空兜底小体积剩余）。
        std::array<u8, 8192> buffer{};
        connection.deadline_reader.start(impl_->options.response_body_timeout);
        while (!connection.codec_reader.body_finished()) {
            auto read = connection.codec_reader.read_body(buffer.data(), buffer.size());
            if (read.is_err()) {
                impl_->connection.reset();
                impl_->released = true;
                return ca::core::Err(std::move(read).unwrap_err());
            }
        }
        impl_->body_done = true;
    }
    auto trailers = connection.codec_reader.finish_body();
    if (trailers.is_err()) {
        impl_->connection.reset();
        impl_->released = true;
        return ca::core::Err(std::move(trailers).unwrap_err());
    }
    if (impl_->options.pool != nullptr)
        detail::pool_checkin(*impl_->options.pool, std::move(impl_->connection));
    else
        impl_->connection.reset();
    impl_->released = true;
    return ca::core::Ok();
}

void HttpStreamingResponse::abort() noexcept
{
    if (impl_ == nullptr)
        return;
    impl_->connection.reset();
    impl_->released = true;
}

// ==================== HttpChunkedRequest ====================

HttpChunkedRequest::HttpChunkedRequest() noexcept = default;

HttpChunkedRequest::HttpChunkedRequest(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

HttpChunkedRequest::HttpChunkedRequest(HttpChunkedRequest&& other) noexcept = default;

HttpChunkedRequest& HttpChunkedRequest::operator=(HttpChunkedRequest&& other) noexcept = default;

HttpChunkedRequest::~HttpChunkedRequest() = default;

HttpResult<void> HttpChunkedRequest::write_chunk(const u8* data, usize length)
{
    if (impl_ == nullptr || impl_->connection == nullptr || impl_->finished)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP chunked request is empty"));
    impl_->connection->deadline_writer.start(impl_->options.request_write_timeout);
    auto written = impl_->chunked_writer->write_chunk(data, length);
    if (written.is_err()) {
        // writer 持有连接内 codec 的指针：先销毁 writer 再销毁连接。
        impl_->chunked_writer.reset();
        impl_->connection.reset();
        return ca::core::Err(std::move(written).unwrap_err());
    }
    return ca::core::Ok();
}

HttpResult<void> HttpChunkedRequest::flush()
{
    if (impl_ == nullptr || impl_->connection == nullptr || impl_->finished)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP chunked request is empty"));
    impl_->connection->deadline_writer.start(impl_->options.request_write_timeout);
    auto flushed = impl_->chunked_writer->flush();
    if (flushed.is_err()) {
        impl_->chunked_writer.reset();
        impl_->connection.reset();
        return ca::core::Err(std::move(flushed).unwrap_err());
    }
    return ca::core::Ok();
}

HttpResult<HttpStreamingResponse> HttpChunkedRequest::finish(const HttpHeaders& trailers)
{
    if (impl_ == nullptr || impl_->connection == nullptr || impl_->finished)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP chunked request is empty"));
    impl_->connection->deadline_writer.start(impl_->options.request_write_timeout);
    auto done = impl_->chunked_writer->finish(trailers);
    if (done.is_err()) {
        impl_->chunked_writer.reset();
        impl_->connection.reset();
        impl_->finished = true;
        return ca::core::Err(std::move(done).unwrap_err());
    }
    auto head = read_streaming_head(*impl_->connection, impl_->options, impl_->method);
    if (head.is_err()) {
        impl_->chunked_writer.reset();
        impl_->connection.reset();
        impl_->finished = true;
        return ca::core::Err(std::move(head).unwrap_err());
    }
    auto response_impl        = std::make_unique<HttpStreamingResponse::Impl>();
    response_impl->connection = std::move(impl_->connection);
    response_impl->options    = impl_->options;
    response_impl->head       = std::move(head).unwrap();
    impl_->chunked_writer.reset();
    impl_->finished = true;
    return ca::core::Ok(HttpStreamingResponse(std::move(response_impl)));
}

void HttpChunkedRequest::abort() noexcept
{
    if (impl_ == nullptr)
        return;
    impl_->chunked_writer.reset();
    impl_->connection.reset();
    impl_->finished = true;
}

// ==================== HttpClient 流式入口 ====================

HttpResult<HttpStreamingResponse> HttpClient::request_streaming(const HttpUrl& url,
                                                                HttpRequest    request)
{
    return impl_->request_streaming(url, std::move(request));
}

HttpResult<HttpChunkedRequest> HttpClient::begin_chunked(const HttpUrl&   url,
                                                         HttpRequestHead  head)
{
    return impl_->begin_chunked(url, std::move(head));
}

}   // namespace ca::http
