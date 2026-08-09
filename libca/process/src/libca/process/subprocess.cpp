// format.hpp 必须在 subprocess.hpp 之前：subprocess.hpp 会 undef windows.h 定义的
// stdin/stdout/stderr 宏（这些宏会破坏本模块用作标识符的同名 API），但 fmt header-only
// 模式下 format-inl.h 的 assert/异常路径引用全局 stderr/stdout。先 include format.hpp，
// 让 fmt 头在 stderr 宏仍存在时完整展开（宏在展开点已正确解析），之后 undef 不影响已展开的 fmt 代码。
#include "libca/str/format.hpp"

#include "libca/process/subprocess.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <functional>
#include <limits>
#include <iterator>
#include <thread>
#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#    undef stdin
#    undef stdout
#    undef stderr
#else
#    include <fcntl.h>
#    include <signal.h>
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace ca::process {
template<typename T>
using StatusResult = ca::core::StatusResult<T>;
using Status       = ca::core::Status;
using ca::core::Err;
using ca::core::ErrStatus;
using ca::core::Ok;
using ca::core::OkStatus;
using ca::core::StatusCode;

namespace {

Status system_error(const char* operation)
{
#if defined(_WIN32)
    return ErrStatus(StatusCode::INTERNAL,
                     ca::str::format_std("{} failed with Windows error {}",
                                         operation,
                                         static_cast<unsigned long>(GetLastError())));
#else
    return ErrStatus(StatusCode::INTERNAL,
                     ca::str::format_std("{} failed: {}", operation, std::strerror(errno)));
#endif
}

Status closed_error(const char* operation)
{
    return ErrStatus(StatusCode::FAILED_PRECONDITION,
                     ca::str::format_std("{} on a closed handle", operation));
}

#if defined(_WIN32)
HANDLE to_handle(std::intptr_t value)
{
    return reinterpret_cast<HANDLE>(value);
}

std::intptr_t to_native(HANDLE handle)
{
    return reinterpret_cast<std::intptr_t>(handle);
}

bool create_pipe(HANDLE& parent, HANDLE& child, bool parent_reads)
{
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength        = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE read_handle        = nullptr;
    HANDLE write_handle       = nullptr;
    if (!CreatePipe(&read_handle, &write_handle, &attributes, 0)) {
        return false;
    }
    HANDLE parent_handle = parent_reads ? read_handle : write_handle;
    HANDLE child_handle  = parent_reads ? write_handle : read_handle;
    if (!SetHandleInformation(parent_handle, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_handle);
        CloseHandle(write_handle);
        return false;
    }
    parent = parent_handle;
    child  = child_handle;
    return true;
}

bool utf8_to_utf16(const std::string& value, std::wstring& converted)
{
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length == 0 && !value.empty()) {
        return false;
    }
    converted.resize(static_cast<usize>(length));
    return length == 0 || MultiByteToWideChar(CP_UTF8,
                                              MB_ERR_INVALID_CHARS,
                                              value.data(),
                                              static_cast<int>(value.size()),
                                              &converted[0],
                                              length) != 0;
}

// 按 Microsoft「Parsing C++ Command-Line Arguments」规则转义单个参数：总是用双引号
// 包裹，引号前的反斜杠翻倍，紧跟引号的反斜杠再额外翻倍（故 *2+1），尾部反斜杠翻倍
// （故结尾 *2）。CreateProcessW 不做 shell 解析，子进程靠 CRT 按此规则还原 argv。
std::wstring quote_argument(const std::wstring& value)
{
    if (!value.empty() && value.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        return value;
    std::wstring output(L"\"");
    usize        slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++slashes;
        }
        else if (ch == L'\"') {
            output.append(slashes * 2 + 1, L'\\');
            output.push_back(ch);
            slashes = 0;
        }
        else {
            output.append(slashes, L'\\');
            output.push_back(ch);
            slashes = 0;
        }
    }
    output.append(slashes * 2, L'\\');
    output.push_back(L'\"');
    return output;
}

