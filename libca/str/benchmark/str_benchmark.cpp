/// @file str_benchmark.cpp
/// @brief libca/str 热点原语压力测试（非 gtest，独立 main）。
/// @details 构建：`xmake f --with_tests=y && xmake build libca_str_benchmark`，
///          运行：`xmake run libca_str_benchmark`。覆盖被 json/fs 高频依赖的原语：
///          - Utf8StringBuilder append + build/build_or_empty（校验趟数）；
///          - Utf8StringArena::intern 去重（全命中/全 miss）；
///          - Utf8StringRef::substr / slice_by_cp（码点计数）；
///          - to_lower/to_upper / split（reserve 与分配）。
///          输出取多次运行的最优值（best），近似排除调度抖动。
/// @author Canrad

#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace ca::str;
using Clock = std::chrono::steady_clock;

namespace {

double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// 生成一段可重复的 ASCII 内容（含少量多字节以覆盖非 ASCII 路径）。
std::string make_ascii(int len) {
    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i) s += char('a' + (i % 26));
    return s;
}

template <typename F>
double best_ms(int iters, F&& f) {
    f();  // 预热
    double best = 1e18;
    for (int i = 0; i < iters; ++i) {
        auto t0 = Clock::now();
        f();
        best = std::min(best, ms_since(t0));
    }
    return best;
}

void line(const char* name, double ms, double items, const char* unit) {
    std::printf("%-40s | best=%9.3f ms | %10.1f %s/s\n",
                name, ms, items / (ms / 1000.0), unit);
}

// ---- Builder：append + build_or_empty（锁定单趟校验回归）----
void bench_builder() {
    std::printf("-- Utf8StringBuilder append + build_or_empty --\n");
    for (int slen : {16, 256, 4096}) {
        std::string chunk = make_ascii(slen);
        const int N = 20000;
        double ms = best_ms(5, [&] {
            for (int i = 0; i < N; ++i) {
                Utf8StringBuilder b;
                b.append(reinterpret_cast<const ca::u8*>(chunk.data()), chunk.size());
                volatile auto s = b.build_or_empty();
                (void)s;
            }
        });
        char nm[64];
        std::snprintf(nm, sizeof(nm), "builder slen=%d x%d", slen, N);
        line(nm, ms, double(N), "build");
    }
    std::printf("\n");
}

// ---- Arena intern：全 miss（唯一串）vs 全命中（重复串）----
void bench_arena() {
    std::printf("-- Utf8StringArena::intern --\n");
    const int N = 50000;
    // 全 miss：N 个唯一串
    std::vector<std::string> uniq;
    uniq.reserve(N);
    for (int i = 0; i < N; ++i) uniq.push_back("unique_key_" + std::to_string(i));
    double ms_miss = best_ms(5, [&] {
        Utf8StringArena arena;
        for (auto& s : uniq)
            arena.intern(reinterpret_cast<const ca::u8*>(s.data()), s.size());
    });
    line("intern all-miss (unique)", ms_miss, double(N), "intern");

    // 全命中：同一串 intern N 次（去重走比较路径）
    std::string one = "a_repeated_interned_key_value";
    double ms_hit = best_ms(5, [&] {
        Utf8StringArena arena;
        for (int i = 0; i < N; ++i)
            arena.intern(reinterpret_cast<const ca::u8*>(one.data()), one.size());
    });
    line("intern all-hit (dedup)", ms_hit, double(N), "intern");
    std::printf("\n");
}

// ---- substr / slice_by_cp（锁定码点计数成本）----
void bench_substr() {
    std::printf("-- substr / slice_by_cp --\n");
    std::string big = make_ascii(100000);
    Utf8String s(reinterpret_cast<const ca::u8*>(big.data()), big.size());
    const int N = 50000;
    double ms_sub = best_ms(5, [&] {
        for (int i = 0; i < N; ++i) {
            volatile auto sub = s.substr(i % 1000, 64);
            (void)sub;
        }
    });
    line("substr(offset,64) x50000", ms_sub, double(N), "substr");
    std::printf("\n");
}

// ---- to_lower / split（锁定 reserve 与分配）----
void bench_transform() {
    std::printf("-- to_lower / split --\n");
    std::string big = make_ascii(50000);
    Utf8String s(reinterpret_cast<const ca::u8*>(big.data()), big.size());
    const int N = 2000;
    double ms_lower = best_ms(5, [&] {
        for (int i = 0; i < N; ++i) { volatile auto l = s.ref().to_lower(); (void)l; }
    });
    line("to_lower 50KB x2000", ms_lower, double(N), "call");

    std::string csv;
    for (int i = 0; i < 5000; ++i) { if (i) csv += ','; csv += "field"; csv += std::to_string(i); }
    Utf8String cs(reinterpret_cast<const ca::u8*>(csv.data()), csv.size());
    auto comma = Utf8StringRef::from_cstr(",");
    const int M = 2000;
    double ms_split = best_ms(5, [&] {
        volatile ca::usize sink = 0;
        for (int i = 0; i < M; ++i) { auto parts = cs.split(comma); sink += parts.size(); }
        (void)sink;
    });
    line("split 5000-field x2000", ms_split, double(M), "call");
    std::printf("\n");
}

}  // namespace

int main() {
    std::printf("=== libca/str 热点原语基准 ===\n\n");
    bench_builder();
    bench_arena();
    bench_substr();
    bench_transform();
    return 0;
}
