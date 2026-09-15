#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
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

HttpClient make_client(const std::shared_ptr<HttpConnectionPool>& pool)
{
    HttpClientOptions options;
    options.pool = pool;
    auto created = HttpClient::create(options);
    EXPECT_TRUE(created.is_ok()) << (created.is_err() ? created.unwrap_err().to_string() : "");
    return std::move(created).unwrap();
}

// 记录 handler 收到请求时的对端端口，用于以 server 端连接数验证池的复用行为。
struct PeerRecorder
{
    std::mutex       mutex;
    std::vector<u16> ports;

    void record(const HttpServerRequestContext& context)
    {
        std::lock_guard<std::mutex> lock(mutex);
        ports.push_back(context.peer_address().port());
    }

    std::vector<u16> snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return ports;
    }
};

TEST(HttpConnectionPoolTest, ValidatesPoolOptions)
{
    HttpConnectionPoolOptions options;
    options.max_idle_per_host = 0;
    EXPECT_EQ(HttpConnectionPool::create(options).unwrap_err().kind(),
              HttpErrorKind::InvalidState);

    options.max_idle_per_host = 2;
    options.idle_timeout      = std::chrono::milliseconds(0);
    EXPECT_EQ(HttpConnectionPool::create(options).unwrap_err().kind(),
              HttpErrorKind::InvalidState);
}

TEST(HttpConnectionPoolTest, ReusesIdleConnectionForSameOriginAcrossClients)
{
    auto server         = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    PeerRecorder recorder;
    ASSERT_TRUE(server
                    .route("GET",
                           "/hello",
                           [&](const HttpServerRequestContext& context) {
                               recorder.record(context);
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "hello")));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    auto pool =
        std::make_shared<HttpConnectionPool>(std::move(HttpConnectionPool::create().unwrap()));
    auto client_a = make_client(pool);
    auto client_b = make_client(pool);

    auto first = client_a.get(server_url(address, "/hello"));
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(first.unwrap().status, 200);
    EXPECT_EQ(body_text(first.unwrap().body), "hello");
    // 请求完成后连接已归还池中，client 自身不再持有。
    EXPECT_FALSE(client_a.has_open_connection());
    EXPECT_EQ(pool->idle_count(), 1U);

    // 第二个 client 共享同一池：应复用同一条连接，server 只看到一个连接。
    auto second = client_b.get(server_url(address, "/hello"));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(second.unwrap().status, 200);
    EXPECT_EQ(pool->idle_count(), 1U);

    const auto ports = recorder.snapshot();
    ASSERT_EQ(ports.size(), 2U);
    EXPECT_EQ(ports[0], ports[1]);

    // clear() 丢弃全部空闲连接。
    pool->clear();
    EXPECT_EQ(pool->idle_count(), 0U);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpConnectionPoolTest, DiscardsIdleExpiredConnectionAndRebuilds)
{
    auto server         = bind_server();
    auto address_result = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();

    PeerRecorder recorder;
    ASSERT_TRUE(server
                    .route("GET",
                           "/hello",
                           [&](const HttpServerRequestContext& context) {
                               recorder.record(context);
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "hello")));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    HttpConnectionPoolOptions pool_options;
    pool_options.idle_timeout = std::chrono::milliseconds(50);
    auto pool =
        std::make_shared<HttpConnectionPool>(std::move(HttpConnectionPool::create(pool_options).unwrap()));
    auto client = make_client(pool);

    auto first = client.get(server_url(address, "/hello"));
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(pool->idle_count(), 1U);

    // 超过空闲期限后，checkout 应丢弃过期连接并重建新连接（对端端口变化）。
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto second = client.get(server_url(address, "/hello"));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(second.unwrap().status, 200);
    EXPECT_EQ(pool->idle_count(), 1U);

    const auto ports = recorder.snapshot();
    ASSERT_EQ(ports.size(), 2U);
    EXPECT_NE(ports[0], ports[1]);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpConnectionPoolTest, KeepsDifferentOriginsSeparate)
{
    auto server_a = bind_server();
    auto address_a_result = server_a.local_address();
    ASSERT_TRUE(address_a_result.is_ok());
    const auto address_a = address_a_result.unwrap();
    auto server_b = bind_server();
    auto address_b_result = server_b.local_address();
    ASSERT_TRUE(address_b_result.is_ok());
    const auto address_b = address_b_result.unwrap();
    ASSERT_NE(address_a.port(), address_b.port());

    PeerRecorder recorder_a;
    PeerRecorder recorder_b;
    ASSERT_TRUE(server_a
                    .route("GET",
                           "/hello",
                           [&](const HttpServerRequestContext& context) {
                               recorder_a.record(context);
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "alpha")));
                           })
                    .is_ok());
    ASSERT_TRUE(server_b
                    .route("GET",
                           "/hello",
                           [&](const HttpServerRequestContext& context) {
                               recorder_b.record(context);
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "beta")));
                           })
                    .is_ok());
    ServerRunner runner_a(std::move(server_a));
    ServerRunner runner_b(std::move(server_b));

    auto pool =
        std::make_shared<HttpConnectionPool>(std::move(HttpConnectionPool::create().unwrap()));
    auto client = make_client(pool);

    auto from_a = client.get(server_url(address_a, "/hello"));
    ASSERT_TRUE(from_a.is_ok()) << from_a.unwrap_err().to_string();
    EXPECT_EQ(body_text(from_a.unwrap().body), "alpha");
    auto from_b = client.get(server_url(address_b, "/hello"));
    ASSERT_TRUE(from_b.is_ok()) << from_b.unwrap_err().to_string();
    EXPECT_EQ(body_text(from_b.unwrap().body), "beta");
    EXPECT_EQ(pool->idle_count(), 2U);

    // 再次请求 server A：必须取回 A 自己的空闲连接，而不是混用 B 的（响应体可区分）。
    auto from_a_again = client.get(server_url(address_a, "/hello"));
    ASSERT_TRUE(from_a_again.is_ok()) << from_a_again.unwrap_err().to_string();
    EXPECT_EQ(body_text(from_a_again.unwrap().body), "alpha");
    EXPECT_EQ(pool->idle_count(), 2U);

    const auto ports_a = recorder_a.snapshot();
    const auto ports_b = recorder_b.snapshot();
    ASSERT_EQ(ports_a.size(), 2U);
    EXPECT_EQ(ports_a[0], ports_a[1]);
    ASSERT_EQ(ports_b.size(), 1U);
    EXPECT_NE(ports_a[0], ports_b[0]);

    auto finished_a = runner_a.finish();
    auto finished_b = runner_b.finish();
    ASSERT_TRUE(finished_a.is_ok()) << finished_a.unwrap_err().to_string();
    ASSERT_TRUE(finished_b.is_ok()) << finished_b.unwrap_err().to_string();
}

