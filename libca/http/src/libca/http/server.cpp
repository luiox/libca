#include "libca/http/server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "libca/core/bytes.hpp"
#include "libca/http/detail/deadline_io.hpp"
#include "libca/thread/thread_pool.hpp"

namespace ca::http {
namespace {

struct Route
{
    std::string      method;
    std::string      path;
    HttpRouteHandler handler;
};

struct ConnectionJob
{
    ConnectionJob(net::TcpStream value, net::SocketAddress peer)
        : stream(std::move(value))
        , peer_address(std::move(peer))
    {}

    net::TcpStream     stream;
    net::SocketAddress peer_address;
};

HttpResult<void> validate_options(const HttpServerOptions& options)
{
    if (options.pending_connections == 0 || options.max_requests_per_connection == 0 ||
        options.idle_timeout.count() <= 0 || options.request_header_timeout.count() <= 0 ||
        options.request_body_timeout.count() <= 0 || options.response_write_timeout.count() <= 0 ||
        options.stop_poll_interval.count() <= 0)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState,
                                 "all HTTP server capacities and timeouts must be positive"));
    if (options.request_limits.max_start_line_bytes == 0 ||
        options.request_limits.max_header_bytes == 0 ||
        options.request_limits.max_header_count == 0 || options.request_limits.max_body_bytes == 0)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidState, "all HTTP request parsing limits must be positive"));
    return ca::core::Ok();
}

bool valid_route_path(std::string_view path) noexcept
{
    if (path.empty() || path.front() != '/')
        return false;
    return std::none_of(path.begin(), path.end(), [](char value) {
        const auto byte = static_cast<unsigned char>(value);
        return value == '?' || value == '#' || byte <= 0x20 || byte == 0x7f;
    });
}

std::string_view request_path(std::string_view target) noexcept
{
    if (target.empty() || target.front() != '/')
        return {};
    const auto query = target.find('?');
    return target.substr(0, query);
}

ca::core::Bytes bytes_from_string(std::string_view value)
{
    return ca::core::Bytes::copy_from_slice(reinterpret_cast<const u8*>(value.data()),
                                            value.size());
}

HttpResponse text_response(u16 status, std::string_view body)
{
    HttpResponse response;
    response.status = status;
    response.body   = bytes_from_string(body);
    response.headers.append("Content-Type", "text/plain; charset=utf-8");
    return response;
}

HttpResponse protocol_error_response(const HttpError& error)
{
    if (error.kind() == HttpErrorKind::HeaderLimitExceeded)
        return text_response(431, "Request Header Fields Too Large\n");
    if (error.kind() == HttpErrorKind::BodyLimitExceeded)
        return text_response(413, "Content Too Large\n");
    if (error.kind() == HttpErrorKind::ExpectationFailed)
        return text_response(417, "Expectation Failed\n");
    if (error.kind() == HttpErrorKind::Unsupported)
        return text_response(501, "Not Implemented\n");
    if (error.kind() == HttpErrorKind::Io && error.io_error() != nullptr &&
        (error.io_error()->kind() == io::IoErrorKind::TimedOut ||
         error.io_error()->kind() == io::IoErrorKind::WouldBlock))
        return text_response(408, "Request Timeout\n");
    return text_response(400, "Bad Request\n");
}

enum class Expectation
{
    None,
    Continue,
    Unsupported
};

Expectation request_expectation(const HttpHeaders& headers) noexcept
{
    const auto values = headers.get_all("Expect");
    if (values.empty())
        return Expectation::None;
    if (values.size() != 1)
        return Expectation::Unsupported;
    auto value = values.front();
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    if (value.size() != 12)
        return Expectation::Unsupported;
    constexpr std::string_view expected = "100-continue";
    for (usize index = 0; index < value.size(); ++index) {
        auto byte = static_cast<unsigned char>(value[index]);
        if (byte >= 'A' && byte <= 'Z')
            byte = static_cast<unsigned char>(byte + ('a' - 'A'));
        if (byte != static_cast<unsigned char>(expected[index]))
            return Expectation::Unsupported;
    }
    return Expectation::Continue;
}

bool error_allows_response(const HttpError& error) noexcept
{
    if (error.kind() != HttpErrorKind::Io)
        return true;
    return error.io_error() != nullptr && (error.io_error()->kind() == io::IoErrorKind::TimedOut ||
                                           error.io_error()->kind() == io::IoErrorKind::WouldBlock);
}

}   // namespace

