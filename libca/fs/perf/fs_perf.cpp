// 文件 IO 微基准（#249）：同一批样本（小 <4KB / 中 / 大顺序读）对比
//   读：ifstream 整读（复刻 FileUtil 现状：exists+is_regular_file 预检）
//       / ifstream 整读（无预检）/ stdio 整读（fopen+fseek+fread）/ 原生 fd（open+read，下限参照）
//   写：ofstream 整写（现状）/ stdio 整写（fopen+fwrite）
// 替换与否由数字决定（见 issue #249 验收标准），不凭体感。
// 手动运行：xmake build -P . libca_fs_perf && xmake run -P . libca_fs_perf

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "libca/fs/file_util.hpp"
#include "libca/fs/stdio_file.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

// ==================== 平台分支（与拟落地的 fs 封装同一套口径） ====================

#ifdef _WIN32
// mode 仅含 ASCII（"rb"/"wb" 等），逐字符拓宽即可（与 zip::open_stdio_file 同口径）。
std::FILE* stdio_open(const fs::path& p, const char* mode)
{
    std::wstring wide_mode;
    for (const char* q = mode; *q != '\0'; ++q)
        wide_mode.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*q)));
    std::FILE* fp = nullptr;
    if (::_wfopen_s(&fp, p.c_str(), wide_mode.c_str()) != 0)
        return nullptr;
    return fp;
}

int fd_open_read(const fs::path& p)
{
    return ::_wopen(p.c_str(), _O_RDONLY | _O_BINARY);
}

