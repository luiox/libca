/// @file core_benchmark.cpp
/// @brief libca/core 热点类型压力测试（非 gtest，独立 main）。
/// @details 构建：`xmake f --with_tests=y && xmake build libca_core_benchmark`，
///          运行：`xmake run libca_core_benchmark`。core 多为薄封装/constexpr，
///          仅对真正有代码生成且被 net/http/json/csv 高频依赖的两类建基线：
///          - BytesMut put/reserve/grow（序列化热点，锁定扩容策略与溢出守卫成本）；
///          - Result 构造 + 赋值 + map/and_then 链（几乎所有可失败 API 的返回类型）。
///          输出取多次运行的最优值（best），近似排除调度抖动。
/// @author Canrad

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace ca;
using namespace ca::core;
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
    std::printf("%-42s | best=%9.3f ms | %10.1f %s/s\n", name, ms, items / (ms / 1000.0), unit);
}

// ---- BytesMut：put_slice 增长 + 类型化写 ----
void bench_bytes()
{
    std::printf("-- BytesMut put/grow --\n");
    std::vector<u8> chunk(64, 0xAB);
    const int       N       = 200000;   // 追加次数
    double          ms_grow = best_ms(5, [&] {
        BytesMut b;
        for (int i = 0; i < N; ++i)
            b.put_slice(chunk.data(), chunk.size());
    });
    line("put_slice 64B x200000 (grow)", ms_grow, double(N), "put");

    double ms_reserved = best_ms(5, [&] {
        BytesMut b;
        b.reserve(usize(N) * 64);   // 预留后不再扩容
        for (int i = 0; i < N; ++i)
            b.put_slice(chunk.data(), chunk.size());
    });
    line("put_slice 64B x200000 (reserved)", ms_reserved, double(N), "put");

    const int M      = 1000000;
    double    ms_u32 = best_ms(5, [&] {
        BytesMut b;
        b.reserve(usize(M) * 4);
        for (int i = 0; i < M; ++i)
            b.put_u32_be(static_cast<u32>(i));
    });
    line("put_u32_be x1000000", ms_u32, double(M), "put");
    std::printf("\n");
}

// ---- Result：构造 + 赋值 + 链式组合 ----
void bench_result()
{
    std::printf("-- Result construct/assign/chain --\n");
    const int N = 1000000;

    // 构造 Ok<int>
    double ms_ok = best_ms(5, [&] {
        i64 acc = 0;
        for (int i = 0; i < N; ++i) {
            Result<int, std::string> r = Ok(i);
            acc += r.unwrap();
        }
        volatile i64 sink = acc;
        (void)sink;
    });
    line("construct+unwrap Ok<int> x1000000", ms_ok, double(N), "op");

    // 赋值（本 PR 新加的能力）：Result<std::string> 重复赋值
    double ms_assign = best_ms(5, [&] {
        Result<std::string, int> r = Err(0);
        for (int i = 0; i < N; ++i)
            r = Ok(std::to_string(i & 0xFF));
        volatile bool ok = r.is_ok();
        (void)ok;
    });
    line("assign Ok<string> x1000000", ms_assign, double(N), "assign");

    // map + and_then 链
    double ms_chain = best_ms(5, [&] {
        i64 acc = 0;
        for (int i = 0; i < N; ++i) {
            Result<int, std::string> r  = Ok(i);
            auto                     r2 = r.map([](int v) {
                           return v * 2;
                       }).and_then([](int v) -> Result<int, std::string> { return Ok(v + 1); });
            acc += r2.unwrap_or(0);
        }
        volatile i64 sink = acc;
        (void)sink;
    });
    line("map+and_then chain x1000000", ms_chain, double(N), "chain");
    std::printf("\n");
}

}   // namespace

int main()
{
    std::printf("=== libca/core 热点类型基准 ===\n\n");
    bench_bytes();
    bench_result();
    return 0;
}