class HttpServerImpl : public std::enable_shared_from_this<HttpServerImpl>
{
public:
    HttpServerImpl(net::TcpListener listener, net::SocketAddress local_address,
                   HttpServerOptions options)
        : listener_(std::move(listener))
        , local_address_(std::move(local_address))
        , options_(std::move(options))
    {}

    HttpResult<void> route(std::string method, std::string path, HttpRouteHandler handler)
    {
        if (!HttpHeaders::valid_name(method) || !valid_route_path(path) || !handler)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage, "HTTP route method, path, or handler is invalid"));
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (serve_started_)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState, "HTTP routes cannot change after serve starts"));
        const auto duplicate =
            std::find_if(routes_.begin(), routes_.end(), [&](const Route& route) {
                return route.method == method && route.path == path;
            });
        if (duplicate != routes_.end())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState, "HTTP route method and path are already registered"));
        routes_.push_back(Route{std::move(method), std::move(path), std::move(handler)});
        return ca::core::Ok();
    }

    HttpResult<void> set_fallback(HttpRouteHandler handler)
    {
        if (!handler)
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "HTTP fallback handler is empty"));
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (serve_started_)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState, "HTTP fallback cannot change after serve starts"));
        fallback_ = std::move(handler);
        return ca::core::Ok();
    }

    HttpResult<void> serve()
    {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (serve_started_)
                return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                          "HTTP server can only serve once"));
            serve_started_ = true;
            running_.store(true, std::memory_order_release);
        }

        ca::thread::ThreadPoolOptions pool_options;
        pool_options.thread_count   = options_.worker_threads;
        pool_options.queue_capacity = options_.pending_connections;
        auto created                = ca::thread::ThreadPool::create(pool_options);
        if (created.is_err()) {
            running_.store(false, std::memory_order_release);
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState,
                std::string("create HTTP worker pool: ") + created.unwrap_err().to_string()));
        }
        auto pool = std::move(created).unwrap();

        std::optional<HttpError> serve_error;
        while (!stop_source_.stop_requested()) {
            auto accepted = listener_.accept();
            if (accepted.is_err()) {
                auto error = accepted.unwrap_err();
                if (error.kind() == io::IoErrorKind::WouldBlock) {
                    stop_source_.token().wait_for(options_.stop_poll_interval);
                    continue;
                }
                if (error.kind() == io::IoErrorKind::Interrupted)
                    continue;
                serve_error = HttpError::from_io(std::move(error), "accept HTTP connection");
                break;
            }
            auto value    = std::move(accepted).unwrap();
            auto prepared = prepare_connection(std::move(value));
            if (prepared.is_err())
                continue;
            auto job = std::move(prepared).unwrap();
            if (stop_source_.stop_requested()) {
                job->stream.shutdown(net::Shutdown::Both);
                break;
            }

            auto self      = shared_from_this();
            auto submitted = pool.try_submit([self, job] { self->handle_connection(*job); });
            if (submitted.is_err()) {
                serve_error = HttpError::from_kind(
                    HttpErrorKind::InvalidState,
                    std::string("submit HTTP connection: ") + submitted.unwrap_err().to_string());
                break;
            }
            auto queued = std::move(submitted).unwrap();
            if (!queued.has_value())
                send_overloaded(job->stream);
        }

        stop();
        listener_.close();
        auto shutdown = pool.shutdown(ca::thread::ShutdownMode::CancelPending);
        auto joined   = pool.join();
        running_.store(false, std::memory_order_release);
        if (serve_error.has_value())
            return ca::core::Err(std::move(*serve_error));
        if (shutdown.is_err())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState,
                std::string("shutdown HTTP worker pool: ") + shutdown.to_string()));
        if (joined.is_err())
            return ca::core::Err(
                HttpError::from_kind(HttpErrorKind::InvalidState,
                                     std::string("join HTTP worker pool: ") + joined.to_string()));
        return ca::core::Ok();
    }

    void stop() noexcept { stop_source_.request_stop(); }

    const net::SocketAddress& local_address() const noexcept { return local_address_; }

    bool is_running() const noexcept { return running_.load(std::memory_order_acquire); }

    bool stop_requested() const noexcept { return stop_source_.stop_requested(); }

