#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "libca/core/status.hpp"

// 必须先于下面的 #undef 完整展开 fmt：Windows UCRT 把 stdin/stdout/stderr 定义为宏
// （corecrt_wstdio.h: `#define stdout (__acrt_iob_func(1))`），本头文件为让同名 API
// 标识符（Command::stdin/stdout/stderr 等成员）不被宏污染而 #undef 它们；但 fmt
// header-only 模式下 format-inl.h 的 assert/异常路径引用全局 stderr/stdout，宏被 undef
// 后这些标识符彻底消失（Windows 上它们只是宏，无底层 FILE* 变量）。这里先 include
// format.hpp 让 fmt 在宏仍存在时完整展开，之后 #undef 不影响已展开的 fmt 代码——
// 该约束内聚在本头文件内，任何 include 顺序都安全。
#include "libca/str/format.hpp"

#ifdef stdin
#    undef stdin
#endif
#ifdef stdout
#    undef stdout
#endif
#ifdef stderr
#    undef stderr
#endif

namespace ca::process {

class Command;

namespace ipc {

struct AnonymousPipe;

/// @brief 匿名管道的只读端，move-only。
/// @details 拥有一个原生管道读句柄；子进程的标准输出/错误流（ChildStdout / ChildStderr）
///          也是它的别名。read() 读取到所有写端关闭后返回 0 字节表示 EOF。
class PipeReader
{
public:
    PipeReader() = default;
    ~PipeReader();
    PipeReader(const PipeReader&)            = delete;
    PipeReader& operator=(const PipeReader&) = delete;
    PipeReader(PipeReader&& other) noexcept;
    PipeReader& operator=(PipeReader&& other) noexcept;

    /// @brief 读取最多 capacity 字节到 buffer。
    /// @param buffer 接收缓冲区。
    /// @param capacity 缓冲区容量。
    /// @return 成功返回实际读取字节数（0 表示所有写端已关闭的干净 EOF）；
    ///         系统错误返回 Status。
    ca::core::StatusResult<usize>       read(void* buffer, usize capacity);
    /// @brief 读取直到所有写端关闭，把全部内容拼成字符串返回。
    ca::core::StatusResult<std::string> read_to_end();
    /// @brief 句柄是否仍打开。
    bool                                is_open() const noexcept;
    /// @brief 关闭并释放底层句柄；重复调用安全。
    void                                close() noexcept;

private:
    explicit PipeReader(std::intptr_t native_handle) noexcept;
    std::intptr_t release() noexcept;

    std::intptr_t native_handle_{-1};

    friend class ::ca::process::Command;
    friend ca::core::StatusResult<AnonymousPipe> create_anonymous_pipe();
};

/// @brief 匿名管道的只写端，move-only。
/// @details 子进程标准输入流（ChildStdin）是它的别名。write_all() 内部循环直到全部写出，
///          管道读端全部关闭后再写会返回错误。
class PipeWriter
{
public:
    PipeWriter() = default;
    ~PipeWriter();
    PipeWriter(const PipeWriter&)            = delete;
    PipeWriter& operator=(const PipeWriter&) = delete;
    PipeWriter(PipeWriter&& other) noexcept;
    PipeWriter& operator=(PipeWriter&& other) noexcept;

    /// @brief 循环写入直到 length 字节全部写出。
    /// @return 写成功返回 Ok；读端已关闭或系统错误返回 Status。
    ca::core::Status write_all(const void* data, usize length);
    /// @brief 写入字符串全部内容。
    ca::core::Status write_all(const std::string& data);
    bool             is_open() const noexcept;
    void             close() noexcept;

private:
    explicit PipeWriter(std::intptr_t native_handle) noexcept;
    std::intptr_t release() noexcept;

    std::intptr_t native_handle_{-1};

    friend class ::ca::process::Command;
    friend ca::core::StatusResult<AnonymousPipe> create_anonymous_pipe();
};

/// @brief 一对匿名管道读写端。
struct AnonymousPipe
{
    PipeReader reader;
    PipeWriter writer;
};

/// @brief 创建一对匿名管道。
/// @return 成功返回 AnonymousPipe；失败返回 Status。
ca::core::StatusResult<AnonymousPipe> create_anonymous_pipe();

}   // namespace ipc

using ChildStdin  = ipc::PipeWriter;
using ChildStdout = ipc::PipeReader;
using ChildStderr = ipc::PipeReader;

/// @brief 子进程标准流的配置选项。
/// @details 用静态工厂创建后传给 Command::stdin/stdout/stderr。inherit 继承父进程
///          （默认），null 连接到平台空设备，piped 创建匿名管道并由父进程经
///          Child 的 take_stdin/take_stdout/take_stderr 取得管道端。
class Stdio
{
public:
    /// @brief 继承父进程对应的标准流（默认）。
    static Stdio inherit() noexcept;
    /// @brief 连接到平台空设备（/dev/null 或 NUL）。
    static Stdio null() noexcept;
    /// @brief 创建匿名管道，父进程端通过 Child 的 take_* 取出。
    static Stdio piped() noexcept;

private:
    enum class Mode : u8
    {
        Inherit,
        Null,
        Piped
    };

