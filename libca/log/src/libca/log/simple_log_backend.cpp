#include "libca/log/simple_log_backend.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <io.h>   // _isatty
#    include <windows.h>
#else
#    include <unistd.h>   // isatty
#endif

namespace ca::log {

namespace {

#if defined(_WIN32)

// Windows 终端着色用 SetConsoleTextAttribute。
void set_console_color(Level level, FILE* fp)
{
    HANDLE h = GetStdHandle(fp == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE)
        return;
    WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;   // 默认灰白
    switch (level) {
    case Level::Trace: attr = FOREGROUND_INTENSITY; break;                 // 灰
    case Level::Debug: attr = FOREGROUND_GREEN | FOREGROUND_BLUE; break;   // 青
    case Level::Info: attr = FOREGROUND_GREEN; break;                      // 绿
    case Level::Warn:
        attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;                                                                 // 黄
    case Level::Error_: attr = FOREGROUND_RED | FOREGROUND_INTENSITY; break;   // 亮红
    case Level::Critical:
        attr = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
               FOREGROUND_INTENSITY;
        break;   // 红底
    case Level::Off: break;
    }
    SetConsoleTextAttribute(h, attr);
}

void reset_console_color(FILE* fp)
{
    HANDLE h = GetStdHandle(fp == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE)
        return;
    SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

bool is_tty(FILE* fp)
{
    return _isatty(_fileno(fp)) != 0;
}

#else   // POSIX

const char* ansi_color(Level level)
{
    switch (level) {
    case Level::Trace: return "\033[90m";           // 灰
    case Level::Debug: return "\033[36m";           // 青
    case Level::Info: return "\033[32m";            // 绿
    case Level::Warn: return "\033[33m";            // 黄
    case Level::Error_: return "\033[1;31m";        // 亮红
    case Level::Critical: return "\033[41;1;37m";   // 红底白字
    case Level::Off: return "";
    }
    return "";
}

constexpr const char* kAnsiReset = "\033[0m";

bool is_tty(FILE* fp)
{
    return isatty(fileno(fp)) != 0;
}

#endif

// 格式化当前时间到 str（按 strftime）。失败/空格式返回空。
std::string format_now(const std::string& fmt)
{
    if (fmt.empty())
        return std::string();
    using std::chrono::system_clock;
    auto    now = system_clock::to_time_t(system_clock::now());
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    std::string out(32, '\0');
    for (;;) {
        auto written = std::strftime(out.data(), out.size(), fmt.c_str(), &tm_buf);
        if (written != 0) {
            out.resize(written);
            return out;
        }
        out.resize(out.size() * 2);
        if (out.size() > 256)   // 防御性上限
            return std::string();
    }
}

}   // namespace

SimpleLogBackend::SimpleLogBackend() = default;

SimpleLogBackend::SimpleLogBackend(SimpleLogConfig config)
    : config_(std::move(config))
{
    if (!config_.file_path.empty()) {
        // 二进制追加模式：避免 Windows 文本模式把 \n 翻译成 \r\n。
        file_.open(config_.file_path, std::ios::out | std::ios::app | std::ios::binary);
    }
}

void SimpleLogBackend::log(Level level, std::string_view target, std::string_view file, int line,
                           const OpaqueFormat& message)
{
    // 组装一行（不含颜色），日志内容统一格式：
    //   [time] [LEVEL] [target] message (file:line)
    std::ostringstream oss;

    if (!config_.time_format.empty()) {
        auto t = format_now(config_.time_format);
        if (!t.empty())
            oss << '[' << t << "] ";
    }

    oss << '[' << to_string(level) << "] ";

    if (config_.show_target) {
        oss << '[' << target << "] ";
    }

    // 消息体（render_to 追加到 string，再写入 ostringstream）
    std::string rendered;
    message.render_to(rendered);
    oss << rendered;

    if (config_.show_location && !file.empty()) {
        oss << " (" << file << ':' << line << ')';
    }

    oss << '\n';
    std::string text = oss.str();

    std::lock_guard<std::mutex> lock(mutex_);

    FILE* fp = (config_.stream == SimpleLogConfig::Stream::Stdout) ? stdout : stderr;

    // 终端彩色输出（仅 TTY 时着色，避免重定向到文件时混入控制码）
    if (config_.color && is_tty(fp)) {
#if defined(_WIN32)
        set_console_color(level, fp);
        std::fwrite(text.data(), 1, text.size(), fp);
        reset_console_color(fp);
#else
        std::fwrite(ansi_color(level), 1, std::strlen(ansi_color(level)), fp);
        std::fwrite(text.data(), 1, text.size(), fp);
        std::fwrite(kAnsiReset, 1, std::strlen(kAnsiReset), fp);
#endif
    }
    else {
        std::fwrite(text.data(), 1, text.size(), fp);
    }
    std::fflush(fp);

    // 文件输出（不含颜色码）
    if (file_.is_open()) {
        file_.write(text.data(), static_cast<std::streamsize>(text.size()));
        file_.flush();
    }
}

}   // namespace ca::log
