---
version: 1.2
update:
2026-07-14 - 由 design.md 改名补 YAML 头，本文成为 process 唯一设计文档
2026-09-15 - 新增 ShmRingQueue IPC 容错章节：头部布局、心跳与接管时序、崩溃恢复边界
---

# libca::process 设计文档

> 本文讲 process 模块的设计边界与平台差异。具体 API 签名见 `subprocess.hpp`、`ipc.hpp`
> 的 Doxygen 注释。进程控制对齐 Rust `std::process` 的所有权形状：可复用的 `Command`、
> `spawn()` 得到 move-only `Child`，IPC 提供命名管道 / 共享内存 / 命名信号量 / 消息队列。

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

`wait_with_output()` closes any owned stdin and drains the owned stdout and
stderr pipes while polling for exit, using non-blocking incremental reads
(`PeekNamedPipe` on Windows, `poll` on POSIX). This avoids a deadlock when a
child writes enough data to fill either pipe. Interactive users that take
both read ends must drain them concurrently themselves.

`wait_with_output_for(timeout)` bounds the wait. On expiry it returns
`DEADLINE_EXCEEDED` without killing the child; the stream endpoints and the
bytes drained so far stay inside the `Child`, so a later call — normally
after `kill()` — resumes draining and returns everything together with the
exit status, losing nothing. `Command::output()` accepts `OutputOptions`
with `timeout` (zero means unbounded) and `kill_on_timeout` (default true),
which terminates and reaps the child on expiry.

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
| `ShmRingQueue` | shared memory, cross-process atomics | shared memory, cross-process atomics |

All named resource names are simple tokens. The implementation supplies its
own platform namespace prefix, preventing callers from injecting a filesystem
path or a Win32 namespace path. `create()` fails with `ALREADY_EXISTS`; `open()`
fails with `NOT_FOUND` when the resource is absent. `close()` releases only
the local handle or mapping. It never removes a name that another process may
still be using.

Message queues preserve one-message boundaries. On Windows they are
intentionally one-way: `create()` returns the receiver and `open()` returns a
sender. On Linux an opened queue can send and receive. A message larger than
the configured maximum is rejected on Linux; Windows enforces the configured
bound at the receiver. `receive_for()` returns an empty optional on timeout,
allowing callers to distinguish it from operational failure.

## ShmRingQueue：IPC 容错（写者探活 + 崩溃可恢复）

### 设计定位与选型

`ShmRingQueue` 是共享内存环形消息队列：单写者、多读者、写者进程崩溃可恢复。
它与内核对象型 `MessageQueue` 互补而非替代——参考 Morn `morn_message.c` 的
思想（只取设计，不抄代码）：消息直接放进共享内存环形槽，只依赖跨进程原子
操作同步，不持有任何阻塞内核对象，写者死亡后队列仍可安全读取与接管。

为什么是新类而不是改 `MessageQueue`：现有队列由内核（mailslot / POSIX mq）
搬运消息，消息不在调用方可控的内存里，无处安放写者身份、心跳、序号等头部
字段；容错语义要求直接管理环形槽的内存布局与发布协议。按侵入最小原则新增
类型，原 `MessageQueue` 及其测试完全不动（头部格式变更不属于不兼容变更，
`ShmRingQueue` 是新增 API）。

### 共享内存布局

段大小 = 128B 头部块（64 字节对齐）+ `slot_count × slot_stride`；
`slot_stride = align_up(sizeof(槽头) + max_message_size, 64)`。
布局定义在 `ipc.hpp` 的 `ipc::detail` 命名空间（供实现与测试构造故障布局，
非稳定 API），全部定宽字段 + lock-free 原子量，MSVC/GCC 布局一致并用
`static_assert` 钉住尺寸与 lock-free 属性。

队列头（偏移 0）：

```
偏移  大小  字段               说明
0     8     magic              'LIBCRING' 魔数（open() 识别无关段）
8     4     format_version     布局版本（初始化时最后写入，即对 open() 的就绪信号）
12    4     slot_count         环形槽数量
16    8     slot_stride        单槽步长
24    8     payload_capacity   单槽 payload 容量
32    8     generation         世代号（接管/重置时 +1）
40    8     write_seq          已发布消息总数（读者接入点）
48    8     writer_slot        0=无写者 / 1=接管选举中 / 其余=写者 pid
56    8     writer_birth       写者进程出生戳（防 pid 复用误判）
64    8     writer_heartbeat   最近心跳（机器单调时钟毫秒）
```

