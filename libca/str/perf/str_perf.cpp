//
// @brief libca/str 编码转换性能基准（非 gtest，独立 main，不进 CI 测试组）。
// @author Canrad
// @date 2026/09/15
//
// 构建：`xmake build -P . libca_str_perf`；运行：直接跑产物
// `./build/<plat>/<arch>/<mode>/libca_str_perf.exe`。
//
// 覆盖内置编码路径的吞吐（MB/s，按输入字节数计）：
//   - utf8 → gb18030（CharsetConverter::utf8_to_gb18030，表驱动 + 四字节算法）
//   - gb18030 → utf8（CharsetConverter::gb18030_to_utf8）
//   - utf8 → utf16  （conversion.hpp 纯算法原语，预分配缓冲，全平台真 UTF-16）
//   - utf8 → cp1252  （CharsetConverter::utf8_to_cp1252，单字节差异表）
//
// 方法：确定性数据（中文样本段落重复填充）；每项热身 1 轮 + 计时 5 轮取中位数；
// 每轮计算 FNV-1a 64 校验和并全程比对，防止编译器把转换优化空转。
// 注意计时只包转换本身，校验和在停表后计算。
//

#include "libca/str/charset.hpp"
#include "libca/str/conversion.hpp"
#include "libca/time/stopwatch.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace ca;

