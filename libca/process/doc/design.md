# libca process design

## Goal

`libca_process` provides cross-platform child-process control and the IPC
resources needed to communicate with a child or an unrelated process. It is a
separate library, like `libca_fs`, and depends only on `libca_core`.

The public API follows the ownership shape of Rust `std::process`: configure a
reusable `Command`, call `spawn()` to obtain a move-only `Child`, then own its
standard-stream endpoints and lifecycle explicitly. Public types use
`CamelCase`; methods and fields use `snake_case`.

## Process API

```cpp
Command command("program");
command.arg("--flag").stdin(Stdio::piped()).stdout(Stdio::piped());

auto child = std::move(command.spawn().unwrap());
auto input = child.take_stdin();
input->write_all("request\\n");
input->close();
auto output = child.wait_with_output();
```

`Command` owns the executable, arguments, optional current directory, and the
three `Stdio` configurations. `arg()` appends one exact argument; no shell is
started and no command-line string is parsed by a shell. A `Command` can be
used for multiple `spawn()`, `status()`, or `output()` calls.

`Stdio::inherit()` is the default. `Stdio::null()` attaches the platform null
device. `Stdio::piped()` creates an anonymous pipe and makes the parent end
available through `ChildStdin`, `ChildStdout`, or `ChildStderr`.

`Child` is move-only. `try_wait()` returns an empty optional while the child
is running; `wait()` reaps it; `wait_for()` returns an empty optional after a
deadline without killing it. `kill()` terminates the child, and callers then
call `wait()` to reap it. Destruction closes owned endpoints and native handles
but does not kill a running child. On Linux, the process is launched in a new
process group, so `kill()` targets the group. On Windows it targets the child
process handle.

`wait_with_output()` closes any owned stdin and concurrently drains the owned
stdout and stderr pipes before returning. This avoids a deadlock when a child
writes enough data to fill either pipe. Interactive users that take both read
ends must drain them concurrently themselves.

`ExitStatus::code` holds a normal exit code or a platform-derived termination
code. A nonzero exit code is valid process data, not a `Status` error.
Operational failures return `StatusResult<T>` or `Status`.

## IPC API

`ipc::PipeReader` and `ipc::PipeWriter` are move-only anonymous-pipe
endpoints. `read_to_end()` reads until the last writer closes; `write_all()`
retries partial writes. These same endpoint types back the standard streams.

The independent named resources are also move-only:

| API | Windows | Linux |
| --- | --- | --- |
| `NamedPipeServer` / `NamedPipeClient` | Win32 named pipe | Unix-domain stream socket |
| `SharedMemory` | file mapping and view | `shm_open` and `mmap` |
| `NamedSemaphore` | named semaphore handle | `sem_open` |
| `MessageQueue` | mailslot receiver/sender | POSIX message queue |

All named resource names are simple tokens. The implementation supplies its
own platform namespace prefix, preventing callers from injecting a filesystem
path or a Win32 namespace path. `create()` fails with `ALREADY_EXISTS`; `open()`
fails with `NOT_FOUND` when the resource is absent. `close()` releases only
the local handle or mapping. It never removes a name that another process may
still be using.

Message queues preserve one-message boundaries. On Windows they are
intentionally one-way: `create()` returns the receiver and `open()` returns a
sender. On Linux an opened queue can send and receive. A message larger than
the configured maximum is rejected. `receive_for()` returns an empty optional
on timeout, allowing callers to distinguish it from operational failure.

## Error and Platform Rules

Platform diagnostics are converted to `ca::core::Status` with stable codes:
`INVALID_ARGUMENT`, `NOT_FOUND`, `ALREADY_EXISTS`, `FAILED_PRECONDITION`,
`DEADLINE_EXCEEDED`, `OUT_OF_RANGE`, and `INTERNAL`. Public APIs do not throw
for expected operating-system failures.

Windows process creation uses `CreateProcessW` with UTF-8 to UTF-16 conversion
and correctly quoted arguments. Linux uses `fork`, `execvp`, pipes, and
`waitpid`. Linux additionally links `pthread` for concurrent stream draining
and `rt` for POSIX message queues on toolchains that require it.

## Test Strategy

The unit suite covers exact argument boundaries, command reuse, nonzero exit,
interactive stdin/stdout, timeout observation followed by kill and reap, and
concurrent stdout/stderr collection. IPC tests cover anonymous-pipe transfer,
named-pipe exchange, shared-memory visibility, timed semaphore acquisition,
and whole-message queue delivery. Platform-specific tests remain guarded by
their native platform conditions.
