#include "libca/process/subprocess.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <thread>
#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <fcntl.h>
#    include <pthread.h>
#    include <signal.h>
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace ca::process {
namespace {

SubprocessResult start_error(std::string message)
{
    SubprocessResult result;
    result.stderr_data = std::move(message);
    return result;
}

#if defined(_WIN32)

class WindowsHandle
{
public:
    WindowsHandle() = default;
    explicit WindowsHandle(HANDLE handle)
        : handle_(handle)
    {}

    ~WindowsHandle() { reset(); }

    WindowsHandle(const WindowsHandle&)            = delete;
    WindowsHandle& operator=(const WindowsHandle&) = delete;

    WindowsHandle(WindowsHandle&& other) noexcept
        : handle_(other.release())
    {}

    WindowsHandle& operator=(WindowsHandle&& other) noexcept
    {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HANDLE get() const { return handle_; }

    HANDLE release()
    {
        HANDLE handle = handle_;
        handle_       = nullptr;
        return handle;
    }

    void reset(HANDLE handle = nullptr)
    {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_{nullptr};
};

bool create_capture_pipe(WindowsHandle& parent_handle, WindowsHandle& child_handle,
                         bool parent_reads)
{
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength        = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE read_handle  = nullptr;
    HANDLE write_handle = nullptr;
    if (!CreatePipe(&read_handle, &write_handle, &attributes, 0)) {
        return false;
    }

    if (parent_reads) {
        parent_handle.reset(read_handle);
        child_handle.reset(write_handle);
    }
    else {
        child_handle.reset(read_handle);
        parent_handle.reset(write_handle);
    }

    return SetHandleInformation(parent_handle.get(), HANDLE_FLAG_INHERIT, 0) != 0;
}

std::string windows_error_message(const char* operation)
{
    return std::string(operation) + " failed with Windows error " +
           std::to_string(static_cast<unsigned long>(GetLastError()));
}

bool utf8_to_utf16(const std::string& value, std::wstring& converted)
{
    const auto length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length == 0 && !value.empty()) {
        return false;
    }

    converted.resize(static_cast<std::size_t>(length));
    return length == 0 || MultiByteToWideChar(CP_UTF8,
                                              MB_ERR_INVALID_CHARS,
                                              value.data(),
                                              static_cast<int>(value.size()),
                                              converted.data(),
                                              length) != 0;
}

std::wstring quote_windows_argument(const std::wstring& argument)
{
    std::wstring quoted;
    quoted.push_back(L'\"');

    std::size_t slash_count = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++slash_count;
            continue;
        }
        if (character == L'\"') {
            quoted.append(slash_count * 2 + 1, L'\\');
            quoted.push_back(L'\"');
            slash_count = 0;
            continue;
        }
        quoted.append(slash_count, L'\\');
        slash_count = 0;
        quoted.push_back(character);
    }
    quoted.append(slash_count * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

bool build_windows_command_line(const SubprocessOptions& options, std::wstring& command_line)
{
    std::wstring executable;
    if (!utf8_to_utf16(options.executable, executable)) {
        return false;
    }

    command_line = quote_windows_argument(executable);
    for (const std::string& argument : options.args) {
        std::wstring wide_argument;
        if (!utf8_to_utf16(argument, wide_argument)) {
            return false;
        }
        command_line.push_back(L' ');
        command_line += quote_windows_argument(wide_argument);
    }
    return true;
}

void read_windows_pipe(HANDLE handle, std::string& output)
{
    char buffer[4096];
    for (;;) {
        DWORD read_count = 0;
        if (!ReadFile(handle, buffer, sizeof(buffer), &read_count, nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                return;
            }
            return;
        }
        if (read_count == 0) {
            return;
        }
        output.append(buffer, read_count);
    }
}

void write_windows_pipe(HANDLE handle, const std::string& input)
{
    std::size_t offset = 0;
    while (offset < input.size()) {
        const auto  remaining = input.size() - offset;
        const DWORD count     = static_cast<DWORD>(std::min<std::size_t>(
            remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD       written   = 0;
        if (!WriteFile(handle, input.data() + offset, count, &written, nullptr) || written == 0) {
            return;
        }
        offset += written;
    }
}

SubprocessResult run_windows(const SubprocessOptions& options)
{
    std::wstring command_line;
    if (!build_windows_command_line(options, command_line)) {
        return start_error("executable or argument is not valid UTF-8");
    }

    std::wstring   working_dir;
    const wchar_t* working_dir_ptr = nullptr;
    if (options.working_dir.has_value()) {
        if (!utf8_to_utf16(*options.working_dir, working_dir)) {
            return start_error("working_dir is not valid UTF-8");
        }
        working_dir_ptr = working_dir.c_str();
    }

    WindowsHandle stdout_parent;
    WindowsHandle stdout_child;
    WindowsHandle stderr_parent;
    WindowsHandle stderr_child;
    WindowsHandle stdin_parent;
    WindowsHandle stdin_child;

    if (options.capture_stdout && !create_capture_pipe(stdout_parent, stdout_child, true)) {
        return start_error(windows_error_message("CreatePipe for stdout"));
    }
    if (options.capture_stderr && !create_capture_pipe(stderr_parent, stderr_child, true)) {
        return start_error(windows_error_message("CreatePipe for stderr"));
    }
    if (options.stdin_data.has_value() && !create_capture_pipe(stdin_parent, stdin_child, false)) {
        return start_error(windows_error_message("CreatePipe for stdin"));
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    if (options.capture_stdout || options.capture_stderr || options.stdin_data.has_value()) {
        startup_info.dwFlags |= STARTF_USESTDHANDLES;
        startup_info.hStdInput =
            options.stdin_data.has_value() ? stdin_child.get() : GetStdHandle(STD_INPUT_HANDLE);
        startup_info.hStdOutput =
            options.capture_stdout ? stdout_child.get() : GetStdHandle(STD_OUTPUT_HANDLE);
        startup_info.hStdError =
            options.capture_stderr ? stderr_child.get() : GetStdHandle(STD_ERROR_HANDLE);
    }

    PROCESS_INFORMATION  process_info{};
    std::vector<wchar_t> mutable_command_line(command_line.begin(), command_line.end());
    mutable_command_line.push_back(L'\0');
    if (!CreateProcessW(nullptr,
                        mutable_command_line.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        working_dir_ptr,
                        &startup_info,
                        &process_info)) {
        return start_error(windows_error_message("CreateProcessW"));
    }

    WindowsHandle process_handle(process_info.hProcess);
    WindowsHandle thread_handle(process_info.hThread);
    stdout_child.reset();
    stderr_child.reset();
    stdin_child.reset();

    SubprocessResult result;
    std::thread      stdout_thread;
    std::thread      stderr_thread;
    std::thread      stdin_thread;
    if (options.capture_stdout) {
        stdout_thread =
            std::thread(read_windows_pipe, stdout_parent.get(), std::ref(result.stdout_data));
    }
    if (options.capture_stderr) {
        stderr_thread =
            std::thread(read_windows_pipe, stderr_parent.get(), std::ref(result.stderr_data));
    }
    if (options.stdin_data.has_value()) {
        const HANDLE input_handle = stdin_parent.release();
        stdin_thread              = std::thread([input_handle, input = *options.stdin_data]() {
            write_windows_pipe(input_handle, input);
            CloseHandle(input_handle);
        });
    }

    DWORD timeout = INFINITE;
    if (options.timeout.has_value()) {
        const auto milliseconds = options.timeout->count();
        timeout =
            milliseconds <= 0
                ? 0
                : static_cast<DWORD>(std::min<std::int64_t>(
                      milliseconds, static_cast<std::int64_t>(std::numeric_limits<DWORD>::max())));
    }

    const DWORD wait_result = WaitForSingleObject(process_handle.get(), timeout);
    std::string supervisor_error;
    if (wait_result == WAIT_TIMEOUT) {
        result.timed_out = true;
        TerminateProcess(process_handle.get(), 1);
        WaitForSingleObject(process_handle.get(), INFINITE);
    }
    else if (wait_result == WAIT_FAILED) {
        supervisor_error = windows_error_message("WaitForSingleObject");
        TerminateProcess(process_handle.get(), 1);
        WaitForSingleObject(process_handle.get(), INFINITE);
    }

    if (stdout_thread.joinable()) {
        stdout_thread.join();
    }
    if (stderr_thread.joinable()) {
        stderr_thread.join();
    }
    if (stdin_thread.joinable()) {
        stdin_thread.join();
    }
    result.stderr_data += supervisor_error;

    if (result.timed_out) {
        result.exit_code = -1;
        return result;
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_handle.get(), &exit_code)) {
        result.stderr_data += windows_error_message("GetExitCodeProcess");
        return result;
    }
    result.exit_code = static_cast<i32>(exit_code);
    return result;
}

#else

class FileDescriptor
{
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int descriptor)
        : descriptor_(descriptor)
    {}

    ~FileDescriptor() { reset(); }

    FileDescriptor(const FileDescriptor&)            = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    int get() const { return descriptor_; }

    int release()
    {
        const int descriptor = descriptor_;
        descriptor_          = -1;
        return descriptor;
    }

    void reset(int descriptor = -1)
    {
        if (descriptor_ >= 0) {
            close(descriptor_);
        }
        descriptor_ = descriptor;
    }

private:
    int descriptor_{-1};
};

bool create_pipe(FileDescriptor& read_end, FileDescriptor& write_end)
{
    int descriptors[2]{};
    if (pipe(descriptors) != 0) {
        return false;
    }
    read_end.reset(descriptors[0]);
    write_end.reset(descriptors[1]);
    return true;
}

std::string posix_error_message(const char* operation, int error)
{
    return std::string(operation) + " failed: " + std::strerror(error);
}

void read_posix_pipe(int descriptor, std::string& output)
{
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(descriptor, buffer, sizeof(buffer));
        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        close(descriptor);
        return;
    }
}

void write_posix_pipe(int descriptor, const std::string& input)
{
    sigset_t blocked_signals;
    sigemptyset(&blocked_signals);
    sigaddset(&blocked_signals, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &blocked_signals, nullptr);

    std::size_t offset = 0;
    while (offset < input.size()) {
        const ssize_t count = write(descriptor, input.data() + offset, input.size() - offset);
        if (count > 0) {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    close(descriptor);
}

void write_exec_error(int descriptor, int error)
{
    const char* bytes  = reinterpret_cast<const char*>(&error);
    std::size_t offset = 0;
    while (offset < sizeof(error)) {
        const ssize_t count = write(descriptor, bytes + offset, sizeof(error) - offset);
        if (count > 0) {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        return;
    }
}

SubprocessResult run_posix(const SubprocessOptions& options)
{
    FileDescriptor stdout_read;
    FileDescriptor stdout_write;
    FileDescriptor stderr_read;
    FileDescriptor stderr_write;
    FileDescriptor stdin_read;
    FileDescriptor stdin_write;
    FileDescriptor exec_error_read;
    FileDescriptor exec_error_write;

    if ((options.capture_stdout && !create_pipe(stdout_read, stdout_write)) ||
        (options.capture_stderr && !create_pipe(stderr_read, stderr_write)) ||
        (options.stdin_data.has_value() && !create_pipe(stdin_read, stdin_write)) ||
        !create_pipe(exec_error_read, exec_error_write)) {
        return start_error(posix_error_message("pipe", errno));
    }
    if (fcntl(exec_error_write.get(), F_SETFD, FD_CLOEXEC) != 0) {
        return start_error(posix_error_message("fcntl", errno));
    }

    std::vector<std::string> command;
    command.reserve(options.args.size() + 1);
    command.push_back(options.executable);
    command.insert(command.end(), options.args.begin(), options.args.end());
    std::vector<char*> arguments;
    arguments.reserve(command.size() + 1);
    for (std::string& argument : command) {
        arguments.push_back(argument.data());
    }
    arguments.push_back(nullptr);

    const pid_t child_pid = fork();
    if (child_pid < 0) {
        return start_error(posix_error_message("fork", errno));
    }
    if (child_pid == 0) {
        setpgid(0, 0);
        if (options.working_dir.has_value() && chdir(options.working_dir->c_str()) != 0) {
            write_exec_error(exec_error_write.get(), errno);
            _exit(127);
        }
        if (options.stdin_data.has_value() && dup2(stdin_read.get(), STDIN_FILENO) < 0) {
            write_exec_error(exec_error_write.get(), errno);
            _exit(127);
        }
        if (options.capture_stdout && dup2(stdout_write.get(), STDOUT_FILENO) < 0) {
            write_exec_error(exec_error_write.get(), errno);
            _exit(127);
        }
        if (options.capture_stderr && dup2(stderr_write.get(), STDERR_FILENO) < 0) {
            write_exec_error(exec_error_write.get(), errno);
            _exit(127);
        }

        stdout_read.reset();
        stdout_write.reset();
        stderr_read.reset();
        stderr_write.reset();
        stdin_read.reset();
        stdin_write.reset();
        exec_error_read.reset();
        execvp(arguments[0], arguments.data());
        write_exec_error(exec_error_write.get(), errno);
        _exit(127);
    }

    setpgid(child_pid, child_pid);
    stdout_write.reset();
    stderr_write.reset();
    stdin_read.reset();
    exec_error_write.reset();

    SubprocessResult result;
    std::thread      stdout_thread;
    std::thread      stderr_thread;
    std::thread      stdin_thread;
    if (options.capture_stdout) {
        stdout_thread =
            std::thread(read_posix_pipe, stdout_read.release(), std::ref(result.stdout_data));
    }
    if (options.capture_stderr) {
        stderr_thread =
            std::thread(read_posix_pipe, stderr_read.release(), std::ref(result.stderr_data));
    }
    if (options.stdin_data.has_value()) {
        stdin_thread = std::thread(write_posix_pipe, stdin_write.release(), *options.stdin_data);
    }

    int         wait_status = 0;
    bool        reaped      = false;
    std::string supervisor_error;
    const auto  deadline = options.timeout.has_value()
                               ? std::chrono::steady_clock::now() + *options.timeout
                               : std::chrono::steady_clock::time_point::max();
    while (!reaped) {
        const pid_t wait_result = waitpid(child_pid, &wait_status, WNOHANG);
        if (wait_result == child_pid) {
            reaped = true;
            break;
        }
        if (wait_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            supervisor_error = posix_error_message("waitpid", errno);
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.timed_out = true;
            if (kill(-child_pid, SIGKILL) != 0) {
                const int kill_error = errno;
                if (kill(child_pid, SIGKILL) != 0 && errno != ESRCH) {
                    supervisor_error = posix_error_message("kill", kill_error);
                }
            }
            while (waitpid(child_pid, &wait_status, 0) < 0 && errno == EINTR) {}
            reaped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (stdout_thread.joinable()) {
        stdout_thread.join();
    }
    if (stderr_thread.joinable()) {
        stderr_thread.join();
    }
    if (stdin_thread.joinable()) {
        stdin_thread.join();
    }
    result.stderr_data += supervisor_error;

    int           exec_error       = 0;
    const ssize_t exec_error_count = read(exec_error_read.get(), &exec_error, sizeof(exec_error));
    if (exec_error_count == static_cast<ssize_t>(sizeof(exec_error))) {
        result.stderr_data += posix_error_message("execvp", exec_error);
        result.exit_code = -1;
        return result;
    }
    if (result.timed_out) {
        result.exit_code = -1;
        return result;
    }
    if (reaped && WIFEXITED(wait_status)) {
        result.exit_code = static_cast<i32>(WEXITSTATUS(wait_status));
    }
    else if (reaped && WIFSIGNALED(wait_status)) {
        result.exit_code = static_cast<i32>(128 + WTERMSIG(wait_status));
    }
    return result;
}

#endif

}   // namespace

bool SubprocessResult::succeeded() const noexcept
{
    return !timed_out && exit_code == 0;
}

SubprocessResult run(const SubprocessOptions& options)
{
    if (options.executable.empty()) {
        return start_error("executable must not be empty");
    }

#if defined(_WIN32)
    return run_windows(options);
#else
    return run_posix(options);
#endif
}

}   // namespace ca::process
