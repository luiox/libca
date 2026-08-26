#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "libca/http/http.hpp"
#include "libca/net/tcp.hpp"

namespace ca::http::test {
namespace {

class ServerRunner
{
public:
    explicit ServerRunner(HttpServer server)
        : server_(std::move(server))
        , completion_(promise_.get_future())
        , thread_([this] { promise_.set_value(server_.serve()); })
    {}

    ServerRunner(const ServerRunner&)            = delete;
    ServerRunner& operator=(const ServerRunner&) = delete;

    ~ServerRunner()
    {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    HttpResult<void> finish()
    {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
        return completion_.get();
    }

private:
    HttpServer                     server_;
    std::promise<HttpResult<void>> promise_;
    std::future<HttpResult<void>>  completion_;
    std::thread                    thread_;
};

std::string body_text(const ca::core::Bytes& body)
{
    return std::string(reinterpret_cast<const char*>(body.as_ptr()), body.remaining());
}

ca::core::Bytes body_bytes(std::string_view body)
{
    return ca::core::Bytes::copy_from_slice(reinterpret_cast<const u8*>(body.data()), body.size());
}

HttpResponse text_response(u16 status, std::string_view body)
{
    HttpResponse response;
    response.status = status;
    response.body   = body_bytes(body);
    EXPECT_TRUE(response.headers.append("Content-Type", "text/plain").is_ok());
    return response;
}

HttpUrl server_url(const net::SocketAddress& address, std::string_view target)
{
    auto parsed =
        HttpUrl::parse("http://127.0.0.1:" + std::to_string(address.port()) + std::string(target));
    EXPECT_TRUE(parsed.is_ok()) << (parsed.is_err() ? parsed.unwrap_err().to_string() : "");
    return std::move(parsed).unwrap();
}

HttpServer bind_server(HttpServerOptions options = HttpServerOptions())
{
    auto bound = HttpServer::bind(net::SocketAddress(net::IpAddress::localhost_v4(), 0), options);
    EXPECT_TRUE(bound.is_ok()) << (bound.is_err() ? bound.unwrap_err().to_string() : "");
    return std::move(bound).unwrap();
}

TEST(HttpClientServerTest, RoutesRequestsAndReusesSameOriginConnection)
{
    HttpServerOptions options;
    options.worker_threads = 2;
    auto server            = bind_server(options);
    auto address_result    = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    std::mutex       peers_mutex;
    std::vector<u16> peer_ports;
    auto             record_peer = [&](const HttpServerRequestContext& context) {
        std::lock_guard<std::mutex> lock(peers_mutex);
        peer_ports.push_back(context.peer_address().port());
    };

    ASSERT_TRUE(server
                    .route("GET",
                           "/hello",
                           [&](const HttpServerRequestContext& context) {
                               record_peer(context);
                               EXPECT_EQ(context.request().target, "/hello?q=1");
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "hello")));
                           })
                    .is_ok());
    ASSERT_TRUE(
        server
            .route("POST",
                   "/echo",
                   [&](const HttpServerRequestContext& context) {
                       record_peer(context);
                       HttpResponse response =
                           text_response(200, body_text(context.request().body));
                       EXPECT_TRUE(
                           response.headers
                               .set("X-Received-Host",
                                    std::string(context.request().headers.get("Host").value_or("")))
                               .is_ok());
                       return ca::core::Ok(HttpServerResponse::buffered(std::move(response)));
                   })
            .is_ok());
    ASSERT_EQ(server
                  .route("GET",
                         "/hello",
                         [](const HttpServerRequestContext&) {
                             return ca::core::Ok(
                                 HttpServerResponse::buffered(text_response(200, "duplicate")));
                         })
                  .unwrap_err()
                  .kind(),
              HttpErrorKind::InvalidState);

    ServerRunner runner(std::move(server));
    auto         created = HttpClient::create();
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto client = std::move(created).unwrap();