槽（`slot_stride` 为步长）：

```
+0   u32 state     Empty=0 / Writing=1 / Committed=2
+4   u32 length    payload 有效字节数
+8   u64 sequence  全局消息序号（从 0 连续递增）
+16  ...           payload（容量 payload_capacity）
```

原子操作要求 lock-free（address-free）：各进程把段映射到不同基址，原子操作
依然正确。读者游标（世代号 + 期望序号）保存在读者本进程内，不在共享内存，
因此多读者互不干扰，也不存在共享游标的修复竞争。

### 发布协议与撕裂槽

写者发布一条消息按固定顺序：

1. `state = Writing`（占槽：此后任一时刻崩溃，槽都停留在可识别的 Writing 态）
2. 写 payload、length、sequence
3. `state = Committed`（release 发布）
4. `write_seq += 1`

读者以 acquire 读到 Committed 才允许读 payload / length / sequence——三者
必然完整可见。撕裂写（写者中途死亡）只可能停留在 Writing 态：读者在期望
序号 E 处看到 Writing 且写者进程已退出，即判定撕裂槽，跳过（E+1，修复本
地读指针）继续，绝不读出撕裂内容，也不对该槽报错（多进程测试中由父进程
断言撕製数据从未出现在读端）。

### 写者探活与接管时序

心跳：`send()` 顺带刷新（按 `Options::heartbeat_interval` 节流，默认 100ms），
长时间静默的写者可显式调 `refresh_heartbeat()`；`Options::heartbeat_timeout`
（默认 1000ms）是接管判定的超时门槛。

进程存活检测：

- Windows：`OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` +
  `GetExitCodeProcess`（≠ STILL_ACTIVE 即退出）；出生戳取 `GetProcessTimes`
  创建时间，与头部 `writer_birth` 比对抵御 pid 复用；打开句柄被拒
  （ACCESS_DENIED）保守视为存活——宁可推迟接管也不误接管。
- POSIX：`kill(pid, 0)`（EPERM 视为存在）；出生戳取 `/proc/<pid>/stat` 第
  22 字段（starttime）比对；/proc 不可读时出生戳记 0，判定退化为仅查 pid。

时序图（写者 W 崩溃、读者 R 排空、新写者 W' 接管）：

```
写者 W                        共享内存头部                 读者 R
  | send(): CAS(writer_slot 0→W.pid)                       |
  |   写 birth / heartbeat       |                          |
  | publish 消息（发布协议）     |                          | receive_for():
  | heartbeat 按 interval 刷新  |                          |   世代校验 g
  |                             |                          |   Committed→读；Writing→等
  × 崩溃（心跳停止）           | writer_slot=W.pid        |
                                | heartbeat 冻结在 t0      | alive(W)=false
                                |                          |   Writing→跳过（撕裂修复）
                                |                          |   Empty→UNAVAILABLE（排空）
新写者 W'                       |                          |
  | send() 或 reset_if_writer_dead()                        |
  | 读 heartbeat：now-t0 > timeout → 超时                   |
  | 读 alive(W)：false → 已退出                             |
  | CAS(W.pid→ELECTING) ✔（选举，至多一个接管者）           |
  | generation+1；槽全部清 Empty；write_seq=0               | 旧世代 R：下次校验
  | writer_slot=W'.pid（接管认领）                          |   → FAILED_PRECONDITION
```

接管门槛是「心跳超时 **且** 进程确认退出」，二者缺一不可：进程活着（哪怕
心跳停更，如写者卡死）绝不接管——双写者会破坏环形槽协议；进程已退出但不等
心跳超时时，读者即可做撕裂修复与排空（存活判定是决定性的，无需等超时）。

### 读者语义（文档化选择）

- **世代不匹配选择报错而非重同步**：接管/重置意味着放弃全部未读消息，静默
  重同步会掩盖丢消息的事实。旧世代读者 `receive_for()` 显式返回
  `FAILED_PRECONDITION`，调用方重新 `open()`；接管者/重置者自身则重新接入
  新流（它知道重置发生了）。
