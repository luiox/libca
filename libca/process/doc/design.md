# libca process and IPC design

## Scope

`libca_process` is a cross-platform process lifecycle and IPC library. It has
two layers:

1. `ca::process` follows the ownership and builder model of Rust
   `std::process`: build a command, spawn a child, then interact with its
   standard streams and lifecycle.
2. `ca::process::ipc` provides independently usable anonymous pipes, named
   pipes, shared memory, message queues, and named semaphores. These are not
   hidden implementation details of a one-shot launcher.

The library supports Windows and Linux. Public types use `CamelCase`; methods
and fields use `snake_case`.

## Process API

```cpp
namespace ca::process {

class Stdio {
public:
    static Stdio inherit();
    static Stdio null();
    static Stdio piped();
};

class Command {
public:
    explicit Command(std::string program);

    Command& arg(std::string value);
    Command& args(std::vector<std::string> values);
    Command& current_dir(std::string path);
    Command& env(std::string key, std::string value);
    Command& env_remove(std::string key);
    Command& env_clear();
    Command& stdin(Stdio stdio);
    Command& stdout(Stdio stdio);
    Command& stderr(Stdio stdio);

    StatusResult<Child> spawn() const;
    StatusResult<ExitStatus> status() const;
    StatusResult<Output> output() const;
};

class Child {
public:
    Child(Child&&) noexcept;
    Child& operator=(Child&&) noexcept;
    ~Child();

    u64 id() const noexcept;
    StatusResult<std::optional<ExitStatus>> try_wait();
    StatusResult<ExitStatus> wait();
    StatusResult<std::optional<ExitStatus>> wait_for(std::chrono::milliseconds timeout);
    Status kill();

    std::optional<ChildStdin> take_stdin();
    std::optional<ChildStdout> take_stdout();
    std::optional<ChildStderr> take_stderr();
    StatusResult<Output> wait_with_output();
};

struct ExitStatus {
    i32 code = -1;
    bool success() const noexcept;
};

struct Output {
    ExitStatus status;
    std::string stdout_data;
    std::string stderr_data;
};

} // namespace ca::process
```

`Command` is reusable. Its defaults match Rust: `spawn()` and `status()`
inherit standard streams, while `output()` overrides stdout and stderr to
`Stdio::piped()` and closes stdin before waiting.

`Child` is move-only. Its destructor does not silently kill a running child;
it releases only native handles. Callers that need containment must call
`kill()` followed by `wait()`. `kill()` terminates the child process group on
Linux and the child on Windows. The returned stream ownership is explicit:
`take_stdout()` moves the read end out of `Child`, so it cannot be read twice.

`wait_with_output()` takes the remaining stdout and stderr streams and drains
them concurrently before returning. This is the safe convenience API for
commands that can fill pipe buffers. Interactive callers use `take_stdin()`,
`take_stdout()`, and `take_stderr()` and are responsible for concurrent
draining when both output streams can be active.

Example:

```cpp
Command command("ls");
command.arg("-l").stdout(Stdio::piped()).stderr(Stdio::piped());
auto child = std::move(command.spawn().unwrap());
auto output = child.wait_with_output();
```

## Anonymous Pipe API

```cpp
namespace ca::process::ipc {

class PipeReader {
public:
    StatusResult<usize> read(void* buffer, usize capacity);
    StatusResult<std::string> read_to_end();
    void close() noexcept;
};

class PipeWriter {
public:
    Status write_all(const void* data, usize length);
    Status write_all(const std::string& data);
    void close() noexcept;
};

struct AnonymousPipe {
    PipeReader reader;
    PipeWriter writer;
};

StatusResult<AnonymousPipe> create_anonymous_pipe();

} // namespace ca::process::ipc
```

`ChildStdin`, `ChildStdout`, and `ChildStderr` are directional wrappers around
these move-only endpoints. Standard-stream pipes are therefore ordinary IPC
resources, not an implementation-private `std::string` collector.

## Named IPC API

```cpp
namespace ca::process::ipc {

class NamedPipeServer {
public:
    static StatusResult<NamedPipeServer> create(const std::string& name);
    StatusResult<NamedPipeConnection> accept();
};

class NamedPipeClient {
public:
    static StatusResult<NamedPipeConnection> connect(const std::string& name);
};

class NamedPipeConnection {
public:
    StatusResult<usize> read(void* buffer, usize capacity);
    Status write_all(const void* data, usize length);
    void close() noexcept;
};

class SharedMemory {
public:
    static StatusResult<SharedMemory> create(const std::string& name, usize size);
    static StatusResult<SharedMemory> open(const std::string& name);
    void* data() noexcept;
    const void* data() const noexcept;
    usize size() const noexcept;
    void close() noexcept;
};

class MessageQueue {
public:
    static StatusResult<MessageQueue> create(const std::string& name, usize max_message_size);
    static StatusResult<MessageQueue> open(const std::string& name);
    Status send(const void* data, usize length);
    StatusResult<std::string> receive();
    StatusResult<std::optional<std::string>> receive_for(
        std::chrono::milliseconds timeout);
    void close() noexcept;
};

class NamedSemaphore {
public:
    static StatusResult<NamedSemaphore> create(const std::string& name, u32 initial_count);
    static StatusResult<NamedSemaphore> open(const std::string& name);
    Status acquire();
    StatusResult<bool> try_acquire_for(std::chrono::milliseconds timeout);
    Status release(u32 count = 1);
    void close() noexcept;
};

} // namespace ca::process::ipc
```

All named resources are move-only. `close()` releases only the local handle;
creation/unlink policy is explicit in the platform implementation and never
silently removes an object opened by another process.

## Platform Mapping

| Primitive | Windows | Linux |
| --- | --- | --- |
| Child and standard pipes | `CreateProcessW`, `CreatePipe`, `ReadFile`, `WriteFile` | `fork`, `execve`, `pipe`, `read`, `write` |
| Child termination | `TerminateProcess` | process group `SIGKILL`, then `waitpid` |
| Named pipe | `CreateNamedPipeW` | `AF_UNIX` stream socket |
| Shared memory | `CreateFileMappingW` / `MapViewOfFile` | `shm_open` / `mmap` |
| Message queue | shared-memory ring plus named semaphores | POSIX `mq_open` |
| Named semaphore | `CreateSemaphoreW` | `sem_open` |

Windows message queues use a fixed-size framed ring stored in `SharedMemory`
and synchronized by two `NamedSemaphore` objects and one named mutex. Linux
uses POSIX message queues directly. Both reject sends larger than
`max_message_size` and preserve whole-message boundaries.

## Error, Timeout, and Test Semantics

All fallible operations return existing `StatusResult<T>` or `Status`; process
nonzero exit remains data in `ExitStatus`, not an API error. `NOT_FOUND`,
`ALREADY_EXISTS`, `PERMISSION_DENIED`, `RESOURCE_EXHAUSTED`, `DEADLINE_EXCEEDED`
and `INTERNAL` map platform diagnostics to stable error classes.

Tests must cover command reuse, argument boundaries, interactive stdin/stdout,
separate stderr draining, nonzero status, timeout/kill/reap, and each IPC
resource's create/open/read-write/close path. Linux-only and Windows-only
coverage is separated where native semantics differ.