StatusResult<std::wstring> command_line(const std::string&              program_value,
                                        const std::vector<std::string>& args)
{
    std::wstring program;
    if (!utf8_to_utf16(program_value, program)) {
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "program is not valid UTF-8"));
    }
    std::wstring result = quote_argument(program);
    for (const std::string& arg : args) {
        std::wstring wide_arg;
        if (!utf8_to_utf16(arg, wide_arg)) {
            return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "argument is not valid UTF-8"));
        }
        result.push_back(L' ');
        result += quote_argument(wide_arg);
    }
    return Ok(std::move(result));
}

bool environment_key_equal(const std::wstring& left, const std::wstring& right)
{
    return CompareStringOrdinal(left.data(),
                                static_cast<int>(left.size()),
                                right.data(),
                                static_cast<int>(right.size()),
                                TRUE) == CSTR_EQUAL;
}

StatusResult<std::vector<wchar_t>> environment_block(
    const std::vector<std::pair<std::string, std::string>>& overrides)
{
    std::vector<std::pair<std::wstring, std::wstring>> values;
    wchar_t*                                           inherited = GetEnvironmentStringsW();
    if (inherited == nullptr)
        return Err(system_error("GetEnvironmentStringsW"));
    for (const wchar_t* entry = inherited; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
        const wchar_t* separator = std::wcschr(entry + (*entry == L'=' ? 1 : 0), L'=');
        if (separator == nullptr)
            continue;
        values.emplace_back(std::wstring(entry, separator), std::wstring(separator + 1));
    }
    FreeEnvironmentStringsW(inherited);

    for (const auto& [key, value] : overrides) {
        if (key.empty() || key.find('=') != std::string::npos)
            return Err(ErrStatus(StatusCode::INVALID_ARGUMENT,
                                 "environment variable key must not be empty or contain '='"));
        std::wstring wide_key;
        std::wstring wide_value;
        if (!utf8_to_utf16(key, wide_key) || !utf8_to_utf16(value, wide_value))
            return Err(ErrStatus(StatusCode::INVALID_ARGUMENT,
                                 "environment variable is not valid UTF-8"));
        const auto existing = std::find_if(values.begin(), values.end(), [&](const auto& entry) {
            return environment_key_equal(entry.first, wide_key);
        });
        if (existing == values.end())
            values.emplace_back(std::move(wide_key), std::move(wide_value));
        else
            existing->second = std::move(wide_value);
    }
    std::sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
        return CompareStringOrdinal(left.first.data(),
                                    static_cast<int>(left.first.size()),
                                    right.first.data(),
                                    static_cast<int>(right.first.size()),
                                    TRUE) == CSTR_LESS_THAN;
    });

    usize length = 1;
    for (const auto& [key, value] : values)
        length += key.size() + value.size() + 2;
    std::vector<wchar_t> block;
    block.reserve(length);
    for (const auto& [key, value] : values) {
        block.insert(block.end(), key.begin(), key.end());
        block.push_back(L'=');
        block.insert(block.end(), value.begin(), value.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return Ok(std::move(block));
}

HANDLE open_null_handle()
{
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength        = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    return CreateFileW(L"NUL",
                       GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &attributes,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}
#else
int to_fd(std::intptr_t value)
{
    return static_cast<int>(value);
}

bool create_pipe(int& parent, int& child, bool parent_reads)
{
    int descriptors[2];
    if (::pipe(descriptors) != 0) {
        return false;
    }
    parent = parent_reads ? descriptors[0] : descriptors[1];
    child  = parent_reads ? descriptors[1] : descriptors[0];
    return true;
}

void write_exec_error(int descriptor, int error) noexcept
{
    const auto* data   = reinterpret_cast<const u8*>(&error);
    usize       offset = 0;
    while (offset < sizeof(error)) {
        const ssize_t count = ::write(descriptor, data + offset, sizeof(error) - offset);
        if (count > 0) {
            offset += static_cast<usize>(count);
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        return;
    }
}
#endif

}   // namespace

namespace ipc {

PipeReader::PipeReader(std::intptr_t native_handle) noexcept
    : native_handle_(native_handle)
{}
PipeReader::~PipeReader()
{
    close();
}
PipeReader::PipeReader(PipeReader&& other) noexcept
    : native_handle_(other.release())
{}
PipeReader& PipeReader::operator=(PipeReader&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_ = other.release();
    }
    return *this;
}

StatusResult<usize> PipeReader::read(void* buffer, usize capacity)
{
    if (!is_open())
        return Err(closed_error("read"));
    if (capacity == 0)
        return Ok(static_cast<usize>(0));
#if defined(_WIN32)
    DWORD count = 0;
    if (!ReadFile(
            to_handle(native_handle_), buffer, static_cast<DWORD>(capacity), &count, nullptr)) {
        const DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE)
            return Ok(static_cast<usize>(0));
        return Err(system_error("ReadFile"));
    }
    return Ok(static_cast<usize>(count));
#else
    const ssize_t count = ::read(to_fd(native_handle_), buffer, capacity);
    if (count < 0)
        return Err(system_error("read"));
    return Ok(static_cast<usize>(count));
#endif
}

StatusResult<std::string> PipeReader::read_to_end()
{
    std::string result;
    char        buffer[4096];
    for (;;) {
        auto count = read(buffer, sizeof(buffer));
        if (count.is_err())
            return Err(count.unwrap_err());
        if (count.unwrap() == 0)
            return Ok(std::move(result));
        result.append(buffer, count.unwrap());
    }
}

bool PipeReader::is_open() const noexcept
{
    return native_handle_ != -1;
}
void PipeReader::close() noexcept
{
    if (!is_open())
        return;
#if defined(_WIN32)
    CloseHandle(to_handle(native_handle_));
#else
    ::close(to_fd(native_handle_));
#endif
    native_handle_ = -1;
}
std::intptr_t PipeReader::release() noexcept
{
    const auto value = native_handle_;
    native_handle_   = -1;
    return value;
}

PipeWriter::PipeWriter(std::intptr_t native_handle) noexcept
    : native_handle_(native_handle)
{}
PipeWriter::~PipeWriter()
{
    close();
}
PipeWriter::PipeWriter(PipeWriter&& other) noexcept
    : native_handle_(other.release())
{}
PipeWriter& PipeWriter::operator=(PipeWriter&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_ = other.release();
    }
    return *this;
}