    auto hello = client.get(server_url(address, "/hello?q=1"));
    ASSERT_TRUE(hello.is_ok()) << hello.unwrap_err().to_string();
    EXPECT_EQ(hello.unwrap().status, 200);
    EXPECT_EQ(body_text(hello.unwrap().body), "hello");
    EXPECT_TRUE(client.has_open_connection());

    HttpRequest echo_request;
    echo_request.method = "POST";
    echo_request.body   = body_bytes("payload");
    ASSERT_TRUE(echo_request.headers.append("Host", "wrong.invalid").is_ok());
    auto echo = client.request(server_url(address, "/echo"), std::move(echo_request));
    ASSERT_TRUE(echo.is_ok()) << echo.unwrap_err().to_string();
    EXPECT_EQ(echo.unwrap().status, 200);
    EXPECT_EQ(body_text(echo.unwrap().body), "payload");
    EXPECT_EQ(echo.unwrap().headers.get("X-Received-Host"),
              "127.0.0.1:" + std::to_string(address.port()));

    auto method_not_allowed = client.get(server_url(address, "/echo"));
    ASSERT_TRUE(method_not_allowed.is_ok());
    EXPECT_EQ(method_not_allowed.unwrap().status, 405);
    EXPECT_EQ(method_not_allowed.unwrap().headers.get("Allow"), "POST");

    auto not_found = client.get(server_url(address, "/missing"));
    ASSERT_TRUE(not_found.is_ok());
    EXPECT_EQ(not_found.unwrap().status, 404);