namespace {

// FNV-1a 64：轻量校验和，仅用于防优化空转与轮间一致性比对。
u64 fnv1a64(const void* data, usize size)
{
    const u8*     bytes = static_cast<const u8*>(data);
    u64           hash  = 14695981039346656037ULL;
    constexpr u64 prime = 1099511628211ULL;
    for (usize i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= prime;
    }
    return hash;
}

// 确定性输入：中文样本段落（含全角标点 + ASCII）重复填充到 target_mb 兆。
std::string make_utf8_input(double target_mb)
{
    const char* paragraph =
        "字符编码内置化让 GBK 转换在裁剪 glibc、Alpine、Windows 上行为一致。"
        "CharsetConverter 三级查找：内置表（Tier 1/2）、iconv 回落（Tier 3）、"
        "UNSUPPORTED。GB18030 覆盖 GBK/GB2312，四字节序列走线性区间表，"
        " utf8/utf16/latin1/cp1252 为纯算法实现。Hello libca! 0123456789\n";
    const usize target = static_cast<usize>(target_mb * 1024.0 * 1024.0);
    const usize chunk  = std::char_traits<char>::length(paragraph);
    std::string input;
    input.reserve(target + chunk);
    while (input.size() < target)
        input.append(paragraph, chunk);
    return input;
}

// cp1252 可表示的西文样本（差异表 + Latin-1 区间；此为该文本的 UTF-8 字节，
// python cp1252 codec 验证可完整编码）。用显式字节数组避免 \x 转义被后随
// 十六进制字符吞噬。
std::string make_cp1252_friendly_input(double target_mb)
{
    static const unsigned char kParagraph[] = {
        0x52, 0xC3, 0xA9, 0x73, 0x75, 0x6D, 0xC3, 0xA9, 0x20, 0xE2, 0x80, 0x94,
        0x20, 0x6E, 0x61, 0xC3, 0xAF, 0x76, 0x65, 0x20, 0x66, 0x61, 0xC3, 0xA7,
        0x61, 0x64, 0x65, 0x2C, 0x20, 0xE2, 0x80, 0x9E, 0x64, 0x65, 0x75, 0x74,
        0x73, 0x63, 0x68, 0xE2, 0x80, 0x9C, 0x2C, 0x20, 0xE2, 0x82, 0xAC, 0x20,
        0x34, 0x32, 0x2C, 0x35, 0x30, 0x3B, 0x20, 0xC2, 0xBF, 0x71, 0x75, 0xC3,
        0xA9, 0x3F, 0x20, 0xC2, 0xAB, 0x76, 0x6F, 0x69, 0x6C, 0xC3, 0xA0, 0xC2,
        0xBB, 0xE2, 0x80, 0xA6, 0x20, 0x63, 0x61, 0x66, 0xC3, 0xA9, 0x20, 0x63,
        0x72, 0xC3, 0xA8, 0x6D, 0x65, 0x20, 0xC3, 0x80, 0xC3, 0x89, 0xC3, 0x8E,
        0xC3, 0x94, 0xC3, 0x99, 0x20, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x0A,
    };
    const usize target   = static_cast<usize>(target_mb * 1024.0 * 1024.0);
    const usize chunk    = sizeof(kParagraph);
    const char* raw      = reinterpret_cast<const char*>(kParagraph);
    std::string input;
    input.reserve(target + chunk);
    while (input.size() < target)
        input.append(raw, chunk);
    return input;
}

// 中位数（轮数奇数）。
double median(std::vector<double>& values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// 通用计时骨架：热身 1 轮 + 计时 5 轮取中位数，校验和逐轮比对。
// work 执行转换并返回 (数据指针, 字节数)；指针所指缓冲须在 work 返回后仍有效
// （调用方用 holder 持有输出），停表后才计算校验和。
template <typename Work>
void bench(const char* name, usize input_bytes, Work&& work)
{
    std::vector<double> seconds;
    seconds.reserve(5);

    u64 baseline = 0;
    for (int round = 0; round < 6; ++round) {
        time::Stopwatch watch;
        const auto [ptr, size] = work();
        const time::Duration elapsed = watch.elapsed();
        if (round == 0 && size == 0) {
            std::printf("%-20s EMPTY OUTPUT（样本不可编码？）\n", name);
            return;
        }
        const u64 checksum = fnv1a64(ptr, size);
        if (round == 0) {
            baseline = checksum;  // 热身轮：记录校验和基线
        } else {
            if (checksum != baseline) {
                std::printf("%-20s checksum MISMATCH (0x%016llX vs 0x%016llX)\n", name,
                            static_cast<unsigned long long>(checksum),
                            static_cast<unsigned long long>(baseline));
                return;
            }
            seconds.push_back(elapsed.as_seconds_f64());
        }
    }

    const double med = median(seconds);
    std::printf("%-20s median %8.2f ms | %8.1f MB/s | checksum 0x%016llX\n", name,
                med * 1000.0,
                static_cast<double>(input_bytes) / (1024.0 * 1024.0) / med,
                static_cast<unsigned long long>(baseline));
}

}  // namespace

int main()
{
    const double      input_mb = 2.0;
    const std::string utf8     = make_utf8_input(input_mb);
    const usize       utf8_len = utf8.size();

    // 预生成 gb18030 输入。
    auto gb_result = str::CharsetConverter::utf8_to_gb18030(utf8);
    if (gb_result.is_err()) {
        std::printf("utf8_to_gb18030 failed: %s\n",
                    gb_result.unwrap_err().to_string().c_str());
        return 1;
    }
    const std::string gb = std::move(gb_result).unwrap();

    std::printf("=== libca/str 编码转换吞吐（输入 %.2f MB 中文样本） ===\n\n",
                static_cast<double>(utf8_len) / (1024.0 * 1024.0));

    // utf8 → gb18030：holder 持有输出，work 返回其视图。
    {
        std::string out;
        bench("utf8 -> gb18030", utf8_len, [&] {
            out = str::CharsetConverter::utf8_to_gb18030(utf8).unwrap_or(std::string{});
            return std::make_pair(out.data(), out.size());
        });
    }

    // gb18030 → utf8
    {
        std::string out;
        bench("gb18030 -> utf8", gb.size(), [&] {
            out = str::CharsetConverter::gb18030_to_utf8(gb).unwrap_or(std::string{});
            return std::make_pair(out.data(), out.size());
        });
    }

    // utf8 → utf16：预分配缓冲，纯转换计时（conversion.hpp 原语，全平台真 UTF-16）。
    {
        std::vector<u16> u16buf(utf8_len * 2 + 16);
        bench("utf8 -> utf16", utf8_len, [&] {
            const usize n = str::utf8_to_utf16(reinterpret_cast<const u8*>(utf8.data()),
                                               utf8_len, u16buf.data());
            return std::make_pair(u16buf.data(), n * sizeof(u16));
        });
    }

    // utf8 → cp1252（西文样本：中文不在 cp1252 范围内，会合法报错产出空串）
    {
        const std::string latin_utf8 = make_cp1252_friendly_input(input_mb);
        std::string       out;
        bench("utf8 -> cp1252", latin_utf8.size(), [&] {
            out = str::CharsetConverter::utf8_to_cp1252(latin_utf8).unwrap_or(std::string{});
            return std::make_pair(out.data(), out.size());
        });
    }

    return 0;
}