Status PipeWriter::write_all(const void* data, usize length)
{
    if (!is_open())
        return closed_error("write");
    usize offset = 0;
    while (offset < length) {
#if defined(_WIN32)
        DWORD       written = 0;
        const DWORD chunk =
            static_cast<DWORD>(std::min<usize>(length - offset, std::numeric_limits<DWORD>::max()));
        if (!WriteFile(to_handle(native_handle_),
                       static_cast<const char*>(data) + offset,
                       chunk,
                       &written,
                       nullptr) ||
            written == 0)
            return system_error("WriteFile");
        offset += written;
#else
        const ssize_t written = ::write(
            to_fd(native_handle_), static_cast<const char*>(data) + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return system_error("write");
        }
        offset += static_cast<usize>(written);
#endif
    }
    return OkStatus();
}

Status PipeWriter::write_all(const std::string& data)
{
    return write_all(data.data(), data.size());
}
bool PipeWriter::is_open() const noexcept
{
    return native_handle_ != -1;
}
void PipeWriter::close() noexcept
{
    if (!is_open())
        return;
#if defined(_WIN32)
    CloseHandle(to_handle(native_handle_));
#else
    ::close(to_fd(native_handle_));
#endif
    native_handle_ = -1;
}
std::intptr_t PipeWriter::release() noexcept
{
    const auto value = native_handle_;
    native_handle_   = -1;
    return value;
}

