// net SockUtil / DNS 缓存性能基准。
//
// 覆盖：
// - dns_cache 命中 vs 未命中的单次延迟与吞吐（注入计数 resolver）；
// - interface_list 单次枚举延迟；
// - TCP keepalive 参数设置延迟。
//
// 方法：每项先热身，再分多轮测量取单次耗时中位数；用注入计数器与回读校验防止
// 空转（编译器/逻辑把被测操作优化掉时直接判失败）。本程序只输出数据，不做
// gtest 断言；任何校验失败以非零码退出。

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "libca/net/dns_cache.hpp"
#include "libca/net/sock_util.hpp"
#include "libca/net/tcp.hpp"
#include "libca/time/stopwatch.hpp"

namespace {

using ca::usize;
using namespace std::chrono_literals;

constexpr usize kRounds       = 5;   // 每项基准的测量轮数，取中位数
constexpr usize kDnsWarmup    = 2000;
constexpr usize kDnsMeasured  = 20000;
constexpr usize kEnumWarmup   = 5;
constexpr usize kEnumMeasured = 100;
constexpr usize kSockWarmup   = 100;
constexpr usize kSockMeasured = 1000;

/// @brief 计算中位数（就地排序）。
double median(std::vector<double>& values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

/// @brief 热身后测量 fn 执行 measured 次的总耗时（秒）。
template<typename Fn>
double measure_seconds(Fn&& fn, usize warmup, usize measured)
{
    for (usize i = 0; i < warmup; ++i)
        fn(i);
    ca::time::Stopwatch watch;
    for (usize i = 0; i < measured; ++i)
        fn(i);
    return static_cast<double>(watch.elapsed().nanoseconds()) / 1e9;
}

/// @brief 计数 resolver：原子统计调用次数并返回固定地址，供命中/未命中基准使用。
class CountingResolver
{
public:
    CountingResolver()
    {
        resolver_ = [this](
                        const std::string&, ca::u16, ca::net::AddressFamily, ca::net::SocketKind) {
            calls_.fetch_add(1, std::memory_order_relaxed);
            return ca::core::Ok(std::vector<ca::net::SocketAddress>{
                ca::net::SocketAddress(ca::net::IpAddress::v4(93, 184, 216, 34), 80)});
        };
    }

    ca::net::DnsResolveFn fn() const { return resolver_; }
    usize                 calls() const { return calls_.load(std::memory_order_relaxed); }

private:
    std::atomic<usize>    calls_{0};
    ca::net::DnsResolveFn resolver_;
};

/// @brief DNS 缓存命中：测量期间底层调用次数必须零增长。
/// @return 单次操作纳秒数（多轮中位数）；校验失败返回 -1。
double dns_cache_hit_benchmark()
{
    CountingResolver counting;
    auto             cache_result = ca::net::CachedDnsResolver::create(
        counting.fn(), ca::net::CachedDnsResolverOptions{10min, 128});
    if (cache_result.is_err()) {
        std::fprintf(stderr, "dns hit: create cache failed\n");
        return -1.0;
    }
    auto cache = std::move(cache_result).unwrap();

    // 预热缓存条目，之后的测量阶段必须零穿透。
    if (!cache.resolve("perf.hit.test", 80).is_ok()) {
        std::fprintf(stderr, "dns hit: initial resolve failed\n");
        return -1.0;
    }

    std::vector<double> samples;
    samples.reserve(kRounds);
    for (usize round = 0; round < kRounds; ++round) {
        const usize  calls_before = counting.calls();
        const double seconds =
            measure_seconds([&cache](usize) { return cache.resolve("perf.hit.test", 80).is_ok(); },
                            kDnsWarmup,
                            kDnsMeasured);
        if (!cache.resolve("perf.hit.test", 80).is_ok() || counting.calls() != calls_before) {
            std::fprintf(stderr, "dns hit: benchmark did not stay on cache\n");
            return -1.0;
        }
        samples.push_back(seconds * 1e9 / static_cast<double>(kDnsMeasured));
    }
    const double ns = median(samples);
    std::printf("dns_cache hit    : %10.1f ns/op  %12.0f ops/s\n", ns, 1e9 / ns);
    return ns;
}

/// @brief DNS 缓存未命中：每次操作用全新 host，必须全部穿透到底层。
/// @return 单次操作纳秒数（多轮中位数）；校验失败返回 -1。
double dns_cache_miss_benchmark()
{
    CountingResolver counting;
    auto             cache_result = ca::net::CachedDnsResolver::create(
        counting.fn(), ca::net::CachedDnsResolverOptions{10min, 128});
    if (cache_result.is_err()) {
        std::fprintf(stderr, "dns miss: create cache failed\n");
        return -1.0;
    }
    auto cache = std::move(cache_result).unwrap();

    std::vector<double> samples;
    samples.reserve(kRounds);
    for (usize round = 0; round < kRounds; ++round) {
        const usize  calls_before = counting.calls();
        const double seconds      = measure_seconds(
            [&cache](usize i) {
                return cache.resolve("perf.miss." + std::to_string(i) + ".test", 80).is_ok();
            },
            kDnsWarmup,
            kDnsMeasured);
        const usize expected_calls = calls_before + kDnsWarmup + kDnsMeasured;
        if (counting.calls() != expected_calls) {
            std::fprintf(stderr, "dns miss: unexpected resolver calls\n");
            return -1.0;
        }
        samples.push_back(seconds * 1e9 / static_cast<double>(kDnsMeasured));
    }
    const double ns = median(samples);
    std::printf("dns_cache miss   : %10.1f ns/op  %12.0f ops/s\n", ns, 1e9 / ns);
    return ns;
}

/// @brief interface_list 单次枚举延迟；结果必须非空。
/// @return 单次操作纳秒数（多轮中位数）；校验失败返回 -1。
double interface_list_benchmark()
{
    std::vector<double> samples;
    samples.reserve(kRounds);
    for (usize round = 0; round < kRounds; ++round) {
        const double seconds = measure_seconds(
            [](usize) { return ca::net::interface_list().is_ok(); }, kEnumWarmup, kEnumMeasured);
        auto interfaces = ca::net::interface_list();
        if (interfaces.is_err() || interfaces.unwrap().empty()) {
            std::fprintf(stderr, "interface_list: empty or failed result\n");
            return -1.0;
        }
        samples.push_back(seconds * 1e9 / static_cast<double>(kEnumMeasured));
    }
    const double ns = median(samples);
    std::printf("interface_list   : %10.1f ns/op  %12.0f ops/s\n", ns, 1e9 / ns);
    return ns;
}

/// @brief TCP keepalive 参数设置延迟；设置必须生效。
/// @return 单次操作纳秒数（多轮中位数）；校验失败返回 -1。
double keepalive_set_benchmark()
{
    auto listener_result =
        ca::net::TcpListener::bind(ca::net::SocketAddress(ca::net::IpAddress::localhost_v4(), 0));
    if (listener_result.is_err()) {
        std::fprintf(stderr, "keepalive: bind listener failed\n");
        return -1.0;
    }
    auto listener      = std::move(listener_result).unwrap();
    auto client_result = ca::net::TcpStream::connect(listener.local_address().unwrap());
    if (client_result.is_err()) {
        std::fprintf(stderr, "keepalive: connect failed\n");
        return -1.0;
    }
    auto client = std::move(client_result).unwrap();
    if (!listener.accept().is_ok()) {
        std::fprintf(stderr, "keepalive: accept failed\n");
        return -1.0;
    }

    ca::net::TcpKeepaliveConfig config;
    config.idle     = 30s;
    config.interval = 5s;
    config.count    = 3;

    std::vector<double> samples;
    samples.reserve(kRounds);
    for (usize round = 0; round < kRounds; ++round) {
        const double seconds = measure_seconds(
            [&client, &config](usize) { return client.set_keepalive(config).is_ok(); },
            kSockWarmup,
            kSockMeasured);
        auto enabled = client.keepalive_enabled();
        if (enabled.is_err() || !enabled.unwrap()) {
            std::fprintf(stderr, "keepalive: setting did not take effect\n");
            return -1.0;
        }
        samples.push_back(seconds * 1e9 / static_cast<double>(kSockMeasured));
    }
    const double ns = median(samples);
    std::printf("keepalive set    : %10.1f ns/op  %12.0f ops/s\n", ns, 1e9 / ns);
    return ns;
}

}   // namespace

int main()
{
    std::printf("libca_net sock perf (median of %zu rounds, warmup + measured ops)\n", kRounds);

    const double dns_hit  = dns_cache_hit_benchmark();
    const double dns_miss = dns_cache_miss_benchmark();
    const double enum_ns  = interface_list_benchmark();
    const double keep_ns  = keepalive_set_benchmark();

    if (dns_hit < 0.0 || dns_miss < 0.0 || enum_ns < 0.0 || keep_ns < 0.0) {
        std::fprintf(stderr, "sock perf: validation failed\n");
        return 1;
    }
    if (dns_hit > dns_miss) {
        // 命中必须显著快于穿透到底层的未命中，否则说明缓存没起作用。
        std::fprintf(stderr, "sock perf: cache hit slower than miss, cache is ineffective\n");
        return 1;
    }
    return 0;
}
