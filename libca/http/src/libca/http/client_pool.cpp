#include "libca/http/client_pool.hpp"

#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "libca/http/detail/client_connection.hpp"

namespace ca::http {
namespace {

// 池内 origin 键：host 统一转 ASCII 小写，与 client 的同源判定（大小写不敏感）保持一致。
struct PoolOrigin
{
    HttpScheme  scheme{HttpScheme::Http};
    std::string host;
    u16         port{0};

    bool operator<(const PoolOrigin& other) const noexcept
    {
        if (scheme != other.scheme)
            return scheme < other.scheme;
        if (port != other.port)
            return port < other.port;
        return host < other.host;
    }
};

PoolOrigin make_origin(HttpScheme scheme, std::string_view host, u16 port)
{
    PoolOrigin origin;
    origin.scheme = scheme;
    origin.port   = port;
    origin.host.reserve(host.size());
    for (const char character : host) {
        auto byte = static_cast<unsigned char>(character);
        if (byte >= 'A' && byte <= 'Z')
            byte = static_cast<unsigned char>(byte + ('a' - 'A'));
        origin.host.push_back(static_cast<char>(byte));
    }
    return origin;
}

}   // namespace

class HttpConnectionPool::Impl
{
public:
    struct IdleEntry
    {
        std::unique_ptr<detail::ClientConnection> connection;
        std::chrono::steady_clock::time_point     idle_since;
    };

    explicit Impl(HttpConnectionPoolOptions value)
        : options(std::move(value))
    {}

    HttpConnectionPoolOptions                    options;
    std::mutex                                   mutex;
    std::map<PoolOrigin, std::vector<IdleEntry>> idle;
};

HttpConnectionPool::HttpConnectionPool() noexcept = default;

HttpConnectionPool::HttpConnectionPool(HttpConnectionPool&& other) noexcept = default;

HttpConnectionPool& HttpConnectionPool::operator=(HttpConnectionPool&& other) noexcept = default;

HttpConnectionPool::~HttpConnectionPool() = default;

HttpResult<HttpConnectionPool> HttpConnectionPool::create(const HttpConnectionPoolOptions& options)
{
    if (options.max_idle_per_host == 0 || options.idle_timeout.count() <= 0)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP connection pool limits must be positive"));
    HttpConnectionPool pool;
    pool.impl_ = std::make_unique<Impl>(options);
    return ca::core::Ok(std::move(pool));
}

usize HttpConnectionPool::idle_count() const noexcept
{
    if (impl_ == nullptr)
        return 0;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    usize                       total = 0;
    for (const auto& entry : impl_->idle)
        total += entry.second.size();
    return total;
}

void HttpConnectionPool::clear() noexcept
{
    if (impl_ == nullptr)
        return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->idle.clear();
}

namespace detail {

std::unique_ptr<ClientConnection> pool_checkout(HttpConnectionPool& pool, const HttpUrl& url)
{
    if (pool.impl_ == nullptr)
        return nullptr;
    const auto origin = make_origin(url.scheme(), url.host(), url.port());
    const auto now    = std::chrono::steady_clock::now();
    for (;;) {
        std::unique_ptr<ClientConnection> candidate;
        {
            std::lock_guard<std::mutex> lock(pool.impl_->mutex);
            auto bucket = pool.impl_->idle.find(origin);
            if (bucket == pool.impl_->idle.end())
                return nullptr;
            // 持锁只做弹出：从最新归还的连接向后取，空闲超时的直接丢弃，取到第一个
            // 未过期候选即出锁；取尽顺带清掉空桶，避免 map 残留空 vector 条目。
            while (!bucket->second.empty()) {
                auto entry = std::move(bucket->second.back());
                bucket->second.pop_back();
                if (now - entry.idle_since < pool.impl_->options.idle_timeout) {
                    candidate = std::move(entry.connection);
                    break;
                }
            }
            if (bucket->second.empty())
                pool.impl_->idle.erase(bucket);
            if (candidate == nullptr)
                return nullptr;
        }
        // 探活在锁外执行：probe_alive 含 set_nonblocking/read 系统调用（TLS 连接更贵），
        // 持锁做会把所有借出/归还方串行化在一次 socket I/O 上。失败连接在锁外析构，
        // 回循环重新取下一个候选。
        if (candidate->probe_alive())
            return candidate;
    }
}

bool pool_checkin(HttpConnectionPool& pool, std::unique_ptr<ClientConnection> connection)
{
    if (pool.impl_ == nullptr || connection == nullptr)
        return false;
    auto  origin = make_origin(connection->scheme, connection->host, connection->port);
    std::lock_guard<std::mutex> lock(pool.impl_->mutex);
    auto& bucket = pool.impl_->idle[origin];
    if (bucket.size() >= pool.impl_->options.max_idle_per_host)
        return false;
    bucket.push_back({std::move(connection), std::chrono::steady_clock::now()});
    return true;
}

}   // namespace detail

}   // namespace ca::http