- **接入点**：首次 receive 从最旧的保留消息接入（保留窗
  `[write_seq - slot_count, write_seq)`）；读者被写者套圈、待读消息被覆盖时
  返回 `DATA_LOSS`，不静默跳读。
- **优雅退出 vs 崩溃**：`close()` 释放写者身份，下个写者走快路径立即认领，
  读者继续等待（区分不出「暂时没有写者」与「写者永久离开」）；进程死亡且
  身份残留时，读者获得 `UNAVAILABLE` 终止条件，轮询循环自然结束。

### 崩溃恢复保证与不保证的边界

保证：

- 读者绝不读到撕裂内容（Committed acquire 门禁 + Writing 态跳过）。
- 跨进程单写者互斥：认领 CAS + 出生戳 + 存活判定，双写者被拒绝（UNAVAILABLE）。
- 接管后旧读者确定性地感知（世代不匹配报错，永不混读新旧两代数据）。
- 布局跨平台一致（`static_assert` 钉住），心跳用机器单调时钟，同机可比。

不保证 / 边界：

- 接管重置是有损操作：重置即放弃全部未读消息；写者死于 commit 与
  `write_seq` 递增之间的那条消息虽然完整，也随重置一并丢弃。
- 弱内存序平台：「接管 + 慢读者并发拷贝同一槽」由双重世代校验排除，x86-TSO
  上闭环；ARM 上该窗口理论存在（本库当前以 x64 桌面为主，接受此边界）。
- pid 复用由出生戳兜底；/proc 或进程句柄不可用时退化为保守策略，可能推迟
  接管（不会误接管）。
- Windows 命名映射随最后一个句柄消失：所有进程都退出后队列连同段一起消失，
  无需恢复；POSIX 段残留 /dev/shm，用后须 `remove_shared_memory()` 回收。
- 同一进程内多线程并发调用同一队列的 `send()` / `receive_for()` 需调用方
  串行化：跨进程互斥由认领协议保证，进程内互斥不在本类职责内。
- `writer_slot` 的 ELECTING 哨兵值 1 与 POSIX pid 1 冲突，接受该限制
  （写者不会是 pid 1；`process_identity_alive` 对 pid < 2 直接判死）。

## Error and Platform Rules

Platform diagnostics are converted to `ca::core::Status` with stable codes:
`INVALID_ARGUMENT`, `NOT_FOUND`, `ALREADY_EXISTS`, `FAILED_PRECONDITION`,
`DEADLINE_EXCEEDED`, `OUT_OF_RANGE`, and `INTERNAL`. Public APIs do not throw
for expected operating-system failures.

Windows process creation uses `CreateProcessW` with UTF-8 to UTF-16 conversion
and correctly quoted arguments. Linux uses `fork`, `execvp`, pipes, and
`waitpid`. Linux additionally links `pthread` and `rt` for the IPC primitives
(semaphores, message queues) on toolchains that require them.

## Test Strategy

The unit suite covers exact argument boundaries, command reuse, nonzero exit,
interactive stdin/stdout, timeout observation followed by kill and reap,
incremental stdout/stderr collection with deadline and resume-after-kill
semantics, and the `Command::output` timeout kill policy. IPC tests cover
anonymous-pipe transfer, named-pipe exchange, shared-memory visibility, timed
semaphore acquisition, and whole-message queue delivery. Platform-specific
tests remain guarded by their native platform conditions.

`ShmRingQueue` 的容错测试分两层：单进程用例直接构造共享内存布局（心跳归零、
世代号抬升、Writing 态撕裂槽、已回收子进程 pid），验证接管门槛、世代不匹配
报错与撕裂跳过；多进程用例以 subprocess 自 spawn 测试助手（测试二进制检测
argv 模式标志进入子角色，见 `unittest/main.cpp`）：写者角色 `_Exit` 模拟
「退出不清理」，父进程 `kill()`（Windows TerminateProcess / POSIX kill）模拟
崩溃，读者角色输出到 stdout 由父进程断言。性能基准在
`perf/process_perf.cpp`（target `libca_process_perf`，`libs/perf` 组，默认不
构建）：Stopwatch 计时、热身后多轮取中位数，报告环形槽收发 op/s、强制逐次
心跳刷新的吞吐对比，并以内核 `MessageQueue` 往返作参照。