StatusResult<AnonymousPipe> create_anonymous_pipe()
{
#if defined(_WIN32)
    HANDLE reader = nullptr;
    HANDLE writer = nullptr;
    if (!create_pipe(reader, writer, true))
        return Err(system_error("CreatePipe"));
    return Ok(AnonymousPipe{PipeReader(to_native(reader)), PipeWriter(to_native(writer))});
#else
    int reader = -1;
    int writer = -1;
    if (!create_pipe(reader, writer, true))
        return Err(system_error("pipe"));
    return Ok(AnonymousPipe{PipeReader(reader), PipeWriter(writer)});
#endif
}

}   // namespace ipc

Stdio::Stdio(Mode mode) noexcept
    : mode_(mode)
{}
Stdio Stdio::inherit() noexcept
{
    return Stdio(Mode::Inherit);
}
Stdio Stdio::null() noexcept
{
    return Stdio(Mode::Null);
}
Stdio Stdio::piped() noexcept
{
    return Stdio(Mode::Piped);
}
bool ExitStatus::success() const noexcept
{
    return code == 0;
}

Child::Child(std::intptr_t native_process, u64 process_id, std::optional<ChildStdin> stdin,
             std::optional<ChildStdout> stdout, std::optional<ChildStderr> stderr) noexcept
    : native_process_(native_process)
    , process_id_(process_id)
    , stdin_(std::move(stdin))
    , stdout_(std::move(stdout))
    , stderr_(std::move(stderr))
{}
Child::~Child()
{
    close_process();
}
Child::Child(Child&& other) noexcept
    : native_process_(other.native_process_)
    , process_id_(other.process_id_)
    , exit_status_(std::move(other.exit_status_))
    , stdin_(std::move(other.stdin_))
    , stdout_(std::move(other.stdout_))
    , stderr_(std::move(other.stderr_))
{
    other.native_process_ = -1;
    other.process_id_     = 0;
    other.exit_status_.reset();
}
Child& Child::operator=(Child&& other) noexcept
{
    if (this != &other) {
        close_process();
        native_process_       = other.native_process_;
        process_id_           = other.process_id_;
        exit_status_          = std::move(other.exit_status_);
        stdin_                = std::move(other.stdin_);
        stdout_               = std::move(other.stdout_);
        stderr_               = std::move(other.stderr_);
        other.native_process_ = -1;
        other.process_id_     = 0;
        other.exit_status_.reset();
    }
    return *this;
}
u64 Child::id() const noexcept
{
    return process_id_;
}
void Child::close_process() noexcept
{
    if (native_process_ == -1)
        return;
#if defined(_WIN32)
    CloseHandle(to_handle(native_process_));
#endif
    native_process_ = -1;
}

StatusResult<std::optional<ExitStatus>> Child::try_wait()
{
    if (exit_status_.has_value())
        return Ok(exit_status_);
    if (native_process_ == -1)
        return Err(closed_error("try_wait"));
#if defined(_WIN32)
    const DWORD wait = WaitForSingleObject(to_handle(native_process_), 0);
    if (wait == WAIT_TIMEOUT)
        return Ok(std::optional<ExitStatus>{});
    if (wait != WAIT_OBJECT_0)
        return Err(system_error("WaitForSingleObject"));
    DWORD code = 0;
    if (!GetExitCodeProcess(to_handle(native_process_), &code))
        return Err(system_error("GetExitCodeProcess"));
    exit_status_ = ExitStatus{static_cast<i32>(code)};
    return Ok(exit_status_);
#else
    int         status = 0;
    const pid_t result = waitpid(static_cast<pid_t>(native_process_), &status, WNOHANG);
    if (result == 0)
        return Ok(std::optional<ExitStatus>{});
    if (result < 0)
        return Err(system_error("waitpid"));
    exit_status_ = ExitStatus{WIFEXITED(status) ? static_cast<i32>(WEXITSTATUS(status))
                                                 : static_cast<i32>(128 + WTERMSIG(status))};
    return Ok(exit_status_);
#endif
}

