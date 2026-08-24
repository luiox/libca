#include "libca/http/client.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "libca/core/bytes.hpp"
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
    return kind == io::IoErrorKind::ConnectionReset ||
           kind == io::IoErrorKind::ConnectionAborted;
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

}   // namespace

class HttpClient::Impl
{
public:
    struct Connection
    {
        Connection(std::unique_ptr<detail::ClientTransport> value, HttpScheme origin_scheme,
                   std::string origin_host, u16 origin_port, const HttpLimits& limits)
            : transport(std::move(value))
            , deadline_reader(*transport, transport->tcp_stream(),
                              origin_scheme == HttpScheme::Https)
            , deadline_writer(*transport, transport->tcp_stream(),
                              origin_scheme == HttpScheme::Https)
            , codec_reader(deadline_reader, limits)
            , codec_writer(deadline_writer)
            , scheme(origin_scheme)
            , host(std::move(origin_host))
            , port(origin_port)
        {}

        std::unique_ptr<detail::ClientTransport> transport;
        detail::DeadlineReader                   deadline_reader;
        detail::DeadlineWriter                   deadline_writer;
        Http1Reader                              codec_reader;
        Http1Writer                              codec_writer;
        HttpScheme                               scheme{HttpScheme::Http};
        std::string                              host;
        u16                                      port{0};
    };

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
        auto connected =
            net::TcpStream::connect_timeout(url.host(), url.port(), options.connect_timeout);
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
        for (usize attempt = 0;; ++attempt) {
            stale_close         = false;
            informational_count = 0;
            connection->deadline_writer.start(options.request_write_timeout);
            auto written = connection->codec_writer.write_request(request);
            if (written.is_err()) {
                // 服务器已整体关闭复用连接时，写入可能直接以 reset 类错误失败，
                // 与读到 EOF 同样是 stale 连接而非请求本身的问题。
                if (!is_reset_error(written.unwrap_err()))
                    return fail(std::move(written).unwrap_err());
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
        if (stale_close)
            return fail(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                             "HTTP connection closed before response head"));

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
        if (!reusable)
            connection.reset();
        return ca::core::Ok(std::move(response));
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

}   // namespace ca::http