TEST(HttpConnectionPoolTest, ProbesAndRebuildsAfterServerClosedIdleConnection)
{
    // 脚本化原始 server：每条连接响应一次后立即整体关闭。归还池中的连接在下一次
    // checkout 时应被探活识别为失效并丢弃重建；用非幂等的 POST 验证——若探活失效，
    // 写入会撞上 reset 且 POST 不会自动重试，请求必然失败。
    auto bound = net::TcpListener::bind(net::SocketAddress(net::IpAddress::localhost_v4(), 0));
    ASSERT_TRUE(bound.is_ok());
    auto listener = std::move(bound).unwrap();
    auto local    = listener.local_address();
    ASSERT_TRUE(local.is_ok());
    const auto address = local.unwrap();

    std::atomic<bool>  server_stop{false};
    std::atomic<usize> accepted{0};
    std::thread server_thread([&listener, &server_stop, &accepted] {
        const std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
        if (!listener.set_nonblocking(true).is_ok())
            return;
        for (usize connection_index = 0;
             connection_index < 2 && !server_stop.load(std::memory_order_relaxed);
             ++connection_index) {
            std::optional<net::TcpStream> stream;
            while (!server_stop.load(std::memory_order_relaxed)) {
                auto accepted_result = listener.accept();
                if (accepted_result.is_ok()) {
                    stream = std::move(std::move(accepted_result).unwrap().stream);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (!stream)
                return;
            // Windows 上 accept 出的 socket 继承 listener 的非阻塞态，必须显式恢复阻塞。
            if (!stream->set_nonblocking(false).is_ok())
                return;
            std::string            received;
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
            accepted.fetch_add(1);
            // 响应后立即关闭，连接在下一次 checkout 探活时应判定为失效。
            stream.reset();
        }
    });

    struct ServerThreadGuard
    {
        std::thread        thread;
        std::atomic<bool>& stop;
        std::atomic<usize>& accepted;
        ~ServerThreadGuard()
        {
            stop.store(true, std::memory_order_relaxed);
            if (thread.joinable())
                thread.join();
        }
    } guard{std::move(server_thread), server_stop, accepted};

    auto pool =
        std::make_shared<HttpConnectionPool>(std::move(HttpConnectionPool::create().unwrap()));
    auto client = make_client(pool);

    HttpRequest first_request;
    first_request.method = "POST";
    auto first           = client.request(server_url(address, "/first"), std::move(first_request));
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(first.unwrap().status, 200);
    EXPECT_EQ(pool->idle_count(), 1U);

    // 等待 server 的关闭到达本端，checkout 探活即可确定性地判定连接失效。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    HttpRequest second_request;
    second_request.method = "POST";
    auto second = client.request(server_url(address, "/second"), std::move(second_request));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(second.unwrap().status, 200);
    EXPECT_EQ(body_text(second.unwrap().body), "ok");
    // server 在写完响应后才递增 accepted 计数，等它被调度执行，避免与 client 完成请求竞争。
    const auto accepted_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (accepted.load() < 2 && std::chrono::steady_clock::now() < accepted_deadline)
        std::this_thread::yield();
    EXPECT_EQ(accepted.load(), 2U);
    EXPECT_EQ(pool->idle_count(), 1U);
}

TEST(HttpConnectionPoolTest, RejectsCheckinBeyondMaxIdlePerHost)
{
    PeerRecorder      recorder;
    std::promise<void> entered_promise;
    auto               entered = entered_promise.get_future();
    std::promise<void> gate_promise;
    auto               gate = gate_promise.get_future().share();
    std::atomic<usize> hits{0};
    HttpServerOptions  server_options;
    server_options.worker_threads = 4;
    auto server                   = bind_server(server_options);
    auto address_result           = server.local_address();
    ASSERT_TRUE(address_result.is_ok());
    const auto address = address_result.unwrap();
    ASSERT_TRUE(server
                    .route("GET",
                           "/gate",
                           [&](const HttpServerRequestContext& context) {
                               recorder.record(context);
                               // 只有第一个进入的 handler 阻塞，等第二个请求完成归还后再放行。
                               if (hits.fetch_add(1) == 0)
                               {
                                   entered_promise.set_value();
                                   gate.wait();
                               }
                               return ca::core::Ok(
                                   HttpServerResponse::buffered(text_response(200, "done")));
                           })
                    .is_ok());
    ServerRunner runner(std::move(server));

    HttpConnectionPoolOptions pool_options;
    pool_options.max_idle_per_host = 1;
    auto pool = std::make_shared<HttpConnectionPool>(
        std::move(HttpConnectionPool::create(pool_options).unwrap()));
    auto client_a = make_client(pool);
    auto client_b = make_client(pool);

    // client A 的请求阻塞在 handler 中，连接处于借出状态。
    auto first = std::async(std::launch::async,
                            [&] { return client_a.get(server_url(address, "/gate")); });
    ASSERT_EQ(entered.wait_for(std::chrono::seconds(2)), std::future_status::ready);

    // client B 借助另一条连接完成请求并归还，池中同 origin 空闲数达到 max_idle_per_host。
    auto second = client_b.get(server_url(address, "/gate"));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(pool->idle_count(), 1U);

    // 放行 A：其连接在归还时因满额被丢弃，池中空闲数不应增长。
    gate_promise.set_value();
    auto first_outcome = first.get();
    ASSERT_TRUE(first_outcome.is_ok()) << first_outcome.unwrap_err().to_string();
    EXPECT_EQ(first_outcome.unwrap().status, 200);
    EXPECT_EQ(pool->idle_count(), 1U);

    const auto ports = recorder.snapshot();
    ASSERT_EQ(ports.size(), 2U);
    EXPECT_NE(ports[0], ports[1]);

    auto finished = runner.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
}

TEST(HttpConnectionPoolTest, MovedFromPoolIsInert)
{
    auto created = HttpConnectionPool::create();
    ASSERT_TRUE(created.is_ok());
    auto pool  = std::move(created).unwrap();
    auto moved = std::move(pool);

    // moved-from 池的公开操作都是无副作用的空操作。
    EXPECT_EQ(pool.idle_count(), 0U);
    pool.clear();
    EXPECT_EQ(pool.idle_count(), 0U);
    EXPECT_EQ(moved.idle_count(), 0U);
}

}   // namespace
}   // namespace ca::http::test