private:
    HttpResult<std::shared_ptr<ConnectionJob>> prepare_connection(net::TcpAcceptResult accepted)
    {
        auto blocking = accepted.stream.set_nonblocking(false);
        if (blocking.is_err())
            return ca::core::Err(
                HttpError::from_io(blocking.unwrap_err(), "configure accepted HTTP connection"));
        auto nodelay = accepted.stream.set_nodelay(options_.tcp_nodelay);
        if (nodelay.is_err())
            return ca::core::Err(
                HttpError::from_io(nodelay.unwrap_err(), "configure accepted HTTP connection"));
        return ca::core::Ok(std::make_shared<ConnectionJob>(std::move(accepted.stream),
                                                            std::move(accepted.peer_address)));
    }

    void send_overloaded(net::TcpStream& stream)
    {
        detail::DeadlineWriter deadline_writer(
            stream, stop_source_.token(), options_.stop_poll_interval);
        deadline_writer.start(options_.response_write_timeout);
        Http1Writer writer(deadline_writer);
        auto        response = text_response(503, "Service Unavailable\n");
        response.headers.set("Connection", "close");
        writer.write_response(response, "GET");
    }

    HttpResult<HttpRequest> read_request(Http1Reader&            reader,
                                         detail::DeadlineReader& deadline_reader,
                                         detail::DeadlineWriter& deadline_writer,
                                         Http1Writer&            writer)
    {
        deadline_reader.start_head(
            options_.idle_timeout, options_.request_header_timeout, reader.buffered_len() != 0);
        auto head_result = reader.read_request_head();
        if (head_result.is_err())
            return ca::core::Err(head_result.unwrap_err());
        auto head = std::move(head_result).unwrap();
        if (!head.has_value())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState, "HTTP peer closed the keep-alive connection"));

        const auto expectation = request_expectation(head->headers);
        if (expectation == Expectation::Unsupported)
            return ca::core::Err(
                HttpError::from_kind(HttpErrorKind::ExpectationFailed,
                                     "HTTP request contains an unsupported expectation"));
        if (expectation == Expectation::Continue && !reader.body_finished()) {
            HttpResponse interim;
            interim.version = head->version;
            interim.status  = 100;
            deadline_writer.start(options_.response_write_timeout);
            auto sent = writer.write_response(interim, head->method);
            if (sent.is_err())
                return ca::core::Err(sent.unwrap_err());
        }

        const HttpBodyInfo body_info = reader.body_info();
        const usize        initial_capacity =
            body_info.kind == HttpBodyKind::ContentLength
                       ? body_info.content_length
                       : std::min<usize>(options_.request_limits.max_body_bytes, 8192);
        auto                 output = ca::core::BytesMut::with_capacity(initial_capacity);
        std::array<u8, 8192> buffer{};
        deadline_reader.start(options_.request_body_timeout);
        while (!reader.body_finished()) {
            auto read = reader.read_body(buffer.data(), buffer.size());
            if (read.is_err())
                return ca::core::Err(read.unwrap_err());
            if (read.unwrap() != 0)
                output.put_slice(buffer.data(), read.unwrap());
        }
        auto trailers = reader.finish_body();
        if (trailers.is_err())
            return ca::core::Err(trailers.unwrap_err());

        HttpRequest request;
        request.method   = std::move(head->method);
        request.target   = std::move(head->target);
        request.version  = head->version;
        request.headers  = std::move(head->headers);
        request.body     = output.freeze();
        request.trailers = std::move(trailers).unwrap();
        return ca::core::Ok(std::move(request));
    }

    HttpResult<HttpServerResponse> dispatch(const HttpServerRequestContext& context)
    {
        const auto               path = request_path(context.request().target);
        std::vector<std::string> allowed;
        HttpRouteHandler         handler;
        HttpRouteHandler         fallback;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            for (const auto& route : routes_) {
                if (route.path != path)
                    continue;
                if (route.method == context.request().method) {
                    handler = route.handler;
                    break;
                }
                if (std::find(allowed.begin(), allowed.end(), route.method) == allowed.end())
                    allowed.push_back(route.method);
            }
            fallback = fallback_;
        }
        if (handler)
            return invoke_handler(handler, context);
        if (!allowed.empty()) {
            auto        response = text_response(405, "Method Not Allowed\n");
            std::string allow;
            for (usize index = 0; index < allowed.size(); ++index) {
                if (index != 0)
                    allow += ", ";
                allow += allowed[index];
            }
            response.headers.append("Allow", std::move(allow));
            return ca::core::Ok(HttpServerResponse::buffered(std::move(response)));
        }
        if (fallback)
            return invoke_handler(fallback, context);
        return ca::core::Ok(HttpServerResponse::buffered(text_response(404, "Not Found\n")));
    }

    HttpResult<HttpServerResponse> invoke_handler(const HttpRouteHandler&         handler,
                                                  const HttpServerRequestContext& context)
    {
        try {
            return handler(context);
        }
        catch (const std::exception& error) {
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState,
                std::string("HTTP route handler threw an exception: ") + error.what()));
        }
        catch (...) {
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidState, "HTTP route handler threw a non-standard exception"));
        }
    }

    bool send_response(HttpServerResponse response, const HttpRequest& request, Http1Writer& writer,
                       detail::DeadlineWriter& deadline_writer, bool request_keep_alive,
                       bool force_close)
    {
        if (!response.chunked_) {
            deadline_writer.start(options_.response_write_timeout);
            response.response_.version = request.version;
            prepare_connection_header(
                response.response_.headers, request.version, request_keep_alive && !force_close);
            const bool keep_alive = request_keep_alive && !force_close &&
                                    should_keep_alive(request.version, response.response_.headers);
            auto sent = writer.write_response(response.response_, request.method);
            return sent.is_ok() && keep_alive;
        }

        response.head_.version = request.version;
        deadline_writer.start_idle(options_.response_write_timeout);
        prepare_connection_header(
            response.head_.headers, request.version, request_keep_alive && !force_close);
        const bool keep_alive = request_keep_alive && !force_close &&
                                should_keep_alive(request.version, response.head_.headers);
        auto begun = writer.begin_chunked_response(response.head_, request.method);
        if (begun.is_err())
            return false;
        auto body = std::move(begun).unwrap();
        try {
            auto produced = response.producer_(body, stop_source_.token());
            if (produced.is_err())
                return false;
        }
        catch (...) {
            return false;
        }
        auto finished = body.finish(response.trailers_);
        return finished.is_ok() && keep_alive;
    }

    void prepare_connection_header(HttpHeaders& headers, HttpVersion version, bool may_keep_alive)
    {
        if (!may_keep_alive) {
            headers.set("Connection", "close");
            return;
        }
        if (version == HttpVersion::Http10 && !headers.contains("Connection"))
            headers.set("Connection", "keep-alive");
    }

    void send_protocol_error(const HttpError& error, Http1Writer& writer,
                             detail::DeadlineWriter& deadline_writer)
    {
        if (!error_allows_response(error))
            return;
        auto response = protocol_error_response(error);
        response.headers.set("Connection", "close");
        deadline_writer.start(options_.response_write_timeout);
        writer.write_response(response, "GET");
    }

    void handle_connection(ConnectionJob& job)
    {
        detail::DeadlineReader deadline_reader(
            job.stream, stop_source_.token(), options_.stop_poll_interval);
        detail::DeadlineWriter deadline_writer(
            job.stream, stop_source_.token(), options_.stop_poll_interval);
        Http1Reader reader(deadline_reader, options_.request_limits);
        Http1Writer writer(deadline_writer);

        for (usize count = 0; count < options_.max_requests_per_connection; ++count) {
            if (stop_source_.stop_requested())
                break;
            auto request_result = read_request(reader, deadline_reader, deadline_writer, writer);
            if (request_result.is_err()) {
                const auto& error = request_result.unwrap_err();
                if (error.kind() != HttpErrorKind::InvalidState)
                    send_protocol_error(error, writer, deadline_writer);
                break;
            }
            auto       request            = std::move(request_result).unwrap();
            const bool request_keep_alive = should_keep_alive(request.version, request.headers);
            HttpServerRequestContext context(request, job.peer_address, stop_source_.token());
            auto                     response = dispatch(context);
            if (response.is_err()) {
                auto failure    = text_response(500, "Internal Server Error\n");
                failure.version = request.version;
                failure.headers.set("Connection", "close");
                deadline_writer.start(options_.response_write_timeout);
                writer.write_response(failure, request.method);
                break;
            }
            const bool force_close =
                count + 1 >= options_.max_requests_per_connection || stop_source_.stop_requested();
            if (!send_response(std::move(response).unwrap(),
                               request,
                               writer,
                               deadline_writer,
                               request_keep_alive,
                               force_close))
                break;
        }
    }

    net::TcpListener       listener_;
    net::SocketAddress     local_address_;
    HttpServerOptions      options_;
    ca::thread::StopSource stop_source_;
    mutable std::mutex     state_mutex_;
    std::vector<Route>     routes_;
    HttpRouteHandler       fallback_;
    bool                   serve_started_{false};
    std::atomic<bool>      running_{false};
};

