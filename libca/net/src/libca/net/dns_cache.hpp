#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "libca/io/error.hpp"
#include "libca/net/address.hpp"
#include "libca/net/dns.hpp"
#include "libca/net/socket.hpp"

namespace ca::net {

/// @brief 可注入的底层 resolver 签名，与 DnsResolver::resolve 一致。
using DnsResolveFn = std::function<io::IoResult<std::vector<SocketAddress>>(
    const std::string& host, u16 port, AddressFamily family, SocketKind kind)>;

/// @brief 单调时钟源签名，返回当前时刻；测试可注入假时钟避免真实睡眠。
using DnsTimeSource = std::function<std::chrono::steady_clock::time_point()>;

/// @brief 默认时钟源：steady_clock::now()。
std::chrono::steady_clock::time_point default_dns_time_source() noexcept;

/// @brief 缓存配置：TTL 与容量上限。
struct CachedDnsResolverOptions
{
    /// 缓存条目的存活时间，必须大于 0。
    std::chrono::milliseconds ttl{60000};

    /// 最多缓存的条目数，必须大于 0；超出按 LRU 淘汰。
    usize capacity{128};
};

/// @brief 解析缓存的命中统计。
struct DnsCacheStats
{
    usize hits{0};             ///< 命中且未过期的次数。
    usize misses{0};           ///< 未命中或已过期需要重新解析的次数。
    usize expiries{0};         ///< 命中缓存但条目已过期的次数。
    usize evictions{0};        ///< 因容量满被 LRU 淘汰的条目数。
    usize resolver_calls{0};   ///< 实际下发到底层 resolver 的解析次数。
};

/// @brief 带 TTL 的线程安全 DNS 正向缓存，以装饰方式包装底层 resolver。
///
/// 缓存键为 (host, port, family, kind) 四元组；命中且未过期直接返回缓存副本，
/// 过期后重新解析并刷新条目，容量满按 LRU 淘汰（复用 collection 的 LruCache，
/// net 位于 collection 之上，依赖方向合规）。底层 resolver 与时钟均可注入，
/// 默认分别使用 DnsResolver 与 steady_clock，行为与未包装时一致。
///
/// 设计取舍：
/// - 失败结果不缓存负项：DNS 的临时失败（如 EAI_AGAIN）被缓存会把瞬时故障放大成
///   TTL 时长内的持续失败；代价是解析失败的流量全部穿透到底层。
/// - 底层解析在锁外执行：慢解析不会阻塞其它线程的缓存读写，代价是并发 miss 同一个
///   键可能造成少量重复解析（缓存击穿），由调用方按需在上游串行化。
///
/// @note 同一实例可被多线程共享（内部互斥锁保护全部状态）。
class CachedDnsResolver
{
public:
    /// @brief 使用默认 resolver（DnsResolver）与默认时钟创建缓存。
    /// @throws 无；参数非法时返回 Err，不会抛出。
    static io::IoResult<CachedDnsResolver> create(
        const CachedDnsResolverOptions& options = CachedDnsResolverOptions());

    /// @brief 使用注入的 resolver 与时钟创建缓存；参数全部可注入便于测试。
    /// @param resolver 底层解析函数；为空返回 InvalidInput。
    /// @param now 时钟源；为空返回 InvalidInput。
    static io::IoResult<CachedDnsResolver> create(
        DnsResolveFn resolver, const CachedDnsResolverOptions& options = CachedDnsResolverOptions(),
        DnsTimeSource now = default_dns_time_source);

    CachedDnsResolver(CachedDnsResolver&& other) noexcept;
    CachedDnsResolver& operator=(CachedDnsResolver&& other) noexcept;
    ~CachedDnsResolver();

    /// @brief 带缓存地解析主机名，语义与 DnsResolver::resolve 一致。
    /// @note 空主机名直接返回 InvalidInput，不进入缓存路径。
    io::IoResult<std::vector<SocketAddress>> resolve(
        const std::string& host, u16 port, AddressFamily family = AddressFamily::Unspecified,
        SocketKind kind = SocketKind::Stream);

    /// @brief 返回累计命中统计的快照。
    DnsCacheStats stats() const;

private:
    class Impl;

    CachedDnsResolver() noexcept;

    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::net
