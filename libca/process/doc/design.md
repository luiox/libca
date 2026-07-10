# libca process design

## Goal

`libca_process` starts and supervises one external command without involving a
shell. It is intended for tools such as Java-based decompilers, where callers
need arguments preserved exactly, collected output, a working directory, and a
bounded runtime.

## Public API

```cpp
struct SubprocessOptions {
    std::string executable;
    std::vector<std::string> args;
    bool capture_stdout = true;
    bool capture_stderr = true;
    std::optional<std::string> stdin_data;
    std::optional<std::chrono::milliseconds> timeout;
    std::optional<std::string> working_dir;
};

struct SubprocessResult {
    i32 exit_code = -1;
    std::string stdout_data;
    std::string stderr_data;
    bool timed_out = false;

    bool succeeded() const noexcept;
};

SubprocessResult run(const SubprocessOptions& options);
```

Fields use `snake_case`; types use `CamelCase`.

## Result Semantics

`run()` does not throw for a normal launch, capture, or timeout failure.

- A normally exiting child returns its native exit code, including nonzero
  codes.
- A timeout sets `timed_out` to `true` and `exit_code` to `-1` after the child
  has been terminated and reaped.
- A process that cannot be started returns `exit_code == -1`,
  `timed_out == false`, and appends the platform diagnostic to `stderr_data`.
- Captured output is raw bytes stored in `std::string`; no encoding conversion
  is applied.

`capture_stdout` and `capture_stderr` default to `true`. If disabled, the
corresponding child stream is inherited from the parent and the matching result
field remains empty. With no `stdin_data`, the child inherits the parent stdin.

## Platform Mapping

| Concern | Windows | Linux |
| --- | --- | --- |
| Start | `CreateProcessW` | `fork` then `execvp` |
| UTF-8 paths | converted to UTF-16 | passed as bytes |
| Capture | inheritable `CreatePipe` handles | `pipe` and reader threads |
| Wait | `WaitForSingleObject` | `waitpid(WNOHANG)` loop |
| Timeout | `TerminateProcess` | kill child process group with `SIGKILL` |

The Linux child creates a process group before `execvp`. A timeout first kills
that group, then waits for the direct child, preventing a simple shell wrapper
from outliving the command.

## I/O and Lifetime

Output readers and the optional stdin writer run concurrently with the wait.
This prevents a child that writes more than a pipe buffer from blocking before
the parent can observe its timeout. All pipe endpoints are closed before the
function returns and all worker threads are joined.

The function does not invoke a shell. `executable` is the program path and each
element of `args` is one argument. On Windows the command line is quoted using
the `CommandLineToArgvW` escaping rules.

## Test Strategy

Platform-specific shell commands exercise:

1. successful execution with separate stdout and stderr capture;
2. timeout and termination;
3. nonzero process exit.