HttpServerRequestContext::HttpServerRequestContext(const HttpRequest&        request,
                                                   const net::SocketAddress& peer_address,
                                                   ca::thread::StopToken     stop_token) noexcept
    : request_(&request)
    , peer_address_(&peer_address)
    , stop_token_(std::move(stop_token))
{}

const HttpRequest& HttpServerRequestContext::request() const noexcept
{
    return *request_;
}

const net::SocketAddress& HttpServerRequestContext::peer_address() const noexcept
{
    return *peer_address_;
}

const ca::thread::StopToken& HttpServerRequestContext::stop_token() const noexcept
{
    return stop_token_;
}

HttpServerResponse::HttpServerResponse(HttpResponse response)
    : response_(std::move(response))
{}

HttpServerResponse::HttpServerResponse(HttpResponseHead head, HttpChunkedBodyProducer producer,
                                       HttpHeaders trailers)
    : chunked_(true)
    , head_(std::move(head))
    , producer_(std::move(producer))
    , trailers_(std::move(trailers))
{}

HttpServerResponse HttpServerResponse::buffered(HttpResponse response)
{
    return HttpServerResponse(std::move(response));
}

HttpServerResponse HttpServerResponse::chunked(HttpResponseHead        head,
                                               HttpChunkedBodyProducer producer,
                                               HttpHeaders             trailers)
{
    return HttpServerResponse(std::move(head), std::move(producer), std::move(trailers));
}

