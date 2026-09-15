// crypto_perf.cpp：libca_crypto AES 后端加密吞吐基准（手动运行，不进测试列表）。
// 指标：AES-256-CTR / AES-256-CBC 加密吞吐（MB/s），Builtin 与可用外部后端同表输出。
// 方法：Stopwatch 计时；每项先热身 2 轮再测 5 轮取中位数；每轮结果按字节累加
//       checksum 并打印，防止编译器把加密当作死代码消除。
// 运行：xmake build -P . libca_crypto_perf && xmake run -P . libca_crypto_perf

#include "libca/crypto/aes.hpp"
#include "libca/crypto/crypto_util.hpp"
#include "libca/core/bytes.hpp"
#include "libca/time/stopwatch.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace ca;
using namespace ca::core;
using namespace ca::crypto;
using namespace ca::time;

namespace {

// Bytes -> 只读视图。
ByteSlice as_view(const Bytes& data) noexcept
{
    return ByteSlice(data.as_ptr(), data.len());
}

// 数据规模与轮数：4 MiB 单轮，兼顾稳定性与总时长。
constexpr usize kDataSize = 4u * 1024u * 1024u;
constexpr usize kWarmupRounds = 2;
constexpr usize kMeasureRounds = 5;

// 确定性伪随机源（splitmix64，固定种子），保证各后端输入完全一致。
class DetRng
{
public:
    explicit DetRng(u64 seed) noexcept : state_(seed + 0x9e3779b97f4a7c15ULL) {}

    u64 next() noexcept
    {
        state_ += 0x9e3779b97f4a7c15ULL;
        u64 z = state_;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

private:
    u64 state_;
};

// 用 DetRng 填充 len 字节缓冲。
Bytes deterministic_bytes(DetRng& rng, usize len)
{
    BytesMut buffer = BytesMut::with_capacity(len);
    for (usize i = 0; i < len; ++i)
        buffer.put_u8(static_cast<u8>(rng.next() & 0xff));
    return buffer.freeze();
}

// 把 result 的字节折叠进 checksum（FNV-1a 语义），返回是否成功。
bool fold_checksum(const Result<Bytes, CryptoError>& result, u64& checksum) noexcept
{
    if (!result.is_ok())
        return false;
    const Bytes data = result.unwrap();
    for (usize i = 0; i < data.len(); ++i) {
        checksum ^= data.as_ptr()[i];
        checksum *= 0x100000001b3ULL;
    }
    return true;
}

enum class BenchMode
{
    Ctr,
    Cbc,
};

// 测单项吞吐：返回中位数 MB/s；加密失败返回负数。
double bench_encrypt(BenchMode mode, AesBackend backend, ByteSlice key, ByteSlice iv,
                     ByteSlice data, u64& checksum)
{
    auto run_once = [&]() {
        if (mode == BenchMode::Ctr)
            return aes_ctr_crypt(key, iv, data, backend);
        return aes_cbc_encrypt(key, iv, data, backend);
    };

    // 热身。
    for (usize i = 0; i < kWarmupRounds; ++i) {
        if (!fold_checksum(run_once(), checksum))
            return -1.0;
    }

    // 正式多轮计时，取中位数。
    std::vector<i64> rounds;
    rounds.reserve(kMeasureRounds);
    for (usize i = 0; i < kMeasureRounds; ++i) {
        Stopwatch watch;
        const bool ok = fold_checksum(run_once(), checksum);
        const i64 nanos = watch.elapsed().nanoseconds();
        if (!ok)
            return -1.0;
        rounds.push_back(nanos);
    }

    std::sort(rounds.begin(), rounds.end());
    const i64 median = rounds[rounds.size() / 2];
    return static_cast<double>(data.size()) * 1e9 / static_cast<double>(median) / 1e6;
}

const char* backend_name(AesBackend backend) noexcept
{
    switch (backend) {
    case AesBackend::Builtin: return "Builtin";
    case AesBackend::OpenSsl: return "OpenSsl";
    case AesBackend::Cng: return "Cng";
    case AesBackend::Auto: return "Auto";
    }
    return "unknown";
}

}  // namespace

int main()
{
    std::printf("=== libca_crypto_perf: AES-256 encryption throughput ===\n");
    std::printf("data: %zu bytes/round, warmup %zu rounds, median of %zu rounds\n\n",
                static_cast<size_t>(kDataSize), static_cast<size_t>(kWarmupRounds),
                static_cast<size_t>(kMeasureRounds));
    std::printf("%-10s %12s %12s\n", "backend", "CTR MB/s", "CBC MB/s");

    // 固定种子的密钥/IV/数据：所有后端输入完全一致，密文可跨后端比对。
    DetRng rng(0x4145533150455246ULL);  // "AES1PERF"
    const Bytes key = deterministic_bytes(rng, 32);
    const Bytes iv = deterministic_bytes(rng, AES_BLOCK_SIZE);
    const Bytes data = deterministic_bytes(rng, kDataSize);

    const AesBackend backends[] = {AesBackend::Builtin, AesBackend::OpenSsl, AesBackend::Cng};
    u64 checksum = 0xcbf29ce484222325ULL;

    for (const auto backend : backends) {
        if (!aes_backend_available(backend)) {
            std::printf("%-10s %12s %12s\n", backend_name(backend), "n/a", "n/a");
            continue;
        }
        const double ctr = bench_encrypt(BenchMode::Ctr, backend, as_view(key), as_view(iv),
                                         as_view(data), checksum);
        const double cbc = bench_encrypt(BenchMode::Cbc, backend, as_view(key), as_view(iv),
                                         as_view(data), checksum);
        if (ctr < 0.0 || cbc < 0.0) {
            std::printf("%-10s %12s %12s\n", backend_name(backend), "error", "error");
            continue;
        }
        std::printf("%-10s %12.1f %12.1f\n", backend_name(backend), ctr, cbc);
    }

    // 打印 checksum：跨后端一致（同输入应产出相同密文），同时防空转优化。
    std::printf("\nchecksum: %016llx\n", static_cast<unsigned long long>(checksum));
    return 0;
}
