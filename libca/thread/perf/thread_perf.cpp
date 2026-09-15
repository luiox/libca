// MessageLoop 性能基准：post_task 空任务吞吐 / post_task_with_result 往返延迟。
//
// 方法：Stopwatch 计时 + 热身轮 + 多轮测量取中位数；每轮都用原子计数 / 校验和确认
// 任务真实执行（防空转）。输出 op/s，退出码非 0 表示校验失败。

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "libca/thread/message_loop.hpp"
#include "libca/time/stopwatch.hpp"

using namespace std::chrono_literals;

namespace {

// 基准参数：热身 2 轮不计入统计，测量 5 轮取中位数。
constexpr ca::u64 kWarmupRounds        = 2;
constexpr ca::u64 kMeasuredRounds      = 5;
constexpr ca::u64 kPostOpsPerRound     = 200000;
constexpr ca::u64 kRoundTripOpsPerRound = 50000;

// 多轮测量的中位数：样本数为奇数（5 轮），直接取排序中点。
double median_of(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values.empty() ? 0.0 : values[values.size() / 2];
}

// 等待已执行计数达到目标；超时返回 false（视为防空转校验失败）。
bool wait_executed(const std::atomic<ca::u64>& executed, ca::u64 target)
{
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (executed.load(std::memory_order_relaxed) < target) {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

struct BenchOutcome
{
    double median_ops_per_sec{0.0};
    bool   validated{false};
};

// post_task 空任务吞吐：只计投递 N 个任务的耗时，队列排空等待不计入测量窗口。
BenchOutcome bench_post_throughput(ca::thread::MessageLoop& loop)
{
    std::atomic<ca::u64> executed{0};
    std::vector<double>  samples;
    ca::u64              posted_total = 0;
    bool                 ok           = true;

    auto run_round = [&](bool count_it) {
        ca::time::Stopwatch watch;
        for (ca::u64 i = 0; i < kPostOpsPerRound; ++i)
            loop.post_task([&executed] { executed.fetch_add(1, std::memory_order_relaxed); });
        const double seconds = watch.elapsed().as_seconds_f64();
        posted_total += kPostOpsPerRound;
        if (!wait_executed(executed, posted_total))   // 校验：所有任务确实执行
            ok = false;
        if (count_it && seconds > 0.0)
            samples.push_back(static_cast<double>(kPostOpsPerRound) / seconds);
    };

    for (ca::u64 round = 0; round < kWarmupRounds; ++round)
        run_round(false);
    for (ca::u64 round = 0; round < kMeasuredRounds; ++round)
        run_round(true);

    BenchOutcome outcome;
    outcome.validated = ok && executed.load() == posted_total && !samples.empty();
    if (!samples.empty())
        outcome.median_ops_per_sec = median_of(samples);
    return outcome;
}

// post_task_with_result 往返延迟：每次测量 post + future.get() 完整往返，op/s 越高延迟越低。
BenchOutcome bench_round_trip(ca::thread::MessageLoop& loop)
{
    constexpr ca::u64 kMagicValue = 7;

    std::vector<double> samples;
    bool                ok = true;

    auto run_round = [&](bool count_it) {
        ca::time::Stopwatch watch;
        ca::u64             checksum = 0;
        for (ca::u64 i = 0; i < kRoundTripOpsPerRound; ++i) {
            auto posted = loop.post_task_with_result([kMagicValue] { return kMagicValue; });
            if (posted.is_err()) {
                ok = false;
                continue;
            }
            checksum += static_cast<ca::u64>(std::move(posted).unwrap().get());
        }
        const double seconds = watch.elapsed().as_seconds_f64();
        if (checksum != kRoundTripOpsPerRound * kMagicValue)   // 校验：结果真实产生且正确
            ok = false;
        if (count_it && seconds > 0.0)
            samples.push_back(static_cast<double>(kRoundTripOpsPerRound) / seconds);
    };

    for (ca::u64 round = 0; round < kWarmupRounds; ++round)
        run_round(false);
    for (ca::u64 round = 0; round < kMeasuredRounds; ++round)
        run_round(true);

    BenchOutcome outcome;
    outcome.validated = ok && !samples.empty();
    if (!samples.empty())
        outcome.median_ops_per_sec = median_of(samples);
    return outcome;
}

}   // namespace

int main()
{
    auto created = ca::thread::MessageLoop::create();
    if (created.is_err()) {
        std::printf("message loop create failed: %s\n", created.unwrap_err().message().c_str());
        return 1;
    }
    auto loop = std::move(created).unwrap();

    std::printf("== libca_thread MessageLoop perf: median of %llu rounds after %llu warmup rounds ==\n",
                static_cast<unsigned long long>(kMeasuredRounds),
                static_cast<unsigned long long>(kWarmupRounds));

    const auto post_result = bench_post_throughput(loop);
    std::printf("post_task throughput        : %12.0f op/s   [validated=%s]\n",
                post_result.median_ops_per_sec, post_result.validated ? "ok" : "FAILED");

    const auto round_trip = bench_round_trip(loop);
    std::printf("post_task_with_result rtt   : %12.0f op/s   [validated=%s]\n",
                round_trip.median_ops_per_sec, round_trip.validated ? "ok" : "FAILED");

    loop.stop_and_drain();
    loop.join();

    if (!post_result.validated || !round_trip.validated) {
        std::printf("validation FAILED: counters/checksums did not confirm execution\n");
        return 1;
    }
    std::printf("all validation passed\n");
    return 0;
}
