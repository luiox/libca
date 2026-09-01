/// @file json_benchmark.cpp
/// @brief libca/json 解析/序列化压力测试（非 gtest，独立 main）。
/// @details 构建：`xmake f --with_tests=y && xmake build libca_json_benchmark`，
///          运行：`xmake run libca_json_benchmark`。用于回归性能，特别是：
///          - 大扁平对象：探测 DOM 装配是否退化为 O(n^2)；
///          - 记录数组：典型 API 载荷；
///          - 长字符串数组：字符串解析路径（快/慢路径与拷贝次数）。
///          输出取多次运行的最优值（best），近似排除调度抖动。
/// @author Canrad

#include "libca/json/json.hpp"
#include "libca/str/utf8_string.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace ca::json;
using ca::str::Utf8StringRef;
using Clock = std::chrono::steady_clock;

namespace {

double ms_since(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// {"key_0":0,"key_1":1,...}，n 个成员的扁平对象。
std::string gen_flat_object(int n)
{
    std::string s = "{";
    for (int i = 0; i < n; ++i) {
        if (i)
            s += ',';
        s += "\"key_";
        s += std::to_string(i);
        s += "\":";
        s += std::to_string(i);
    }
    s += "}";
    return s;
}

// n 个记录的对象数组，模拟典型 API 载荷。
std::string gen_record_array(int n)
{
    std::string  s = "[";
    std::mt19937 rng(42);
    for (int i = 0; i < n; ++i) {
        if (i)
            s += ',';
        s += "{\"id\":";
        s += std::to_string(i);
        s += ",\"name\":\"user_";
        s += std::to_string(rng() % 100000);
        s += "\",\"email\":\"user";
        s += std::to_string(i);
        s += "@example.com\"";
        s += ",\"active\":";
        s += (i & 1) ? "true" : "false";
        s += ",\"score\":";
        s += std::to_string((rng() % 10000) / 100.0);
        s += ",\"tags\":[\"a\",\"bb\",\"ccc\"]}";
    }
    s += "]";
    return s;
}

// n 个长度为 slen 的无转义字符串数组，测字符串解析路径。
std::string gen_string_array(int n, int slen)
{
    std::string s = "[";
    for (int i = 0; i < n; ++i) {
        if (i)
            s += ',';
        s += '"';
        for (int j = 0; j < slen; ++j)
            s += char('a' + (j % 26));
        s += '"';
    }
    s += "]";
    return s;
}

struct Res
{
    double parse_ms;
    double write_ms;
};

Res run_once(const std::string& text, bool do_write)
{
    Res  r{0, 0};
    auto view   = Utf8StringRef::from_string_view(std::string_view(text.data(), text.size()));
    auto t0     = Clock::now();
    auto result = JsonReader::read(view);
    r.parse_ms  = ms_since(t0);
    if (result.is_err()) {
        std::printf("  PARSE ERROR\n");
        return r;
    }
    JsonDocument doc = std::move(result).unwrap();
    if (do_write) {
        auto              t1 = Clock::now();
        JsonWriterOptions opts;
        volatile auto     out = JsonWriter::write(doc, opts);
        r.write_ms            = ms_since(t1);
        (void)out;
    }
    return r;
}

void bench(const std::string& name, const std::string& text, int iters, bool do_write)
{
    run_once(text, do_write);   // 预热
    double best_parse = 1e18, best_write = 1e18;
    for (int i = 0; i < iters; ++i) {
        Res r      = run_once(text, do_write);
        best_parse = std::min(best_parse, r.parse_ms);
        best_write = std::min(best_write, r.write_ms);
    }
    double mb = text.size() / (1024.0 * 1024.0);
    std::printf("%-28s | size=%7.2f MB | parse best=%8.3f ms (%6.1f MB/s)",
                name.c_str(),
                mb,
                best_parse,
                mb / (best_parse / 1000.0));
    if (do_write)
        std::printf(" | write best=%8.3f ms (%6.1f MB/s)", best_write, mb / (best_write / 1000.0));
    std::printf("\n");
}

}   // namespace

int main()
{
    std::printf("=== libca/json 性能基准 ===\n\n");

    std::printf("-- 大扁平对象（探测 DOM 装配是否 O(n^2)）--\n");
    for (int n : {1000, 4000, 8000, 16000}) {
        bench("flat_object n=" + std::to_string(n), gen_flat_object(n), 5, false);
    }
    std::printf("\n");

    std::printf("-- 记录数组（典型 API 载荷）--\n");
    for (int n : {1000, 10000, 50000}) {
        bench("record_array n=" + std::to_string(n), gen_record_array(n), 5, true);
    }
    std::printf("\n");

    std::printf("-- 长字符串数组（字符串解析路径）--\n");
    for (int slen : {8, 64, 512}) {
        bench("string_array slen=" + std::to_string(slen), gen_string_array(20000, slen), 5, true);
    }
    std::printf("\n");

    return 0;
}
