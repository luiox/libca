// libca_net 性能基准：TlsStream 经 OpenSSL memory BIO + 内存双工流自环的
// 明文吞吐（MB/s）与完整 TLS 握手速率（次/秒）。
//
// 用法：先 `xmake f -P . --with_openssl=y`，再
//   xmake build -P . libca_net_perf
//   ./build/windows/x64/release/libca_net_perf.exe   (Linux: build/ 下对应产物)
//
// 说明：不监听任何端口，握手对与数据通路都发生在进程内的两条内存管道上；
// 未启用 OpenSSL 时打印提示后正常退出。

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/net/tls_stream.hpp"

// 测试专用内存双工流与测试证书（net/test 在 perf target 的 include path 上）。
#include "mem_duplex.hpp"

#include "libca/io/error.hpp"
#include "libca/time/stopwatch.hpp"

#if defined(LIBCA_NET_HAS_OPENSSL)

#    include <filesystem>
#    include <future>
#    include <utility>

namespace {

using ca::u64;
using ca::u8;
using ca::usize;
using ca::io::IoResult;
using ca::net::TlsClientOptions;
using ca::net::TlsHandshakeControl;
using ca::net::TlsServerContext;
using ca::net::TlsServerOptions;
using ca::net::TlsStream;
using ca::net::test::MemDuplex;
using ca::time::Stopwatch;

constexpr usize kPayloadSize  = 8U * 1024U * 1024U;   // 吞吐单轮数据量：8 MiB
constexpr usize kDrainChunk   = 16U * 1024U;
constexpr int   kWarmupRounds = 2;
constexpr int   kMeasureRounds = 5;
constexpr int   kHandshakeWarmup = 5;
constexpr int   kHandshakesPerRound = 60;
constexpr int   kHandshakeRounds = 3;

std::string asset(const char* name)
{
    const std::filesystem::path dir("libca/net/test");
    return (dir / name).string();
}

u64 fnv1a(const u8* data, usize size)
{
    u64 hash = 14695981039346656037ULL;
    for (usize index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

TlsServerContext make_server_context()
{
    TlsServerOptions options;
    options.certificate_chain_file = asset("tls_test_server_cert.pem");
    options.private_key_file       = asset("tls_test_server_key.pem");
    TlsServerContext               context;
    auto                           loaded = context.load(options);
    if (loaded.is_err()) {
        std::fprintf(stderr, "load server TLS context failed: %s\n",
                     loaded.unwrap_err().to_string().c_str());
        std::exit(1);
    }
    return context;
}

TlsClientOptions make_client_options()
{
    TlsClientOptions options;
    options.ca_file = asset("tls_test_ca.pem");
    return options;
}

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// 非阻塞自环上一轮完整吞吐：client 写满 8 MiB，server 排空并累计 FNV-1a 校验和。
// 管道写永不阻塞，同一线程先写后读即可驱动两侧，测量不含线程切换噪声。
double throughput_round(TlsStream& client, TlsStream& server, const std::vector<u8>& payload,
                        u64* checksum_out)
{
    u64  checksum = 14695981039346656037ULL;   // FNV-1a offset basis
    usize drained = 0;
    u8   buffer[kDrainChunk];

    Stopwatch watch;
    auto written = client.write_all(payload.data(), payload.size());
    if (written.is_err()) {
        std::fprintf(stderr, "throughput write failed: %s\n",
                     written.unwrap_err().to_string().c_str());
        std::exit(1);
    }
    auto flushed = client.flush();
    if (flushed.is_err()) {
        std::fprintf(stderr, "throughput flush failed: %s\n",
                     flushed.unwrap_err().to_string().c_str());
        std::exit(1);
    }
    while (drained < payload.size()) {
        auto count = server.read(buffer, sizeof(buffer));
        if (count.is_err()) {
            std::fprintf(stderr, "throughput read failed: %s\n",
                         count.unwrap_err().to_string().c_str());
            std::exit(1);
        }
        if (count.unwrap() == 0) {
            std::fprintf(stderr, "throughput read hit unexpected EOF at %zu/%zu\n", drained,
                         payload.size());
            std::exit(1);
        }
        for (usize index = 0; index < count.unwrap(); ++index) {
            checksum ^= buffer[index];
            checksum *= 1099511628211ULL;
        }
        drained += count.unwrap();
    }
    const double seconds = static_cast<double>(watch.elapsed().nanoseconds()) * 1e-9;
    *checksum_out        = checksum;
    return static_cast<double>(payload.size()) / seconds / (1024.0 * 1024.0);   // MiB/s
}

// server 线程：从队列取待握手的内存链路，accept 后把结果交回。
struct HandshakeJob
{
    MemDuplex* link = nullptr;
    std::promise<IoResult<TlsStream>> done;
};

class HandshakeServer
{
public:
    explicit HandshakeServer(const TlsServerContext& context)
        : context_(context)
        , thread_([this] { serve(); })
    {}

    ~HandshakeServer()
    {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            stopped_ = true;
        }
        ready_.notify_all();
        thread_.join();
    }

    std::future<IoResult<TlsStream>> submit(MemDuplex& link)
    {
        auto job   = std::make_unique<HandshakeJob>();
        job->link  = &link;
        auto future = job->done.get_future();
        {
            std::lock_guard<std::mutex> guard(mutex_);
            queue_.push_back(std::move(job));
        }
        ready_.notify_all();
        return future;
    }

private:
    void serve()
    {
        for (;;) {
            std::unique_ptr<HandshakeJob> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
                if (queue_.empty())
                    return;
                job = std::move(queue_.front());
                queue_.pop_front();
            }
            job->done.set_value(
                TlsStream::accept(context_, job->link->b(), job->link->b()));
        }
    }

    const TlsServerContext&            context_;
    std::mutex                         mutex_;
    std::condition_variable            ready_;
    std::deque<std::unique_ptr<HandshakeJob>> queue_;
    bool                               stopped_{false};
    std::thread                        thread_;
};

// 一轮完整握手速率：client 逐个 connect，server 线程并发 accept。
double handshake_round(HandshakeServer& server, const std::string& host,
                       const TlsClientOptions& options)
{
    Stopwatch watch;
    for (int index = 0; index < kHandshakesPerRound; ++index) {
        MemDuplex link;
        auto      server_ready = server.submit(link);
        auto      client       = TlsStream::connect(link.a(), link.a(), host, options);
        if (client.is_err()) {
            std::fprintf(stderr, "handshake connect failed: %s\n",
                         client.unwrap_err().to_string().c_str());
            std::exit(1);
        }
        auto accepted = server_ready.get();
        if (accepted.is_err()) {
            std::fprintf(stderr, "handshake accept failed: %s\n",
                         accepted.unwrap_err().to_string().c_str());
            std::exit(1);
        }
    }
    const double seconds = static_cast<double>(watch.elapsed().nanoseconds()) * 1e-9;
    return static_cast<double>(kHandshakesPerRound) / seconds;
}

}   // namespace

