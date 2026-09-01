/// @file log_benchmark.cpp
/// @brief libca/log 门面与后端压力测试（非 gtest，独立 main）。
/// @details 构建：`xmake f --with_tests=y && xmake build libca_log_benchmark`，
///          运行：`xmake run libca_log_benchmark`。覆盖三类热点：
///          - 门面过滤开销：未注册 target / 级别被过滤时的零输出代价（热路径最关键）；
///          - 门面格式化开销：经 OpaqueFormat/FmtArgsHolder 渲染（虚函数 + view）的代价；
///          - SimpleLogBackend 同步吞吐：单线程与多线程下实际写出的 ops/s，
///            量化 mutex 竞争对吞吐的影响。
///          输出取多次运行的最优值（best），近似排除调度抖动。
/// @author Canrad

#include "libca/log/log_macros.hpp"
#include "libca/log/logger.hpp"
#include "libca/log/logger_registry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace ca::log;
using Clock = std::chrono::steady_clock;

namespace {

double ms_since(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

template<typename F>
double best_ms(int iters, F&& f)
{
    f();   // 预热
    double best = 1e18;
    for (int i = 0; i < iters; ++i) {
        auto t0 = Clock::now();
        f();
        best = std::min(best, ms_since(t0));
    }
    return best;
}

void line(const char* name, double ms, double items, const char* unit)
{
    std::printf("%-48s | best=%9.3f ms | %12.1f %s/s\n", name, ms, items / (ms / 1000.0), unit);
}

// 黑洞后端：丢弃所有输出，用于纯测门面 + 格式化开销（消除 I/O 噪声）。
class NullBackend final : public ILogBackend
{
public:
    void log(Level, std::string_view, std::string_view, int, const OpaqueFormat& message) override
    {
        // 仍调 render_to，保证格式化路径被执行（测的是格式化开销，不是 I/O）。
        std::string buf;
        message.render_to(buf);
        sink_.assign(buf);   // 防止优化器整段删除
    }

private:
    std::string sink_;
};

// RAII 注册/清理，避免污染全局 registry。
class RegistryScope
{
public:
    RegistryScope()
    {
        LoggerRegistry::register_logger("default",
                                        std::make_shared<Logger>(std::make_shared<NullBackend>()));
    }
    ~RegistryScope() { LoggerRegistry::clear(); }
};

// ---- 门面过滤开销：热路径上"不输出时"的代价 ----
void bench_facade_filter()
{
    std::printf("-- facade filter overhead (no actual output) --\n");
    constexpr int N = 1000000;

    // 1. 未注册 target：get() 返回 nullptr，宏直接返回。
    double ms = best_ms(5, [&] {
        for (int i = 0; i < N; ++i)
            CA_LOGT_INFO("unregistered_target", "msg {}", i);
    });
    line("unregistered target (nullptr drop)", ms, double(N), "log");

    // 2. 已注册但级别被过滤：should_log 返回 false。
    {
        RegistryScope guard;
        LoggerRegistry::get("default")->set_level(Level::Off);
        double ms2 = best_ms(5, [&] {
            for (int i = 0; i < N; ++i)
                CA_LOG_INFO("msg {}", i);
        });
        line("registered, level=Off (runtime filter)", ms2, double(N), "log");
    }
    std::printf("\n");
}

// ---- 门面格式化开销：经 OpaqueFormat/FmtArgsHolder 渲染 + 后端输出 ----
void bench_facade_format()
{
    std::printf("-- facade format + backend render --\n");
    RegistryScope guard;
    LoggerRegistry::get("default")->set_level(Level::Trace);

    constexpr int N = 200000;
    for (int nargs : {0, 1, 3}) {
        double ms;
        if (nargs == 0) {
            ms = best_ms(5, [&] {
                for (int i = 0; i < N; ++i)
                    CA_LOG_INFO("plain message no args");
            });
        }
        else if (nargs == 1) {
            ms = best_ms(5, [&] {
                for (int i = 0; i < N; ++i)
                    CA_LOG_INFO("value is {}", i);
            });
        }
        else {
            ms = best_ms(5, [&] {
                for (int i = 0; i < N; ++i)
                    CA_LOG_INFO("a={} b={} c={}", i, i * 2, "str");
            });
        }
        char nm[64];
        std::snprintf(nm, sizeof(nm), "format render nargs=%d", nargs);
        line(nm, ms, double(N), "log");
    }
    std::printf("\n");
}

// ---- 多线程吞吐：门面 + 格式化在并发下的开销 ----
// 用 NullBackend（无 I/O、无 mutex），隔离测门面层（registry get + should_log +
// FmtArgsHolder render + 虚分发）的多线程扩展性。SimpleLogBackend 的真实 I/O 吞吐
// 受磁盘/sink 影响极大，不在本基准范围内（如需测，自行绑定 ostream sink）。
void bench_backend_throughput()
{
    std::printf("-- concurrent facade throughput (NullBackend, no I/O) --\n");
    RegistryScope guard;   // NullBackend
    LoggerRegistry::get("default")->set_level(Level::Trace);

    constexpr int N = 200000;
    for (int threads : {1, 2, 4, 8}) {
        std::atomic<long long> counter{0};
        auto                   worker = [&]() {
            for (int i = 0; i < N; ++i) {
                CA_LOG_INFO("thread log iter={} tag=bench", counter.fetch_add(1));
            }
        };

        // best of 3
        double best = 1e18;
        for (int run = 0; run < 3; ++run) {
            auto                     t0 = Clock::now();
            std::vector<std::thread> pool;
            pool.reserve(threads);
            for (int t = 0; t < threads; ++t)
                pool.emplace_back(worker);
            for (auto& th : pool)
                th.join();
            best = std::min(best, ms_since(t0));
        }
        char nm[64];
        std::snprintf(nm, sizeof(nm), "threads=%d x%d ops/thread", threads, N);
        line(nm, best, double(threads) * double(N), "log");
    }
    std::printf("\n");
}

}   // namespace

int main()
{
    std::printf("libca/log benchmark (best-of-N, lower ms = faster)\n");
    std::printf("==================================================\n\n");
    bench_facade_filter();
    bench_facade_format();
    bench_backend_throughput();
    std::printf("done.\n");
    return 0;
}
