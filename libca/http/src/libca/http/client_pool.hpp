#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "libca/core/datatype.hpp"
#include "libca/http/http_error.hpp"

namespace ca::http {

class HttpUrl;

class HttpConnectionPool;

namespace detail {

class ClientConnection;

// 连接池借还入口。连接句柄类型只在 detail 头文件与 libca_http 内部完整可见，公开的
// HttpConnectionPool 不暴露任何连接类型；HttpClient 的实现经 detail 头文件使用这两个入口。
std::unique_ptr<ClientConnection> pool_checkout(HttpConnectionPool& pool, const HttpUrl& url);
bool                             pool_checkin(HttpConnectionPool&              pool,
                                              std::unique_ptr<ClientConnection> connection);

}   // namespace detail

/// @brief 连接池的容量与空闲回收配置。
struct HttpConnectionPoolOptions
{
    /// @brief 同一 scheme/host/port 下最多保留的空闲连接数，必须大于 0；归还满额时新连接被丢弃。
    usize max_idle_per_host{4};

    /// @brief 空闲连接的最长保留时间（steady_clock 计时）；借出时发现超时即丢弃，必须大于 0。
    std::chrono::milliseconds idle_timeout{60000};
};

/// @brief 按 scheme/host/port 复用 keep-alive 连接的线程安全连接池。
/// @details 通过 HttpClientOptions::pool 注入，可被多个 HttpClient 共享。借出前做两级
/// 校验：空闲超时（steady_clock）与非阻塞 socket 探活，失效连接直接丢弃、由 client 重建；
/// 归还由 HttpClient 在响应体消费完且双方 keep-alive framing 允许时执行。整池一把 mutex
/// 保证线程安全；HTTPS 连接与普通连接同池管理，TLS 细节对池不可见。
class HttpConnectionPool
{
public:
    /// @brief 校验 options 并创建空池。
    static HttpResult<HttpConnectionPool> create(
        const HttpConnectionPoolOptions& options = HttpConnectionPoolOptions());

    HttpConnectionPool(HttpConnectionPool&& other) noexcept;
    HttpConnectionPool& operator=(HttpConnectionPool&& other) noexcept;
    ~HttpConnectionPool();

    /// @brief 返回池中空闲连接总数（所有 origin 合计，不含已借出连接）。
    usize idle_count() const noexcept;

    /// @brief 丢弃全部空闲连接；已借出的连接不受影响。
    void clear() noexcept;

private:
    friend std::unique_ptr<detail::ClientConnection> detail::pool_checkout(
        HttpConnectionPool&, const HttpUrl&);
    friend bool detail::pool_checkin(HttpConnectionPool&,
                                     std::unique_ptr<detail::ClientConnection>);

    class Impl;

    HttpConnectionPool() noexcept;

    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::http