int main()
{
    auto server_context = make_server_context();
    auto client_options = make_client_options();

    // server accept 在独立线程进行；阻塞内存流要求两侧并发驱动（与真实 socket 一致）。
    HandshakeServer server(server_context);

    // ---- mem-BIO 自环明文吞吐 ----
    std::vector<u8> payload(kPayloadSize);
    for (usize index = 0; index < payload.size(); ++index)
        payload[index] = static_cast<u8>(index * 31U + (index >> 9));
    const u64 expected_checksum = fnv1a(payload.data(), payload.size());

    {
        MemDuplex link;
        auto      server_ready = server.submit(link);
        auto      secured      = TlsStream::connect(link.a(), link.a(), "localhost", client_options);
        if (secured.is_err()) {
            std::fprintf(stderr, "throughput handshake failed: %s\n",
                         secured.unwrap_err().to_string().c_str());
            return 1;
        }
        auto accepted = std::move(server_ready).get();
        if (accepted.is_err()) {
            std::fprintf(stderr, "throughput accept failed: %s\n",
                         accepted.unwrap_err().to_string().c_str());
            return 1;
        }
        auto client_stream = std::move(secured).unwrap();
        auto server_stream = std::move(accepted).unwrap();

        u64  checksum = 0;
        for (int round = 0; round < kWarmupRounds; ++round)
            throughput_round(client_stream, server_stream, payload, &checksum);

        std::vector<double> results;
        for (int round = 0; round < kMeasureRounds; ++round)
            results.push_back(throughput_round(client_stream, server_stream, payload, &checksum));
        if (checksum != expected_checksum) {
            std::fprintf(stderr, "checksum mismatch: got %llu expected %llu\n",
                         static_cast<unsigned long long>(checksum),
                         static_cast<unsigned long long>(expected_checksum));
            return 1;
        }
        std::printf("tls_memloop_throughput_mb_s = %.1f (median of %d rounds, %d MiB per round)\n",
                    median(results), kMeasureRounds, static_cast<int>(kPayloadSize / (1024U * 1024U)));
        std::printf("tls_memloop_checksum      = %llu\n",
                    static_cast<unsigned long long>(checksum));
    }

    // ---- 完整握手速率 ----
    for (int round = 0; round < kHandshakeWarmup; ++round) {
        MemDuplex link;
        auto      server_ready = server.submit(link);
        auto      client       = TlsStream::connect(link.a(), link.a(), "localhost", client_options);
        (void)std::move(server_ready).get();
        if (client.is_err()) {
            std::fprintf(stderr, "handshake warmup failed\n");
            return 1;
        }
    }

    {
        std::vector<double> results;
        for (int round = 0; round < kHandshakeRounds; ++round)
            results.push_back(handshake_round(server, "localhost", client_options));
        std::printf("tls_handshake_per_s       = %.1f (median of %d rounds, %d handshakes per round)\n",
                    median(results), kHandshakeRounds, kHandshakesPerRound);
    }

    return 0;
}

#else

int main()
{
    std::printf("libca_net_perf requires a build with OpenSSL enabled "
                "(xmake f -P . --with_openssl=y)\n");
    return 0;
}

#endif