    explicit Stdio(Mode mode) noexcept;
    Mode mode_{Mode::Inherit};

    friend class Command;
};

/// @brief 子进程退出状态。
/// @details code 为正常退出码或平台衍生的终止码；非零退出码是有效的进程数据，
///          不是 Status 错误。
struct ExitStatus
{
    i32  code{-1};
    /// @brief 退出码是否为 0（成功）。
    bool success() const noexcept;
};

/// @brief 一次性收集的子进程输出。
struct Output
{
    ExitStatus  status;
    std::string stdout_data;
    std::string stderr_data;
};

/// @brief 拥有一个子进程的 move-only 句柄。
/// @details 析构会关闭它持有的标准流端和原生进程句柄，但不会终止仍在运行的子进程；
///          需要终止须显式调用 kill()。wait_with_output() 会先关闭 stdin 再并发排空
///          stdout/stderr，避免管道写满导致死锁。
class Child
{
public:
    ~Child();
    Child(const Child&)            = delete;
    Child& operator=(const Child&) = delete;
    Child(Child&& other) noexcept;
    Child& operator=(Child&& other) noexcept;

    /// @brief 返回平台进程标识（Linux 为 pid，Windows 为进程 id）。
    u64                                               id() const noexcept;
    /// @brief 非阻塞试探子进程是否已退出。
    /// @return 子进程仍在运行返回空 optional；已退出返回 ExitStatus。
    ca::core::StatusResult<std::optional<ExitStatus>> try_wait();
    /// @brief 阻塞回收子进程，返回退出状态。
    ca::core::StatusResult<ExitStatus>                wait();
    /// @brief 最多等待 timeout，超时不杀子进程，返回空 optional。
    ca::core::StatusResult<std::optional<ExitStatus>> wait_for(std::chrono::milliseconds timeout);
    /// @brief 终止子进程。Linux 上子进程在新进程组内，kill 针对整个组；
    ///        Windows 上针对单个子进程句柄。终止后仍需调用 wait() 回收。
    ca::core::Status                                  kill();

    /// @brief 取出（并交出所有权）子进程标准输入写端；未配置 piped 返回空。
    std::optional<ChildStdin>      take_stdin();
    /// @brief 取出子进程标准输出读端；未配置 piped 返回空。
    std::optional<ChildStdout>     take_stdout();
    /// @brief 取出子进程标准错误读端；未配置 piped 返回空。
    std::optional<ChildStderr>     take_stderr();
    /// @brief 关闭 stdin 后并发排空 stdout/stderr，阻塞到子进程退出，返回全部输出。
    ca::core::StatusResult<Output> wait_with_output();

private:
    Child(std::intptr_t native_process, u64 process_id, std::optional<ChildStdin> stdin,
          std::optional<ChildStdout> stdout, std::optional<ChildStderr> stderr) noexcept;
    void close_process() noexcept;

    std::intptr_t              native_process_{-1};
    u64                        process_id_{0};
    std::optional<ExitStatus>  exit_status_;
    std::optional<ChildStdin>  stdin_;
    std::optional<ChildStdout> stdout_;
    std::optional<ChildStderr> stderr_;

    friend class Command;
};

/// @brief 子进程启动配置，可复用。
/// @details 持有可执行文件路径、参数、可选工作目录和三个标准流配置。arg() 逐个追加
///          精确参数，不启动 shell、不做命令行解析。一个 Command 可多次调用
///          spawn()/status()/output()。所有权形状对齐 Rust std::process::Command。
class Command
{
public:
    /// @brief 构造指定可执行程序的启动配置。
    explicit Command(std::string program);

    /// @brief 追加一个精确参数（不经 shell 解析）。
    Command& arg(std::string value);
    /// @brief 追加多个参数。
    Command& args(std::vector<std::string> values);
    /// @brief 设置子进程工作目录。
    Command& current_dir(std::string path);
    /// @brief 设置或覆盖一个子进程环境变量，同时保留其他继承环境变量。
    Command& env(std::string key, std::string value);
    /// @brief 配置标准输入。
    Command& stdin(Stdio stdio);
    /// @brief 配置标准输出。
    Command& stdout(Stdio stdio);
    /// @brief 配置标准错误。
    Command& stderr(Stdio stdio);

    /// @brief 启动子进程并返回 Child 句柄。
    ca::core::StatusResult<Child>      spawn() const;
    /// @brief 启动子进程、等待其退出并返回退出状态（不收集输出）。
    ca::core::StatusResult<ExitStatus> status() const;
    /// @brief 启动子进程、收集 stdout/stderr 并返回 Output。
    ca::core::StatusResult<Output>     output() const;

private:
    std::string                program_;
    std::vector<std::string>   args_;
    std::vector<std::pair<std::string, std::string>> env_;
    std::optional<std::string> current_dir_;
    Stdio                      stdin_{Stdio::inherit()};
    Stdio                      stdout_{Stdio::inherit()};
    Stdio                      stderr_{Stdio::inherit()};
};

}   // namespace ca::process