StatusResult<ExitStatus> Child::wait()
{
    stdin_.reset();
    if (exit_status_.has_value())
        return Ok(*exit_status_);
    if (native_process_ == -1)
        return Err(closed_error("wait"));
#if defined(_WIN32)
    if (WaitForSingleObject(to_handle(native_process_), INFINITE) != WAIT_OBJECT_0)
        return Err(system_error("WaitForSingleObject"));
    DWORD code = 0;
    if (!GetExitCodeProcess(to_handle(native_process_), &code))
        return Err(system_error("GetExitCodeProcess"));
    exit_status_ = ExitStatus{static_cast<i32>(code)};
    return Ok(*exit_status_);
#else
    int status = 0;
    if (waitpid(static_cast<pid_t>(native_process_), &status, 0) < 0)
        return Err(system_error("waitpid"));
    exit_status_ = ExitStatus{WIFEXITED(status) ? static_cast<i32>(WEXITSTATUS(status))
                                                 : static_cast<i32>(128 + WTERMSIG(status))};
    return Ok(*exit_status_);
#endif
}

StatusResult<std::optional<ExitStatus>> Child::wait_for(std::chrono::milliseconds timeout)
{
    if (exit_status_.has_value())
        return Ok(exit_status_);
    if (native_process_ == -1)
        return Err(closed_error("wait_for"));
#if defined(_WIN32)
    const auto count =
        timeout.count() <= 0
            ? 0
            : static_cast<DWORD>(std::min<i64>(timeout.count(), std::numeric_limits<DWORD>::max()));
    const DWORD wait = WaitForSingleObject(to_handle(native_process_), count);
    if (wait == WAIT_TIMEOUT)
        return Ok(std::optional<ExitStatus>{});
    if (wait != WAIT_OBJECT_0)
        return Err(system_error("WaitForSingleObject"));
    DWORD code = 0;
    if (!GetExitCodeProcess(to_handle(native_process_), &code))
        return Err(system_error("GetExitCodeProcess"));
    exit_status_ = ExitStatus{static_cast<i32>(code)};
    return Ok(exit_status_);
#else
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto value = try_wait();
        if (value.is_err() || value.unwrap().has_value())
            return value;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return try_wait();
#endif
}

Status Child::kill()
{
    if (native_process_ == -1 || exit_status_.has_value())
        return closed_error("kill");
#if defined(_WIN32)
    // Windows 上没有进程组概念，直接终止单个子进程句柄。
    if (!TerminateProcess(to_handle(native_process_), 1))
        return system_error("TerminateProcess");
#else
    // Linux 子进程在新进程组内启动（见 spawn 的 setpgid），先对负 pid（整个进程组）
    // 发 SIGKILL，失败再回退到单 pid，覆盖子进程自己再 fork 的孙进程。
    const pid_t pid = static_cast<pid_t>(native_process_);
    if (::kill(-pid, SIGKILL) != 0 && ::kill(pid, SIGKILL) != 0)
        return system_error("kill");
#endif
    return OkStatus();
}

std::optional<ChildStdin> Child::take_stdin()
{
    auto result = std::move(stdin_);
    stdin_.reset();
    return result;
}
std::optional<ChildStdout> Child::take_stdout()
{
    auto result = std::move(stdout_);
    stdout_.reset();
    return result;
}
std::optional<ChildStderr> Child::take_stderr()
{
    auto result = std::move(stderr_);
    stderr_.reset();
    return result;
}

StatusResult<Output> Child::wait_with_output()
{
    stdin_.reset();
    Output      output;
    Status      stdout_status = OkStatus();
    Status      stderr_status = OkStatus();
    auto        stdout        = take_stdout();
    auto        stderr        = take_stderr();
    std::thread stdout_thread([&]() {
        if (stdout) {
            auto value = stdout->read_to_end();
            if (value.is_ok())
                output.stdout_data = value.unwrap();
            else
                stdout_status = value.unwrap_err();
        }
    });
    std::thread stderr_thread([&]() {
        if (stderr) {
            auto value = stderr->read_to_end();
            if (value.is_ok())
                output.stderr_data = value.unwrap();
            else
                stderr_status = value.unwrap_err();
        }
    });
    auto        status = wait();
    stdout_thread.join();
    stderr_thread.join();
    if (status.is_err())
        return Err(status.unwrap_err());
    if (stdout_status.is_err())
        return Err(stdout_status);
    if (stderr_status.is_err())
        return Err(stderr_status);
    output.status = status.unwrap();
    return Ok(std::move(output));
}