int fd_open_write(const fs::path& p)
{
    return ::_wopen(p.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
}

int fd_stat_size(int fd, long long* out)
{
    struct _stat64 st;
    if (::_fstat64(fd, &st) != 0)
        return -1;
    *out = st.st_size;
    return 0;
}

using fd_read_ret = int;
#else
std::FILE* stdio_open(const fs::path& p, const char* mode)
{
    return std::fopen(p.c_str(), mode);
}

int fd_open_read(const fs::path& p)
{
    return ::open(p.c_str(), O_RDONLY);
}

int fd_open_write(const fs::path& p)
{
    return ::open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

int fd_stat_size(int fd, long long* out)
{
    struct stat st;
    if (::fstat(fd, &st) != 0)
        return -1;
    *out = static_cast<long long>(st.st_size);
    return 0;
}

using fd_read_ret = ssize_t;
#endif

// fseek/ftell 的 64 位口径：MSVC 下裸 fseek/ftell 是 32 位（>2GB 截断）。
#ifdef _WIN32
#define PERF_SEEK64 _fseeki64
#define PERF_TELL64 _ftelli64
#else
#define PERF_SEEK64 fseeko
#define PERF_TELL64 ftello
#endif

// ==================== 四种整读实现 ====================

// 复刻 FileUtil::read_all_text 现状（iostream + 预检 + tellg/seekg/read）。
bool read_ifstream_current(const fs::path& p, std::string& out)
{
    if (!fs::exists(p) || !fs::is_regular_file(p))
        return false;
    std::ifstream file(p, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;
    auto size = file.tellg();
    if (size < 0)
        return false;
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(&out[0], size);
    file.close();
    return !file.fail() || file.eof();
}

// ifstream 不带预检：隔离"流机制开销"与"exists/is_regular_file 两次 stat 开销"。
bool read_ifstream_nostat(const fs::path& p, std::string& out)
{
    std::ifstream file(p, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;
    auto size = file.tellg();
    if (size < 0)
        return false;
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(&out[0], size);
    file.close();
    return !file.fail() || file.eof();
}

// 拟落地的 stdio 整读：fopen + seek END 取尺寸 + rewind + 单次 fread。
bool read_stdio(const fs::path& p, std::string& out)
{
    std::FILE* fp = stdio_open(p, "rb");
    if (fp == nullptr)
        return false;
    if (PERF_SEEK64(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    const long long size = PERF_TELL64(fp);
    if (size < 0) {
        std::fclose(fp);
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    std::rewind(fp);
    bool ok = true;
    if (size > 0) {
        ok = std::fread(&out[0], 1, static_cast<std::size_t>(size), fp) ==
             static_cast<std::size_t>(size);
    }
    std::fclose(fp);
    return ok;
}

// 原生 fd：open + fstat + read 循环 + close（同步 IO 的理论下限参照）。
bool read_fd(const fs::path& p, std::string& out)
{
    const int fd = fd_open_read(p);
    if (fd < 0)
        return false;
    long long size = 0;
    if (fd_stat_size(fd, &size) != 0 || size < 0) {
#ifdef _WIN32
        ::_close(fd);
#else
        ::close(fd);
#endif
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    std::size_t done = 0;
    while (done < out.size()) {
        const fd_read_ret n =
#ifdef _WIN32
            ::_read(fd, &out[done], static_cast<unsigned>(out.size() - done));
#else
            ::read(fd, &out[done], out.size() - done);
#endif
        if (n <= 0)
            break;
        done += static_cast<std::size_t>(n);
    }
#ifdef _WIN32
    ::_close(fd);
#else
    ::close(fd);
#endif
    return done == out.size();
}

// ==================== 两种整写实现 ====================

// 复刻 FileUtil::write_bytes 非 CREATE_NEW 分支现状（ofstream trunc + write + close）。
bool write_ofstream(const fs::path& p, const std::string& content)
{
    std::ofstream file(p, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.close();
    return !file.fail();
}

// 拟落地的 stdio 整写：fopen wb + 单次 fwrite + fclose（fclose 冲刷并报错误）。
bool write_stdio(const fs::path& p, const std::string& content)
{
    std::FILE* fp = stdio_open(p, "wb");
    if (fp == nullptr)
        return false;
    bool ok = true;
    if (!content.empty()) {
        ok = std::fwrite(content.data(), 1, content.size(), fp) == content.size();
    }
    ok = (std::fclose(fp) == 0) && ok;
    return ok;
}

// stdio 整写（无缓冲）：setvbuf(_IONBF) 强制大块 fwrite 直写，隔离
// fwrite 经内部缓冲多一次 memcpy 的假设（对照 ofstream 的直接 WriteFile 路径）。
bool write_stdio_nobuf(const fs::path& p, const std::string& content)
{
    std::FILE* fp = stdio_open(p, "wb");
    if (fp == nullptr)
        return false;
    std::setvbuf(fp, nullptr, _IONBF, 0);
    bool ok = true;
    if (!content.empty()) {
        ok = std::fwrite(content.data(), 1, content.size(), fp) == content.size();
    }
    ok = (std::fclose(fp) == 0) && ok;
    return ok;
}

// ==================== 计时与样本 ====================

// 真实 API（落地后）：FileUtil::read_all_text（1 次 status 预检 + stdio 整读）。
bool read_api_file_util(const fs::path& p, std::string& out)
{
    auto r = ca::fs::FileUtil::read_all_text(ca::fs::Path(p));
    if (r.is_err())
        return false;
    out = std::move(r).unwrap();
    return true;
}

// 真实 API（落地后）：fs::read_file_all（无预检 stdio 整读，解析器 read_file 路径）。
bool read_api_raw(const fs::path& p, std::string& out)
{
    auto r = ca::fs::read_file_all(ca::fs::Path(p));
    if (r.is_err())
        return false;
    out = std::move(r).unwrap();
    return true;
}

using Clock = std::chrono::steady_clock;

template <typename Fn>
double bench_ns(Fn fn, int warmup, int iters)
{
    for (int i = 0; i < warmup; ++i)
        fn();
    const auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i)
        fn();
    const auto t1 = Clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() /
           static_cast<double>(iters);
}

double mb_per_sec(std::size_t bytes, double ns_per_op)
{
    if (ns_per_op <= 0)
        return 0.0;
    return static_cast<double>(bytes) / (ns_per_op / 1e9) / (1024.0 * 1024.0);
}

}  // namespace

int main()
{
    const fs::path dir = fs::temp_directory_path() / "libca_fs_perf";
    std::error_code ec;
    fs::create_directories(dir, ec);

    struct Sample
    {
        const char* name;
        std::size_t size;
        int         iters;
        int         warmup;
    };
    const Sample samples[] = {
        {"small-2KB", 2 * 1024, 5000, 200},
        {"mid-64KB", 64 * 1024, 1000, 50},
        {"large-4MB", 4 * 1024 * 1024, 100, 10},
    };

    std::printf("#249 file IO micro-benchmark\n");
#ifdef _WIN32
    std::printf("platform: Windows (MSVC runtime), sync IO, OS cache warm\n");
#else
    std::printf("platform: POSIX, sync IO, OS cache warm\n");
#endif
    std::printf("%-12s %-22s %12s %10s\n", "sample", "variant", "ns/op", "MiB/s");

    for (const auto& s : samples) {
        const fs::path p = dir / (std::string(s.name) + ".bin");
        std::string payload(s.size, 'x');
        for (std::size_t i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>('a' + (i % 26));
        write_ofstream(p, payload);

        // 各实现结果必须逐字节一致，否则基准无效。
        std::string r0, r1, r2, r3, r4, r5;
        if (!read_ifstream_current(p, r0) || !read_ifstream_nostat(p, r1) ||
            !read_stdio(p, r2) || !read_fd(p, r3) || !read_api_file_util(p, r4) ||
            !read_api_raw(p, r5) || r0 != payload || r1 != r0 || r2 != r0 || r3 != r0 ||
            r4 != r0 || r5 != r0) {
            std::printf("CORRECTNESS FAILURE on %s — abort\n", s.name);
            return 1;
        }

        std::string buf;
        double      ns = 0;

        ns = bench_ns([&] { read_ifstream_current(p, buf); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "read:ifstream(cur)", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { read_ifstream_nostat(p, buf); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "read:ifstream(nostat)", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { read_stdio(p, buf); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "read:stdio", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { read_fd(p, buf); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "read:fd", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { read_api_file_util(p, buf); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "read:api-FileUtil", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { read_api_raw(p, buf); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "read:api-read_file_all", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { write_ofstream(p, payload); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "write:ofstream(cur)", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { write_stdio(p, payload); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "write:stdio", ns,
                    mb_per_sec(s.size, ns));

        ns = bench_ns([&] { write_stdio_nobuf(p, payload); }, s.warmup, s.iters);
        std::printf("%-12s %-22s %12.0f %10.1f\n", s.name, "write:stdio-nobuf", ns,
                    mb_per_sec(s.size, ns));

        fs::remove(p, ec);
    }

    fs::remove_all(dir, ec);
    return 0;
}