    {
        std::lock_guard<std::mutex> lock(peers_mutex);
        ASSERT_EQ(peer_ports.size(), 2U);
        EXPECT_EQ(peer_ports[0], peer_ports[1]);
    }
    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, RunsMiddlewareBeforeRoutingAndCanShortCircuit)
{
    auto server         = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    std::mutex               order_mutex;
    std::vector<std::string> order;
    std::atomic<usize>       after_auth_calls{0};
    std::atomic<usize>       route_calls{0};
    ASSERT_TRUE(server
                    .add_middleware([&](const HttpServerRequestContext&) {
                        std::lock_guard<std::mutex> lock(order_mutex);
                        order.push_back("audit");
                        return ca::core::Ok(std::optional<HttpServerResponse>{});
                    })
                    .is_ok());
    ASSERT_TRUE(server
                    .add_middleware([&](const HttpServerRequestContext& context) {
                        {
                            std::lock_guard<std::mutex> lock(order_mutex);
                            order.push_back("auth");
                        }
                        if (context.request().headers.get("Authorization") != "Bearer secret")
                            return ca::core::Ok(std::optional<HttpServerResponse>(
                                HttpServerResponse::buffered(text_response(403, "forbidden"))));
                        return ca::core::Ok(std::optional<HttpServerResponse>{});
                    })
                    .is_ok());
    ASSERT_TRUE(server
                    .add_middleware([&](const HttpServerRequestContext&) {
                        after_auth_calls.fetch_add(1);
                        return ca::core::Ok(std::optional<HttpServerResponse>{});
                    })
                    .is_ok());
    ASSERT_TRUE(server
                    .route("POST",
                           "/secure",
                           [&](const HttpServerRequestContext&) {
                               route_calls.fetch_add(1);
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "accepted")));
                           })
                    .is_ok());

    ServerRunner runner(std::move(server));
    auto         created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto       client  = std::move(created).unwrap();
    const auto request = [&](std::string method, std::string_view path, bool authorized) {
        HttpRequest value;
        value.method = std::move(method);
        if (authorized)
            EXPECT_TRUE(value.headers.append("Authorization", "Bearer secret").is_ok());
        return client.request(server_url(address, path), std::move(value));
    };

    auto unknown_without_auth = request("GET", "/missing", false);
    ASSERT_TRUE(unknown_without_auth.is_ok());
    EXPECT_EQ(unknown_without_auth.unwrap().status, 403);

    auto method_without_auth = request("PUT", "/secure", false);
    ASSERT_TRUE(method_without_auth.is_ok());
    EXPECT_EQ(method_without_auth.unwrap().status, 403);

    auto accepted = request("POST", "/secure", true);
    ASSERT_TRUE(accepted.is_ok());
    EXPECT_EQ(accepted.unwrap().status, 200);

    auto method_not_allowed = request("PUT", "/secure", true);
    ASSERT_TRUE(method_not_allowed.is_ok());
    EXPECT_EQ(method_not_allowed.unwrap().status, 405);

    auto not_found = request("GET", "/missing", true);
    ASSERT_TRUE(not_found.is_ok());
    EXPECT_EQ(not_found.unwrap().status, 404);

    {
        std::lock_guard<std::mutex> lock(order_mutex);
        ASSERT_EQ(order.size(), 10U);
        for (usize index = 0; index < order.size(); index += 2) {
            EXPECT_EQ(order[index], "audit");
            EXPECT_EQ(order[index + 1], "auth");
        }
    }
    EXPECT_EQ(after_auth_calls.load(), 3U);
    EXPECT_EQ(route_calls.load(), 1U);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, MapsMiddlewareErrorsAndExceptionsToInternalServerError)
{
    auto server         = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    ASSERT_TRUE(server
                    .add_middleware([](const HttpServerRequestContext& context)
                                        -> HttpResult<std::optional<HttpServerResponse>> {
                        if (context.request().target == "/error")
                            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                                      "middleware test error"));
                        if (context.request().target == "/throw")
                            throw std::runtime_error("middleware test exception");
                        return ca::core::Ok(std::optional<HttpServerResponse>{});
                    })
                    .is_ok());

    ServerRunner runner(std::move(server));
    auto         created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto client = std::move(created).unwrap();

    auto error = client.get(server_url(address, "/error"));
    ASSERT_TRUE(error.is_ok());
    EXPECT_EQ(error.unwrap().status, 500);
    EXPECT_FALSE(client.has_open_connection());

    auto exception = client.get(server_url(address, "/throw"));
    ASSERT_TRUE(exception.is_ok());
    EXPECT_EQ(exception.unwrap().status, 500);
    EXPECT_FALSE(client.has_open_connection());

    auto missing = client.get(server_url(address, "/missing"));
    ASSERT_TRUE(missing.is_ok());
    EXPECT_EQ(missing.unwrap().status, 404);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, StreamsChunkedResponseWithTrailersAndStopsIdleWorker)
{
    auto server         = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    ASSERT_TRUE(
        server
            .route("GET",
                   "/events",
                   [](const HttpServerRequestContext&) {
                       HttpResponseHead head;
                       EXPECT_TRUE(
                           head.headers.append("Content-Type", "text/event-stream").is_ok());
                       EXPECT_TRUE(head.headers.append("Trailer", "X-End").is_ok());
                       HttpHeaders trailers;
                       EXPECT_TRUE(trailers.append("X-End", "yes").is_ok());
                       return ca::core::Ok(HttpServerResponse::chunked(
                           std::move(head),
                           [](Http1ChunkedBodyWriter& body, const ca::thread::StopToken& token) {
                               EXPECT_FALSE(token.stop_requested());
                               auto first = body.write_chunk("event: one\ndata: 1\n\n");
                               if (first.is_err())
                                   return first;
                               auto flushed = body.flush();
                               if (flushed.is_err())
                                   return flushed;
                               return body.write_chunk("event: two\ndata: 2\n\n");
                           },
                           std::move(trailers)));
                   })
            .is_ok());

    ServerRunner runner(std::move(server));
    auto         created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto client = std::move(created).unwrap();

    auto response = client.get(server_url(address, "/events"));
    ASSERT_TRUE(response.is_ok()) << response.unwrap_err().to_string();
    EXPECT_EQ(response.unwrap().status, 200);
    EXPECT_EQ(body_text(response.unwrap().body), "event: one\ndata: 1\n\nevent: two\ndata: 2\n\n");
    EXPECT_EQ(response.unwrap().trailers.get("X-End"), "yes");
    EXPECT_TRUE(client.has_open_connection());

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, BuffersBodiesLargerThanInitialCapacity)
{
    auto server         = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    ASSERT_TRUE(server
                    .route("POST",
                           "/echo",
                           [](const HttpServerRequestContext& context) {
                               HttpResponse response;
                               response.status = 200;
                               response.body   = context.request().body;
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(std::move(response)));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    auto created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto client = std::move(created).unwrap();

    const std::string body(12 * 1024, 'x');
    HttpRequest       request;
    request.method = "POST";
    request.body   = body_bytes(body);
    auto response  = client.request(server_url(address, "/echo"), std::move(request));
    ASSERT_TRUE(response.is_ok()) << response.unwrap_err().to_string();
    EXPECT_EQ(response.unwrap().status, 200);
    EXPECT_EQ(body_text(response.unwrap().body), body);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, MapsBodyLimitAndUnsupportedExpectation)
{
    HttpServerOptions options;
    options.request_limits.max_body_bytes = 4;
    auto server                           = bind_server(options);
    auto address_result                   = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();
    ASSERT_TRUE(server
                    .route("POST",
                           "/upload",
                           [](const HttpServerRequestContext& context) {
                               return ca::core::Ok(HttpServerResponse::buffered(
                                   text_response(200, body_text(context.request().body))));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    auto created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto client = std::move(created).unwrap();

    HttpRequest oversized;
    oversized.method = "POST";
    oversized.body   = body_bytes("hello");
    auto too_large   = client.request(server_url(address, "/upload"), std::move(oversized));
    ASSERT_TRUE(too_large.is_ok()) << too_large.unwrap_err().to_string();
    EXPECT_EQ(too_large.unwrap().status, 413);
    EXPECT_FALSE(client.has_open_connection());

    HttpRequest expectation;
    expectation.method = "POST";
    ASSERT_TRUE(expectation.headers.append("Expect", "something-else").is_ok());
    auto rejected = client.request(server_url(address, "/upload"), std::move(expectation));
    ASSERT_TRUE(rejected.is_ok()) << rejected.unwrap_err().to_string();
    EXPECT_EQ(rejected.unwrap().status, 417);
    EXPECT_FALSE(client.has_open_connection());

    HttpRequest continued;
    continued.method = "POST";
    continued.body   = body_bytes("data");
    ASSERT_TRUE(continued.headers.append("Expect", "100-continue").is_ok());
    auto accepted = client.request(server_url(address, "/upload"), std::move(continued));
    ASSERT_TRUE(accepted.is_ok()) << accepted.unwrap_err().to_string();
    EXPECT_EQ(accepted.unwrap().status, 200);
    EXPECT_EQ(body_text(accepted.unwrap().body), "data");

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, RejectsConnectionsBeyondWorkerAndPendingCapacity)
{
    HttpServerOptions options;
    options.worker_threads            = 1;
    options.pending_connections       = 1;
    options.overload_response_timeout = std::chrono::milliseconds(200);
    options.stop_poll_interval        = std::chrono::milliseconds(5);
    auto server                       = bind_server(options);
    auto address_result               = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    std::promise<void> entered_promise;
    auto               entered = entered_promise.get_future();
    std::promise<void> gate_promise;
    auto               gate = gate_promise.get_future().share();
    std::atomic<bool>  first_handler{true};
    ASSERT_TRUE(server
                    .route("GET",
                           "/block",
                           [&](const HttpServerRequestContext&) {
                               if (first_handler.exchange(false))
                                   entered_promise.set_value();
                               gate.wait();
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "released")));
                           })
                    .is_ok());

    ServerRunner runner(std::move(server));
    const auto   url = server_url(address, "/block");
    struct ClientOutcome
    {
        u16         status{0};
        std::string error;
    };
    auto request = [url] {
        HttpClientOptions client_options;
        client_options.connect_timeout         = std::chrono::milliseconds(5000);
        client_options.request_write_timeout   = std::chrono::milliseconds(5000);
        client_options.response_header_timeout = std::chrono::milliseconds(5000);
        client_options.response_body_timeout   = std::chrono::milliseconds(5000);
        auto created                           = HttpClient::create(client_options);
        if (created.is_err())
            return ClientOutcome{0, created.unwrap_err().to_string()};
        auto client   = std::move(created).unwrap();
        auto response = client.get(url);
        if (response.is_err())
            return ClientOutcome{0, response.unwrap_err().to_string()};
        return ClientOutcome{response.unwrap().status, {}};
    };

    auto first = std::async(std::launch::async, request);
    ASSERT_EQ(entered.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    std::vector<std::future<ClientOutcome>> later;
    later.push_back(std::async(std::launch::async, request));
    later.push_back(std::async(std::launch::async, request));

    bool       overload_ready = false;
    const auto deadline       = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!overload_ready && std::chrono::steady_clock::now() < deadline) {
        for (auto& response : later) {
            if (response.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                overload_ready = true;
                break;
            }
        }
        if (!overload_ready)
            std::this_thread::yield();
    }
    EXPECT_TRUE(overload_ready);
    gate_promise.set_value();

    const auto first_outcome = first.get();
    EXPECT_EQ(first_outcome.status, 200) << first_outcome.error;
    usize success_count  = first_outcome.status == 200 ? 1 : 0;
    usize overload_count = 0;
    for (auto& response : later) {
        const auto outcome = response.get();
        success_count += outcome.status == 200 ? 1 : 0;
        overload_count += outcome.status == 503 ? 1 : 0;
        EXPECT_TRUE(outcome.status == 200 || outcome.status == 503)
            << "unexpected status " << outcome.status << ": " << outcome.error;
    }
    EXPECT_EQ(success_count, 2U);
    EXPECT_EQ(overload_count, 1U);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpClientServerTest, ValidatesOptionsRoutesAndHttpsCapability)
{
    HttpClientOptions client_options;
    client_options.connect_timeout = std::chrono::milliseconds(0);
    EXPECT_EQ(HttpClient::create(client_options).unwrap_err().kind(), HttpErrorKind::InvalidState);

    client_options.connect_timeout       = std::chrono::milliseconds(10000);
    client_options.tls_handshake_timeout = std::chrono::milliseconds(0);
    EXPECT_EQ(HttpClient::create(client_options).unwrap_err().kind(), HttpErrorKind::InvalidState);

    client_options.tls_handshake_timeout = std::chrono::milliseconds(10000);
    client_options.tls.ca_file           = std::string("ca.pem\0ignored", 14);
    EXPECT_EQ(HttpClient::create(client_options).unwrap_err().kind(), HttpErrorKind::InvalidState);

    HttpServerOptions server_options;
    server_options.pending_connections = 0;
    auto invalid_server =
        HttpServer::bind(net::SocketAddress(net::IpAddress::localhost_v4(), 0), server_options);
    EXPECT_EQ(invalid_server.unwrap_err().kind(), HttpErrorKind::InvalidState);

    server_options.pending_connections       = 64;
    server_options.overload_response_timeout = std::chrono::milliseconds(0);
    auto invalid_timeout_server =
        HttpServer::bind(net::SocketAddress(net::IpAddress::localhost_v4(), 0), server_options);
    EXPECT_EQ(invalid_timeout_server.unwrap_err().kind(), HttpErrorKind::InvalidState);

    auto server = bind_server();
    EXPECT_EQ(server.add_middleware(HttpRequestMiddleware()).unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);
    EXPECT_EQ(server
                  .route("GET",
                         "missing-slash",
                         [](const HttpServerRequestContext&) {
                             return ca::core::Ok(
                                 HttpServerResponse::buffered(text_response(200, "unused")));
                         })
                  .unwrap_err()
                  .kind(),
              HttpErrorKind::InvalidMessage);

    auto created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto client = std::move(created).unwrap();
    auto https  = HttpUrl::parse("https://localhost/mcp");
    ASSERT_TRUE(https.is_ok());
#if defined(LIBCA_HTTP_HAS_OPENSSL)
    EXPECT_TRUE(HttpClient::supports_https());
#else
    EXPECT_FALSE(HttpClient::supports_https());
    EXPECT_EQ(client.get(https.unwrap()).unwrap_err().kind(), HttpErrorKind::Unsupported);
#endif
}

// ==================== R2 评审修复 ====================

TEST(HttpClientServerTest, RetriesIdempotentRequestOnStaleKeepAliveConnection)
{
    // 脚本化原始 server：第一条连接响应后立即整体关闭（模拟 idle 回收），
    // 第二条连接正常响应。复用到 stale 连接的幂等请求应自动重试一次成功。
    // 线程以非阻塞 accept 轮询运行并受 server_stop 守护，任何失败路径都能安全 join，
    // 不会因 ASSERT 提前返回触发 joinable 线程析构的 std::terminate。
    auto bound =
        net::TcpListener::bind(net::SocketAddress(net::IpAddress::localhost_v4(), 0));
    ASSERT_TRUE(bound.is_ok());
    auto listener = std::move(bound).unwrap();
    auto local = listener.local_address();
    ASSERT_TRUE(local.is_ok());
    const auto address = local.unwrap();

    std::atomic<bool> server_stop{false};
    std::thread server_thread([&listener, &server_stop] {
        const std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
        if (!listener.set_nonblocking(true).is_ok())
            return;
        for (int connection_index = 0;
             connection_index < 2 && !server_stop.load(std::memory_order_relaxed);
             ++connection_index) {
            std::optional<net::TcpStream> stream;
            while (!server_stop.load(std::memory_order_relaxed)) {
                auto accepted = listener.accept();
                if (accepted.is_ok()) {
                    stream = std::move(std::move(accepted).unwrap().stream);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (!stream)
                return;
            std::string received;
            std::array<char, 2048> buffer{};
            while (received.find("\r\n\r\n") == std::string::npos) {
                auto read = stream->read(reinterpret_cast<u8*>(buffer.data()), buffer.size());
                if (read.is_err() || read.unwrap() == 0)
                    return;
                received.append(buffer.data(), read.unwrap());
            }
            auto written =
                stream->write(reinterpret_cast<const u8*>(response.data()), response.size());
            (void)written;
        }
    });

    struct ServerThreadGuard {
        std::thread         thread;
        std::atomic<bool>&  stop;
        ~ServerThreadGuard()
        {
            stop.store(true, std::memory_order_relaxed);
            if (thread.joinable())
                thread.join();
        }
    } guard{std::move(server_thread), server_stop};

    auto created = HttpClient::create();
    ASSERT_TRUE(created.is_ok());
    auto client = std::move(created).unwrap();

    auto first = client.get(server_url(address, "/first"));
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(first.unwrap().status, 200);

    // 第二次请求落在已被服务器关闭的复用连接上：应在新建连接上重试成功。
    auto second = client.get(server_url(address, "/second"));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(second.unwrap().status, 200);
    EXPECT_EQ(body_text(second.unwrap().body), "ok");
}

TEST(HttpClientServerTest, Http10ExpectContinueSkipsInterimResponse)
{
    // RFC 9110 10.1.1：100-continue 仅定义于 HTTP/1.1。HTTP/1.0 请求携带
    // Expect: 100-continue 时不应收到 interim response，服务器直接读 body
    // 并回最终响应。
    auto server = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();
    ASSERT_TRUE(server
                    .route("POST",
                           "/upload",
                           [](const HttpServerRequestContext& context) {
                               return ca::core::Ok(HttpServerResponse::buffered(
                                   text_response(200, body_text(context.request().body))));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    auto connected =
        net::TcpStream::connect_timeout("127.0.0.1", address.port(), std::chrono::seconds(5));
    ASSERT_TRUE(connected.is_ok());
    auto stream = std::move(connected).unwrap();
    ASSERT_TRUE(stream.set_read_timeout(std::chrono::milliseconds(5000)).is_ok());

    const std::string request = "POST /upload HTTP/1.0\r\nHost: 127.0.0.1\r\n"
                                "Expect: 100-continue\r\nContent-Length: 4\r\n\r\nbody";
    auto written = stream.write(reinterpret_cast<const u8*>(request.data()), request.size());
    ASSERT_TRUE(written.is_ok());

    std::string received;
    std::array<char, 1024> buffer{};
    for (;;) {
        auto read = stream.read(reinterpret_cast<u8*>(buffer.data()), buffer.size());
        if (read.is_err() || read.unwrap() == 0)
            break;
        received.append(buffer.data(), read.unwrap());
    }
    EXPECT_NE(received.find("HTTP/1.0 200"), std::string::npos);
    EXPECT_EQ(received.find("100-continue"), std::string::npos);
    EXPECT_EQ(received.find(" 100 "), std::string::npos);
    EXPECT_NE(received.find("body"), std::string::npos);
}

TEST(HttpClientServerTest, ProtocolErrorIsDeliveredWhileClientStreamsBody)
{
    // body 超限时服务器在客户端仍在推送时就要回 413 并关闭。直接 close 会因
    // 未读入站数据触发 RST 吞掉响应（Windows 尤甚），应 shutdown 写半侧并
    // 排水后再关闭，保证客户端能收到 413。
    HttpServerOptions options;
    options.request_limits.max_body_bytes = 16;
    options.overload_response_timeout = std::chrono::milliseconds(2000);
    auto server = bind_server(options);
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();
    ASSERT_TRUE(server
                    .route("POST",
                           "/upload",
                           [](const HttpServerRequestContext& context) {
                               return ca::core::Ok(HttpServerResponse::buffered(
                                   text_response(200, body_text(context.request().body))));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    auto connected =
        net::TcpStream::connect_timeout("127.0.0.1", address.port(), std::chrono::seconds(5));
    ASSERT_TRUE(connected.is_ok());
    auto stream = std::move(connected).unwrap();
    ASSERT_TRUE(stream.set_write_timeout(std::chrono::milliseconds(5000)).is_ok());
    ASSERT_TRUE(stream.set_read_timeout(std::chrono::milliseconds(5000)).is_ok());

    const std::string head =
        "POST /upload HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 1048576\r\n\r\n";
    auto written = stream.write(reinterpret_cast<const u8*>(head.data()), head.size());
    ASSERT_TRUE(written.is_ok());

    // 持续推送远超限额的 body；服务器在收到第 17 字节后即回 413 并开始排水，
    // 之后写入失败是预期内的。
    const std::string chunk(4096, 'x');
    for (int i = 0; i < 64; ++i) {
        auto pushed = stream.write(reinterpret_cast<const u8*>(chunk.data()), chunk.size());
        if (pushed.is_err())
            break;
    }
    ASSERT_TRUE(stream.shutdown(net::Shutdown::Write).is_ok());

    std::string received;
    std::array<char, 1024> buffer{};
    for (;;) {
        auto read = stream.read(reinterpret_cast<u8*>(buffer.data()), buffer.size());
        if (read.is_err() || read.unwrap() == 0)
            break;
        received.append(buffer.data(), read.unwrap());
    }
    EXPECT_NE(received.find("413"), std::string::npos);
}

}   // namespace
}   // namespace ca::http::test