Command::Command(std::string program)
    : program_(std::move(program))
{}
Command& Command::arg(std::string value)
{
    args_.push_back(std::move(value));
    return *this;
}
Command& Command::args(std::vector<std::string> values)
{
    args_.insert(args_.end(),
                 std::make_move_iterator(values.begin()),
                 std::make_move_iterator(values.end()));
    return *this;
}
Command& Command::current_dir(std::string path)
{
    current_dir_ = std::move(path);
    return *this;
}
Command& Command::env(std::string key, std::string value)
{
    const auto existing = std::find_if(env_.begin(), env_.end(), [&](const auto& entry) {
        return entry.first == key;
    });
    if (existing == env_.end())
        env_.emplace_back(std::move(key), std::move(value));
    else
        existing->second = std::move(value);
    return *this;
}
Command& Command::stdin(Stdio stdio)
{
    stdin_ = stdio;
    return *this;
}
Command& Command::stdout(Stdio stdio)
{
    stdout_ = stdio;
    return *this;
}
Command& Command::stderr(Stdio stdio)
{
    stderr_ = stdio;
    return *this;
}

StatusResult<Child> Command::spawn() const
{
    if (program_.empty())
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "program must not be empty"));
    for (const auto& [key, value] : env_) {
        (void)value;
        if (key.empty() || key.find('=') != std::string::npos)
            return Err(ErrStatus(StatusCode::INVALID_ARGUMENT,
                                 "environment variable key must not be empty or contain '='"));
    }
