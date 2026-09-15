#include <gmock/gmock.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "libca/net/dns_cache.hpp"

namespace ca::net::test {
namespace {

using namespace std::chrono_literals;

/// @brief 测试用地址：127.0.0.1:<port>。
SocketAddress test_address(u16 port)
{
    return SocketAddress(IpAddress::localhost_v4(), port);
}

/// @brief 计数 resolver：原子统计底层调用次数，返回固定地址列表。
class CountingResolver
{
public:
    explicit CountingResolver(std::vector<SocketAddress> addresses)
        : addresses_(std::move(addresses))
    {
        fn_ = [this](const std::string&, u16, AddressFamily, SocketKind) {
            calls_.fetch_add(1, std::memory_order_relaxed);
            return ca::core::Ok(addresses_);
        };
    }

    DnsResolveFn fn() const { return fn_; }
    usize        calls() const { return calls_.load(std::memory_order_relaxed); }

private:
    std::vector<SocketAddress> addresses_;
    DnsResolveFn               fn_;
    std::atomic<usize>         calls_{0};
};

/// @brief 可手动推进的假时钟；返回被捕获时刻的引用供测试推进。
DnsTimeSource fake_clock(std::chrono::steady_clock::time_point& current)
{
    return [&current] { return current; };
}

TEST(DnsCacheTest, HitServesCacheWithoutTouchingUnderlyingResolver)
{
    CountingResolver counting(std::vector<SocketAddress>{test_address(80)});
    auto             now          = std::chrono::steady_clock::now();
    auto             cache_result = CachedDnsResolver::create(
        counting.fn(), CachedDnsResolverOptions{10min, 8}, fake_clock(now));
    ASSERT_TRUE(cache_result.is_ok()) << cache_result.unwrap_err().to_string();
    auto cache = std::move(cache_result).unwrap();

    auto first = cache.resolve("example.com", 80);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(first.unwrap(), std::vector<SocketAddress>{test_address(80)});

    for (int i = 0; i < 5; ++i) {
        auto cached = cache.resolve("example.com", 80);
        ASSERT_TRUE(cached.is_ok());
        EXPECT_EQ(cached.unwrap(), std::vector<SocketAddress>{test_address(80)});
    }

    EXPECT_EQ(counting.calls(), static_cast<usize>(1));
    const auto stats = cache.stats();
    EXPECT_EQ(stats.hits, static_cast<usize>(5));
    EXPECT_EQ(stats.misses, static_cast<usize>(1));
    EXPECT_EQ(stats.resolver_calls, static_cast<usize>(1));
}

TEST(DnsCacheTest, DifferentKeysDoNotShareEntries)
{
    CountingResolver counting(std::vector<SocketAddress>{test_address(80)});
    auto             now          = std::chrono::steady_clock::now();
    auto             cache_result = CachedDnsResolver::create(
        counting.fn(), CachedDnsResolverOptions{10min, 8}, fake_clock(now));
    ASSERT_TRUE(cache_result.is_ok());
    auto cache = std::move(cache_result).unwrap();

    EXPECT_TRUE(cache.resolve("a.test", 80).is_ok());
    EXPECT_TRUE(cache.resolve("a.test", 81).is_ok());                        // 端口不同
    EXPECT_TRUE(cache.resolve("a.test", 80, AddressFamily::Ipv4).is_ok());   // 族不同
    EXPECT_TRUE(cache.resolve("a.test", 80, AddressFamily::Ipv4, SocketKind::Datagram)
                    .is_ok());   // 类型不同
    EXPECT_EQ(counting.calls(), static_cast<usize>(4));

    // 再次解析四元组完全相同的键才命中。
    EXPECT_TRUE(cache.resolve("a.test", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(4));
}

TEST(DnsCacheTest, ExpiredEntryIsResolvedAgain)
{
    CountingResolver counting(std::vector<SocketAddress>{test_address(80)});
    auto             now          = std::chrono::steady_clock::now();
    auto             cache_result = CachedDnsResolver::create(
        counting.fn(), CachedDnsResolverOptions{100ms, 8}, fake_clock(now));
    ASSERT_TRUE(cache_result.is_ok());
    auto cache = std::move(cache_result).unwrap();

    EXPECT_TRUE(cache.resolve("example.com", 80).is_ok());
    EXPECT_TRUE(cache.resolve("example.com", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(1));

    // 假时钟推进超过 TTL 后重新解析，不真实睡眠。
    now += 101ms;
    EXPECT_TRUE(cache.resolve("example.com", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(2));

    const auto stats = cache.stats();
    EXPECT_EQ(stats.expiries, static_cast<usize>(1));
    EXPECT_EQ(stats.hits, static_cast<usize>(1));
}

TEST(DnsCacheTest, CapacityFullEvictsLeastRecentlyUsed)
{
    CountingResolver counting(std::vector<SocketAddress>{test_address(80)});
    auto             now          = std::chrono::steady_clock::now();
    auto             cache_result = CachedDnsResolver::create(
        counting.fn(), CachedDnsResolverOptions{10min, 2}, fake_clock(now));
    ASSERT_TRUE(cache_result.is_ok());
    auto cache = std::move(cache_result).unwrap();

    EXPECT_TRUE(cache.resolve("a.test", 80).is_ok());
    EXPECT_TRUE(cache.resolve("b.test", 80).is_ok());
    // 访问 a，把 b 变成 LRU。
    EXPECT_TRUE(cache.resolve("a.test", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(2));

    // 插入 c 触发淘汰，被逐出的是 b 而不是 a。
    EXPECT_TRUE(cache.resolve("c.test", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(3));
    EXPECT_TRUE(cache.resolve("a.test", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(3));
    EXPECT_TRUE(cache.resolve("b.test", 80).is_ok());
    EXPECT_EQ(counting.calls(), static_cast<usize>(4));
    // b 插入时淘汰 c（此时容量又满），加上之前淘汰的 b 共两次。
    EXPECT_EQ(cache.stats().evictions, static_cast<usize>(2));
}

TEST(DnsCacheTest, FailureIsNotCachedAsNegativeEntry)
{
    int attempts = 0;
    // Ok/Err 包装类型不同，lambda 必须显式声明统一返回类型（GCC 教训）。
    DnsResolveFn flaky = [&attempts](const std::string&,
                                     u16,
                                     AddressFamily,
                                     SocketKind) -> io::IoResult<std::vector<SocketAddress>> {
        attempts += 1;
        if (attempts == 1)
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::NotFound, "transient DNS failure"));
        return ca::core::Ok(std::vector<SocketAddress>{test_address(80)});
    };
    auto now = std::chrono::steady_clock::now();
    auto cache_result =
        CachedDnsResolver::create(flaky, CachedDnsResolverOptions{10min, 8}, fake_clock(now));
    ASSERT_TRUE(cache_result.is_ok());
    auto cache = std::move(cache_result).unwrap();

    auto failed = cache.resolve("flaky.test", 80);
    ASSERT_TRUE(failed.is_err());
    auto recovered = cache.resolve("flaky.test", 80);
    ASSERT_TRUE(recovered.is_ok()) << recovered.unwrap_err().to_string();

    // 两次都穿透到底层：失败结果未占据缓存槽位。
    EXPECT_EQ(attempts, 2);
    EXPECT_EQ(cache.stats().resolver_calls, static_cast<usize>(2));
}

TEST(DnsCacheTest, ConcurrentResolvesStayBoundedAndCorrect)
{
    CountingResolver counting(std::vector<SocketAddress>{test_address(80)});
    auto             cache_result =
        CachedDnsResolver::create(counting.fn(), CachedDnsResolverOptions{10min, 8});
    ASSERT_TRUE(cache_result.is_ok());
    auto cache = std::make_shared<CachedDnsResolver>(std::move(cache_result).unwrap());

    constexpr int threads    = 4;
    constexpr int per_thread = 250;
    auto          worker     = [cache, per_thread](int seed) {
        for (int i = 0; i < per_thread; ++i) {
            const std::string host   = "host" + std::to_string((i + seed) % 8) + ".test";
            auto              result = cache->resolve(host, 80);
            if (result.is_err())
                return false;
        }
        return true;
    };

    // 总截止 5 秒：并发出现死锁或异常时不能挂死测试进程。
    auto fut = std::async(std::launch::async, [worker, threads] {
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t)
            workers.emplace_back(worker, t);
        for (auto& t : workers)
            t.join();
        return true;
    });
    ASSERT_EQ(fut.wait_for(5s), std::future_status::ready);
    EXPECT_TRUE(fut.get());

    EXPECT_GE(counting.calls(), static_cast<usize>(8));   // 每个键至少解析一次
    EXPECT_LE(counting.calls(), static_cast<usize>(threads * per_thread));
    const auto stats = cache->stats();
    EXPECT_EQ(stats.hits + stats.misses, static_cast<usize>(threads * per_thread));
}

TEST(DnsCacheTest, InvalidOptionsAndEmptyHostAreRejected)
{
    auto zero_ttl = CachedDnsResolver::create(CachedDnsResolverOptions{0ms, 8});
    ASSERT_TRUE(zero_ttl.is_err());
    EXPECT_EQ(zero_ttl.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    auto zero_capacity = CachedDnsResolver::create(CachedDnsResolverOptions{10min, 0});
    ASSERT_TRUE(zero_capacity.is_err());
    EXPECT_EQ(zero_capacity.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    auto empty_resolver = CachedDnsResolver::create(DnsResolveFn{});
    ASSERT_TRUE(empty_resolver.is_err());
    EXPECT_EQ(empty_resolver.unwrap_err().kind(), io::IoErrorKind::InvalidInput);

    CountingResolver counting(std::vector<SocketAddress>{test_address(80)});
    auto             now = std::chrono::steady_clock::now();
    auto             empty_clock =
        CachedDnsResolver::create(counting.fn(), CachedDnsResolverOptions{}, DnsTimeSource{});
    ASSERT_TRUE(empty_clock.is_err());

    auto cache_result = CachedDnsResolver::create(
        counting.fn(), CachedDnsResolverOptions{10min, 8}, fake_clock(now));
    ASSERT_TRUE(cache_result.is_ok());
    auto cache = std::move(cache_result).unwrap();

    auto empty_host = cache.resolve("", 80);
    ASSERT_TRUE(empty_host.is_err());
    EXPECT_EQ(empty_host.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
    EXPECT_EQ(counting.calls(), static_cast<usize>(0));
}

TEST(DnsCacheTest, DefaultCreationResolvesLocalhost)
{
    // 默认 resolver + 默认时钟走真实系统路径；localhost 不依赖外网。
    auto cache_result = CachedDnsResolver::create();
    ASSERT_TRUE(cache_result.is_ok()) << cache_result.unwrap_err().to_string();
    auto cache = std::move(cache_result).unwrap();

    auto first = cache.resolve("localhost", 8080, AddressFamily::Ipv4);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_FALSE(first.unwrap().empty());
    auto second = cache.resolve("localhost", 8080, AddressFamily::Ipv4);
    ASSERT_TRUE(second.is_ok());
    EXPECT_EQ(second.unwrap(), first.unwrap());
    EXPECT_EQ(cache.stats().hits, static_cast<usize>(1));
}

TEST(DnsCacheTest, ExternalDnsSmokeIsSkippedOffline)
{
    // 外网 DNS smoke：失败视为环境无外网并跳过，不作为硬依赖断言。
    auto resolved = DnsResolver::resolve("example.com", 80);
    if (resolved.is_err())
        GTEST_SKIP() << "external DNS unavailable: " << resolved.unwrap_err().to_string();
    ASSERT_FALSE(resolved.unwrap().empty());

    auto cache_result = CachedDnsResolver::create(CachedDnsResolverOptions{10min, 8});
    ASSERT_TRUE(cache_result.is_ok());
    auto cache  = std::move(cache_result).unwrap();
    auto cached = cache.resolve("example.com", 80);
    ASSERT_TRUE(cached.is_ok()) << cached.unwrap_err().to_string();
    auto direct = DnsResolver::resolve("example.com", 80);
    ASSERT_TRUE(direct.is_ok()) << direct.unwrap_err().to_string();
    // getaddrinfo 对多地址主机两次解析可能返回不同顺序（DNS 轮询 + RFC 6724 排序），
    // 只比较地址集合是否一致，不比较顺序。
    std::vector<std::string> cached_texts;
    std::vector<std::string> direct_texts;
    for (const auto& address : cached.unwrap())
        cached_texts.push_back(address.to_string());
    for (const auto& address : direct.unwrap())
        direct_texts.push_back(address.to_string());
    std::sort(cached_texts.begin(), cached_texts.end());
    std::sort(direct_texts.begin(), direct_texts.end());
    EXPECT_EQ(cached_texts, direct_texts);
}

}   // namespace
}   // namespace ca::net::test