bool HttpServerResponse::is_chunked() const noexcept
{
    return chunked_;
}

HttpResult<HttpServer> HttpServer::bind(const net::SocketAddress& address,
                                        const HttpServerOptions&  options)
{
    auto valid = validate_options(options);
    if (valid.is_err())
        return ca::core::Err(valid.unwrap_err());
    auto bound = net::TcpListener::bind(address, options.listener);
    if (bound.is_err())
        return ca::core::Err(HttpError::from_io(bound.unwrap_err(), "bind HTTP server"));
    auto listener = std::move(bound).unwrap();
    auto blocking = listener.set_nonblocking(true);
    if (blocking.is_err())
        return ca::core::Err(HttpError::from_io(blocking.unwrap_err(), "configure HTTP listener"));
    auto local = listener.local_address();
    if (local.is_err())
        return ca::core::Err(HttpError::from_io(local.unwrap_err(), "query HTTP listener"));
    return ca::core::Ok(HttpServer(
        std::make_shared<HttpServerImpl>(std::move(listener), std::move(local).unwrap(), options)));
}

HttpServer::HttpServer(std::shared_ptr<HttpServerImpl> impl) noexcept
    : impl_(std::move(impl))
{}

HttpServer::HttpServer(HttpServer&& other) noexcept = default;

HttpServer& HttpServer::operator=(HttpServer&& other) noexcept
{
    if (this != &other) {
        stop();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

HttpServer::~HttpServer()
{
    stop();
}

HttpResult<void> HttpServer::route(std::string method, std::string path, HttpRouteHandler handler)
{
    if (impl_ == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP server has been moved from"));
    return impl_->route(std::move(method), std::move(path), std::move(handler));
}

HttpResult<void> HttpServer::set_fallback(HttpRouteHandler handler)
{
    if (impl_ == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP server has been moved from"));
    return impl_->set_fallback(std::move(handler));
}

HttpResult<void> HttpServer::serve()
{
    if (impl_ == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP server has been moved from"));
    return impl_->serve();
}

void HttpServer::stop() noexcept
{
    if (impl_ != nullptr)
        impl_->stop();
}

HttpResult<net::SocketAddress> HttpServer::local_address() const
{
    if (impl_ == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP server has been moved from"));
    return ca::core::Ok(impl_->local_address());
}

bool HttpServer::is_running() const noexcept
{
    return impl_ != nullptr && impl_->is_running();
}

bool HttpServer::stop_requested() const noexcept
{
    return impl_ == nullptr || impl_->stop_requested();
}

}   // namespace ca::http