#if defined(_WIN32)
    auto line = command_line(program_, args_);
    if (line.is_err())
        return Err(line.unwrap_err());
    std::vector<wchar_t> environment;
    if (!env_.empty()) {
        auto block = environment_block(env_);
        if (block.is_err())
            return Err(block.unwrap_err());
        environment = std::move(block).unwrap();
    }
    std::wstring   working;
    const wchar_t* working_ptr = nullptr;
    if (current_dir_ && !utf8_to_utf16(*current_dir_, working))
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "current_dir is not valid UTF-8"));
    if (current_dir_)
        working_ptr = working.c_str();

    HANDLE parent_in = nullptr, parent_out = nullptr, parent_err = nullptr, child_in = nullptr,
           child_out = nullptr, child_err = nullptr;
    HANDLE null_in = nullptr, null_out = nullptr, null_err = nullptr;
    const auto close_handle = [](HANDLE& handle) {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        handle = nullptr;
    };
    const auto cleanup = [&]() {
        close_handle(parent_in);
        close_handle(parent_out);
        close_handle(parent_err);
        close_handle(child_in);
        close_handle(child_out);
        close_handle(child_err);
        close_handle(null_in);
        close_handle(null_out);
        close_handle(null_err);
    };
    if (stdin_.mode_ == Stdio::Mode::Piped && !create_pipe(parent_in, child_in, false)) {
        const auto error = system_error("CreatePipe");
        cleanup();
        return Err(error);
    }
    if (stdout_.mode_ == Stdio::Mode::Piped && !create_pipe(parent_out, child_out, true)) {
        const auto error = system_error("CreatePipe");
        cleanup();
        return Err(error);
    }
    if (stderr_.mode_ == Stdio::Mode::Piped && !create_pipe(parent_err, child_err, true)) {
        const auto error = system_error("CreatePipe");
        cleanup();
        return Err(error);
    }
    if (stdin_.mode_ == Stdio::Mode::Null) {
        null_in = open_null_handle();
        if (null_in == INVALID_HANDLE_VALUE) {
            const auto error = system_error("CreateFileW");
            cleanup();
            return Err(error);
        }
    }
    if (stdout_.mode_ == Stdio::Mode::Null) {
        null_out = open_null_handle();
        if (null_out == INVALID_HANDLE_VALUE) {
            const auto error = system_error("CreateFileW");
            cleanup();
            return Err(error);
        }
    }
    if (stderr_.mode_ == Stdio::Mode::Null) {
        null_err = open_null_handle();
        if (null_err == INVALID_HANDLE_VALUE) {
            const auto error = system_error("CreateFileW");
            cleanup();
            return Err(error);
        }
    }
    STARTUPINFOW startup{};
    startup.cb        = sizeof(startup);
    startup.dwFlags   = STARTF_USESTDHANDLES;
    startup.hStdInput = child_in ? child_in : (null_in ? null_in : GetStdHandle(STD_INPUT_HANDLE));
    startup.hStdOutput =
        child_out ? child_out : (null_out ? null_out : GetStdHandle(STD_OUTPUT_HANDLE));
    startup.hStdError =
        child_err ? child_err : (null_err ? null_err : GetStdHandle(STD_ERROR_HANDLE));
    std::wstring         text = std::move(line).unwrap();
    std::vector<wchar_t> mutable_line(text.begin(), text.end());
    mutable_line.push_back(L'\0');
    PROCESS_INFORMATION info{};
    const DWORD creation_flags =
        env_.empty() ? CREATE_NO_WINDOW : CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
    if (!CreateProcessW(nullptr,
                        mutable_line.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        creation_flags,
                        environment.empty() ? nullptr : environment.data(),
                        working_ptr,
                        &startup,
                        &info)) {
        const auto error = system_error("CreateProcessW");
        cleanup();
        return Err(error);
    }
    close_handle(child_in);
    close_handle(child_out);
    close_handle(child_err);
    close_handle(null_in);
    close_handle(null_out);
    close_handle(null_err);
    CloseHandle(info.hThread);
    return Ok(Child(
        to_native(info.hProcess),
        static_cast<u64>(info.dwProcessId),
        parent_in ? std::optional<ChildStdin>(ChildStdin(to_native(parent_in))) : std::nullopt,
        parent_out ? std::optional<ChildStdout>(ChildStdout(to_native(parent_out))) : std::nullopt,
        parent_err ? std::optional<ChildStderr>(ChildStderr(to_native(parent_err)))
                   : std::nullopt));
