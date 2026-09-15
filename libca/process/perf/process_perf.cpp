// process 模块性能基准（单进程本机往返）：
//   - ShmRingQueue：共享内存环形槽收发 op/s
//   - ShmRingQueue + 每次发送强制刷新心跳：心跳开销对比
//   - MessageQueue：内核对象（Windows mailslot / POSIX mq）收发 op/s，作参照
// 构建：xmake build -P . libca_process_perf
// 运行：./build/windows/x64/release/libca_process_perf.exe（或 xmake run -P . libca_process_perf）

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "libca/process/ipc.hpp"
#include "libca/str/format.hpp"
#include "libca/time/stopwatch.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <unistd.h>
#endif

namespace ipc = ca::process::ipc;

namespace {

constexpr ca::usize kRoundCount        = 7;     // 测量轮数：取中位数抑制抖动
constexpr ca::usize kWarmupOps         = 2000;  // 热身：填充缓存、触发惰性初始化
constexpr ca::usize kRingOpsPerRound   = 20000;
constexpr ca::usize kKernelOpsPerRound = 5000;

ca::u64 current_process_id()
{
#if defined(_WIN32)
    return static_cast<ca::u64>(GetCurrentProcessId());
#else
    return static_cast<ca::u64>(getpid());
#endif
}

// 单轮计时：执行 ops 次操作，返回总纳秒。
template<typename Op>
ca::i64 measure_round(ca::usize ops, Op op)
{
    ca::time::Stopwatch watch;
    for (ca::usize index = 0; index < ops; ++index)
        op();
    return watch.elapsed().nanoseconds();
}

// 热身 + 多轮测量 + 中位数。
template<typename Op>
ca::i64 median_round_nanos(ca::usize ops, ca::usize rounds, Op op)
{
    for (ca::usize index = 0; index < kWarmupOps; ++index)
        op();
    std::vector<ca::i64> samples;
    samples.reserve(rounds);
    for (ca::usize round = 0; round < rounds; ++round)
        samples.push_back(measure_round(ops, op));
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

void report(const std::string& title, ca::usize ops, ca::i64 median_nanos)
{
    const double per_op         = static_cast<double>(median_nanos) / static_cast<double>(ops);
    const double ops_per_second = 1000000000.0 / per_op;
    std::fputs(ca::str::format_std("{:<46} {:>6} ops/轮  中位数 {:>9.0f} ns/op  {:>14.0f} op/s\n",
                                   title,
                                   ops,
                                   per_op,
                                   ops_per_second)
                   .c_str(),
               stdout);
}

// ShmRingQueue 单进程收发往返（环形槽本机 ping-pong）。with_per_op_heartbeat 为 true
// 时把心跳间隔压到 0：每次 send 都强制刷新心跳，用于衡量心跳写入的开销。
void bench_shm_ring(bool with_per_op_heartbeat)
{
    const std::string name = "libca_perf_ring_" + std::to_string(current_process_id());
    ipc::ShmRingQueue::Options options{};
    options.max_message_size = 256;
    options.slot_count       = 64;
    if (with_per_op_heartbeat)
        options.heartbeat_interval = std::chrono::milliseconds(0);

    auto created = ipc::ShmRingQueue::create(name, options);
    if (created.is_err()) {
        std::fprintf(stderr, "ShmRingQueue create failed: %s\n",
                     created.unwrap_err().to_string().c_str());
        std::exit(1);
    }
    auto queue = std::move(created).unwrap();

    const std::string payload(64, 'x');
    ca::usize sink = 0;   // 吞掉接收结果，防止编译器把收发优化掉
    const ca::i64 median = median_round_nanos(kRingOpsPerRound, kRoundCount, [&queue, &payload, &sink]() {
        (void)queue.send(payload);
        auto received = queue.receive_for(std::chrono::seconds(1));
        if (received.is_ok() && received.unwrap().has_value())
            sink += received.unwrap()->size();
    });
    const std::string title = with_per_op_heartbeat
                                  ? "ShmRingQueue 64B 往返 + 每次发送刷新心跳"
                                  : "ShmRingQueue 64B 往返";
    report(title, kRingOpsPerRound, median);
    if (sink == 0)
        std::fputs("(warn: 未收到任何消息，结果无效)\n", stdout);

    queue.close();
    (void)ipc::remove_shared_memory(name);
}

// MessageQueue（Windows mailslot / POSIX mq）单进程收发往返，作内核路径参照。
void bench_kernel_message_queue()
{
    const std::string name = "libca_perf_mq_" + std::to_string(current_process_id());
    auto created = ipc::MessageQueue::create(name, 256);
    if (created.is_err()) {
        std::fprintf(stderr, "MessageQueue create failed: %s\n",
                     created.unwrap_err().to_string().c_str());
        std::exit(1);
    }
    auto receiver = std::move(created).unwrap();
    auto opened   = ipc::MessageQueue::open(name);
    if (opened.is_err()) {
        std::fprintf(stderr, "MessageQueue open failed: %s\n",
                     opened.unwrap_err().to_string().c_str());
        std::exit(1);
    }
    auto sender = std::move(opened).unwrap();

    const std::string payload(64, 'x');
    ca::usize sink = 0;
    const ca::i64 median = median_round_nanos(kKernelOpsPerRound, kRoundCount,
                                          [&sender, &receiver, &payload, &sink]() {
                                              (void)sender.send(payload);
                                              auto received = receiver.receive_for(std::chrono::seconds(1));
                                              if (received.is_ok() && received.unwrap().has_value())
                                                  sink += received.unwrap()->size();
                                          });
    report("MessageQueue(内核) 64B 往返（参照）", kKernelOpsPerRound, median);
    if (sink == 0)
        std::fputs("(warn: 未收到任何消息，结果无效)\n", stdout);

    receiver.close();
    sender.close();
    (void)ipc::remove_message_queue(name);
}

}   // namespace

int main()
{
    std::fputs("libca process 模块性能基准（单进程本机往返，7 轮取中位数）\n", stdout);
    bench_shm_ring(false);
    bench_shm_ring(true);
    bench_kernel_message_queue();
    return 0;
}
