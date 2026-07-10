#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "libca/core/status.hpp"

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

class PipeReader
{
public:
    PipeReader() = default;
    ~PipeReader();
    PipeReader(const PipeReader&)            = delete;
    PipeReader& operator=(const PipeReader&) = delete;
    PipeReader(PipeReader&& other) noexcept;
    PipeReader& operator=(PipeReader&& other) noexcept;

    ca::core::StatusResult<usize>       read(void* buffer, usize capacity);
    ca::core::StatusResult<std::string> read_to_end();
    bool                                is_open() const noexcept;
    void                                close() noexcept;

private:
    explicit PipeReader(std::intptr_t native_handle) noexcept;
    std::intptr_t release() noexcept;

    std::intptr_t native_handle_{-1};

    friend class ::ca::process::Command;
    friend ca::core::StatusResult<AnonymousPipe> create_anonymous_pipe();
};

class PipeWriter
{
public:
    PipeWriter() = default;
    ~PipeWriter();
    PipeWriter(const PipeWriter&)            = delete;
    PipeWriter& operator=(const PipeWriter&) = delete;
    PipeWriter(PipeWriter&& other) noexcept;
    PipeWriter& operator=(PipeWriter&& other) noexcept;

    ca::core::Status write_all(const void* data, usize length);
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

struct AnonymousPipe
{
    PipeReader reader;
    PipeWriter writer;
};

ca::core::StatusResult<AnonymousPipe> create_anonymous_pipe();

}   // namespace ipc

using ChildStdin  = ipc::PipeWriter;
using ChildStdout = ipc::PipeReader;
using ChildStderr = ipc::PipeReader;

class Stdio
{
public:
    static Stdio inherit() noexcept;
    static Stdio null() noexcept;
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

struct ExitStatus
{
    i32  code{-1};
    bool success() const noexcept;
};

struct Output
{
    ExitStatus  status;
    std::string stdout_data;
    std::string stderr_data;
};

class Child
{
public:
    ~Child();
    Child(const Child&)            = delete;
    Child& operator=(const Child&) = delete;
    Child(Child&& other) noexcept;
    Child& operator=(Child&& other) noexcept;

    u64                                               id() const noexcept;
    ca::core::StatusResult<std::optional<ExitStatus>> try_wait();
    ca::core::StatusResult<ExitStatus>                wait();
    ca::core::StatusResult<std::optional<ExitStatus>> wait_for(std::chrono::milliseconds timeout);
    ca::core::Status                                  kill();

    std::optional<ChildStdin>      take_stdin();
    std::optional<ChildStdout>     take_stdout();
    std::optional<ChildStderr>     take_stderr();
    ca::core::StatusResult<Output> wait_with_output();

private:
    Child(std::intptr_t native_process, u64 process_id, std::optional<ChildStdin> stdin,
          std::optional<ChildStdout> stdout, std::optional<ChildStderr> stderr) noexcept;
    void close_process() noexcept;

    std::intptr_t              native_process_{-1};
    u64                        process_id_{0};
    std::optional<ChildStdin>  stdin_;
    std::optional<ChildStdout> stdout_;
    std::optional<ChildStderr> stderr_;

    friend class Command;
};

class Command
{
public:
    explicit Command(std::string program);

    Command& arg(std::string value);
    Command& args(std::vector<std::string> values);
    Command& current_dir(std::string path);
    Command& stdin(Stdio stdio);
    Command& stdout(Stdio stdio);
    Command& stderr(Stdio stdio);

    ca::core::StatusResult<Child>      spawn() const;
    ca::core::StatusResult<ExitStatus> status() const;
    ca::core::StatusResult<Output>     output() const;

private:
    std::string                program_;
    std::vector<std::string>   args_;
    std::optional<std::string> current_dir_;
    Stdio                      stdin_{Stdio::inherit()};
    Stdio                      stdout_{Stdio::inherit()};
    Stdio                      stderr_{Stdio::inherit()};
};

}   // namespace ca::process