#else
    int parent_in = -1;
    int parent_out = -1;
    int parent_err = -1;
    int child_in = -1;
    int child_out = -1;
    int child_err = -1;
    int exec_read = -1;
    int exec_write = -1;
    const auto close_descriptor = [](int& descriptor) {
        if (descriptor >= 0)
            ::close(descriptor);
        descriptor = -1;
    };
    const auto cleanup = [&]() {
        close_descriptor(parent_in);
        close_descriptor(parent_out);
        close_descriptor(parent_err);
        close_descriptor(child_in);
        close_descriptor(child_out);
        close_descriptor(child_err);
        close_descriptor(exec_read);
        close_descriptor(exec_write);
    };
    if ((stdin_.mode_ == Stdio::Mode::Piped && !create_pipe(parent_in, child_in, false)) ||
        (stdout_.mode_ == Stdio::Mode::Piped && !create_pipe(parent_out, child_out, true)) ||
        (stderr_.mode_ == Stdio::Mode::Piped && !create_pipe(parent_err, child_err, true)) ||
        !create_pipe(exec_read, exec_write, true)) {
        const auto error = system_error("pipe");
        cleanup();
        return Err(error);
    }
    // exec 错误自管道：子进程 execvp 失败时把 errno 写进 exec_write，父进程从 exec_read
    // 读到即说明 exec 失败；exec 成功则 exec_write 因 FD_CLOEXEC 自动关闭，read 返回 0。
    if (fcntl(exec_write, F_SETFD, FD_CLOEXEC) != 0) {
        const auto error = system_error("fcntl");
        cleanup();
        return Err(error);
    }

    std::vector<std::string> values;
    values.reserve(args_.size() + 1);
    values.push_back(program_);
    values.insert(values.end(), args_.begin(), args_.end());
    std::vector<char*> argv;
    argv.reserve(values.size() + 1);
    for (std::string& value : values) argv.push_back(value.data());
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        const auto error = system_error("fork");
        cleanup();
        return Err(error);
    }
    if (pid == 0) {
        // 子进程放入独立进程组，使父进程 kill() 能针对整个组（覆盖孙进程），避免孤儿。
        setpgid(0, 0);
        if (current_dir_ && chdir(current_dir_->c_str()) != 0) {
            const int error = errno;
            write_exec_error(exec_write, error);
            _exit(127);
        }
        const auto set_stream = [](Stdio::Mode mode, int descriptor, int target, int access) -> bool {
            if (mode == Stdio::Mode::Piped) return dup2(descriptor, target) >= 0;
            if (mode == Stdio::Mode::Null) {
                const int null_fd = open("/dev/null", access);
                if (null_fd < 0) return false;
                const bool ok = dup2(null_fd, target) >= 0;
                ::close(null_fd);
                return ok;
            }
            return true;
        };
        if (!set_stream(stdin_.mode_, child_in, STDIN_FILENO, O_RDONLY) ||
            !set_stream(stdout_.mode_, child_out, STDOUT_FILENO, O_WRONLY) ||
            !set_stream(stderr_.mode_, child_err, STDERR_FILENO, O_WRONLY)) {
            const int error = errno;
            write_exec_error(exec_write, error);
            _exit(127);
        }
        for (int descriptor : {parent_in, parent_out, parent_err, child_in, child_out, child_err, exec_read}) {
            if (descriptor >= 0 && descriptor > STDERR_FILENO) ::close(descriptor);
        }
        for (const auto& [key, value] : env_) {
            if (setenv(key.c_str(), value.c_str(), 1) != 0) {
                const int error = errno;
                write_exec_error(exec_write, error);
                _exit(127);
            }
        }
        execvp(argv[0], argv.data());
        const int error = errno;
        write_exec_error(exec_write, error);
        _exit(127);
    }

    setpgid(pid, pid);
    close_descriptor(child_in);
    close_descriptor(child_out);
    close_descriptor(child_err);
    close_descriptor(exec_write);
    int exec_error = 0;
    ssize_t exec_result = -1;
    do {
        exec_result = ::read(exec_read, &exec_error, sizeof(exec_error));
    } while (exec_result < 0 && errno == EINTR);
    close_descriptor(exec_read);
    if (exec_result == static_cast<ssize_t>(sizeof(exec_error))) {
        close_descriptor(parent_in);
        close_descriptor(parent_out);
        close_descriptor(parent_err);
        waitpid(pid, nullptr, 0);
        return Err(ErrStatus(StatusCode::NOT_FOUND,
                             std::string("execvp failed: ") + std::strerror(exec_error)));
    }
    if (exec_result < 0) {
        const auto error = system_error("read exec status");
        ::kill(-pid, SIGKILL);
        ::kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        cleanup();
        return Err(error);
    }
    return Ok(Child(static_cast<std::intptr_t>(pid), static_cast<u64>(pid),
                    parent_in >= 0 ? std::optional<ChildStdin>(ChildStdin(parent_in)) : std::nullopt,
                    parent_out >= 0 ? std::optional<ChildStdout>(ChildStdout(parent_out)) : std::nullopt,
                    parent_err >= 0 ? std::optional<ChildStderr>(ChildStderr(parent_err)) : std::nullopt));
#endif
}

StatusResult<ExitStatus> Command::status() const
{
    auto child = spawn();
    if (child.is_err())
        return Err(child.unwrap_err());
    return std::move(child).unwrap().wait();
}
StatusResult<Output> Command::output() const
{
    Command copy(*this);
    copy.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
    auto child = copy.spawn();
    if (child.is_err())
        return Err(child.unwrap_err());
    return std::move(child).unwrap().wait_with_output();
}

}   // namespace ca::process
