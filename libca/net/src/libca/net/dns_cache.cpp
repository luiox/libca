#include "libca/net/dns_cache.hpp"

#include <mutex>
#include <utility>

#include "libca/collection/lru_cache.hpp"

namespace ca::net {
namespace {

/// @brief 缓存键：与 resolve() 全部影响结果的入参一一对应。
struct CacheKey
{
    std::string   host;
    u16           port{0};
    AddressFamily family{AddressFamily::Unspecified};
    SocketKind    kind{SocketKind::Stream};

    bool operator==(const CacheKey& other) const noexcept
    {
        return port == other.port && family == other.family && kind == other.kind &&
               host == other.host;
    }
};

/// @brief 缓存键哈希：字符串哈希为种子，整数键逐步混合。
struct CacheKeyHash
{
    usize operator()(const CacheKey& key) const noexcept
    {
        usize      seed = std::hash<std::string>()(key.host);
        const auto mix  = [&seed](usize value) {
            constexpr usize golden = static_cast<usize>(0x9e3779b97f4a7c15ULL);
            seed ^= value + golden + (seed << 6) + (seed >> 2);
        };
        mix(std::hash<u16>()(key.port));
        mix(std::hash<int>()(static_cast<int>(key.family)));
        mix(std::hash<int>()(static_cast<int>(key.kind)));
        return seed;
    }
};

/// @brief 缓存条目：解析结果与过期时刻。
struct CacheEntry
{
    std::vector<SocketAddress>            addresses;
    std::chrono::steady_clock::time_point expires_at{};
};

DnsResolveFn default_resolver()
{
    return [](const std::string& host, u16 port, AddressFamily family, SocketKind kind) {
        return DnsResolver::resolve(host, port, family, kind);
    };
}

}   // namespace

/// @brief 缓存全部状态；互斥锁保护 LruCache（其本身不线程安全）与计数器。
class CachedDnsResolver::Impl
{
public:
    Impl(DnsResolveFn resolver, DnsTimeSource now, std::chrono::milliseconds ttl, usize capacity)
        : cache_(capacity)
        , resolver_(std::move(resolver))
        , now_(std::move(now))
        , ttl_(ttl)
    {}

    io::IoResult<std::vector<SocketAddress>> resolve(const std::string& host, u16 port,
                                                     AddressFamily family, SocketKind kind)
    {
        if (host.empty())
            return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                        "DNS host must not be empty"));

        const CacheKey key{host, port, family, kind};
        {
            std::lock_guard<std::mutex> guard(mutex_);
            auto*                       entry = cache_.get(key);
            if (entry != nullptr) {
                const auto current = now_();
                if (current < entry->expires_at) {
                    hits_ += 1;
                    return ca::core::Ok(entry->addresses);   // 返回缓存副本
                }
                // 条目已过期：按未命中处理并移除，交给下面的解析路径刷新。
                cache_.remove(key);
                expiries_ += 1;
            }
            misses_ += 1;
        }

        // 底层解析在锁外执行：慢 DNS 不阻塞其它键的读写（可能并发重复解析，见类注释）。
        auto result = resolver_(host, port, family, kind);

        std::lock_guard<std::mutex> guard(mutex_);
        resolver_calls_ += 1;
        if (result.is_ok()) {
            // 失败结果不缓存负项；成功条目的过期时刻基于解析发起时的时钟，避免慢解析
            // 导致 TTL 被进一步拉长。
            auto       addresses  = std::move(result).unwrap();
            const auto current    = now_();
            const auto maximum    = std::chrono::steady_clock::time_point::max();
            const auto expires_at = current > maximum - ttl_ ? maximum : current + ttl_;
            cache_.put(key, CacheEntry{addresses, expires_at});
            return ca::core::Ok(std::move(addresses));
        }
        return result;
    }

    DnsCacheStats stats() const
    {
        std::lock_guard<std::mutex> guard(mutex_);
        DnsCacheStats               snapshot;
        snapshot.hits           = hits_;
        snapshot.misses         = misses_;
        snapshot.expiries       = expiries_;
        snapshot.evictions      = cache_.status().evictions;
        snapshot.resolver_calls = resolver_calls_;
        return snapshot;
    }

private:
    mutable std::mutex                                           mutex_;
    ca::collection::LruCache<CacheKey, CacheEntry, CacheKeyHash> cache_;
    DnsResolveFn                                                 resolver_;
    DnsTimeSource                                                now_;
    std::chrono::milliseconds                                    ttl_;

    usize hits_{0};
    usize misses_{0};
    usize expiries_{0};
    usize resolver_calls_{0};
};

std::chrono::steady_clock::time_point default_dns_time_source() noexcept
{
    return std::chrono::steady_clock::now();
}

CachedDnsResolver::CachedDnsResolver() noexcept = default;

io::IoResult<CachedDnsResolver> CachedDnsResolver::create(const CachedDnsResolverOptions& options)
{
    return create(default_resolver(), options, default_dns_time_source);
}

io::IoResult<CachedDnsResolver> CachedDnsResolver::create(DnsResolveFn                    resolver,
                                                          const CachedDnsResolverOptions& options,
                                                          DnsTimeSource                   now)
{
    if (options.ttl.count() <= 0 || options.capacity == 0)
        return ca::core::Err(io::IoError::from_kind(
            io::IoErrorKind::InvalidInput, "DNS cache ttl and capacity must be greater than zero"));
    if (!resolver)
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "DNS cache resolver must not be empty"));
    if (!now)
        return ca::core::Err(io::IoError::from_kind(io::IoErrorKind::InvalidInput,
                                                    "DNS cache time source must not be empty"));
    CachedDnsResolver created;
    created.impl_ =
        std::make_unique<Impl>(std::move(resolver), std::move(now), options.ttl, options.capacity);
    return ca::core::Ok(std::move(created));
}

CachedDnsResolver::CachedDnsResolver(CachedDnsResolver&& other) noexcept = default;

CachedDnsResolver& CachedDnsResolver::operator=(CachedDnsResolver&& other) noexcept = default;

CachedDnsResolver::~CachedDnsResolver() = default;

io::IoResult<std::vector<SocketAddress>> CachedDnsResolver::resolve(const std::string& host,
                                                                    u16 port, AddressFamily family,
                                                                    SocketKind kind)
{
    if (impl_ == nullptr)
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::InvalidInput, "DNS cache is not initialized"));
    return impl_->resolve(host, port, family, kind);
}

DnsCacheStats CachedDnsResolver::stats() const
{
    if (impl_ == nullptr)
        return DnsCacheStats{};
    return impl_->stats();
}

}   // namespace ca::net
