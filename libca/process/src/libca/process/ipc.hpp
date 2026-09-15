#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "libca/core/status.hpp"

namespace ca::process::ipc {

/// @brief 命名管道的一条已建立连接，move-only。
/// @details 由 NamedPipeServer::accept 或 NamedPipeClient::connect 产出，提供流式
///          读写。Windows 对应 Win32 命名管道，Linux 对应 Unix-domain 流式 socket。
class NamedPipeConnection
{
public:
    NamedPipeConnection() = default;
    ~NamedPipeConnection();
    NamedPipeConnection(const NamedPipeConnection&)            = delete;
    NamedPipeConnection& operator=(const NamedPipeConnection&) = delete;
    NamedPipeConnection(NamedPipeConnection&& other) noexcept;
    NamedPipeConnection& operator=(NamedPipeConnection&& other) noexcept;

    /// @brief 读取最多 capacity 字节。
    /// @return 成功返回实际字节数（0 表示对端关闭的干净 EOF）；系统错误返回 Status。
    ca::core::StatusResult<usize> read(void* buffer, usize capacity);
    /// @brief 循环写入直到 length 字节全部写出。
    ca::core::Status              write_all(const void* data, usize length);
    /// @brief 写入字符串全部内容。
    ca::core::Status              write_all(const std::string& data);
    bool                          is_open() const noexcept;
    void                          close() noexcept;

private:
    explicit NamedPipeConnection(std::intptr_t native_handle) noexcept;
    std::intptr_t native_handle_{-1};

    friend class NamedPipeServer;
    friend class NamedPipeClient;
};

/// @brief 命名管道服务端，move-only。
/// @details create() 创建命名管道并进入监听，accept() 阻塞等待一个客户端连接。
///          名字是简单 token，实现自行加平台命名空间前缀，调用方无法注入文件系统路径。
class NamedPipeServer
{
public:
    NamedPipeServer() = default;
    ~NamedPipeServer();
    NamedPipeServer(const NamedPipeServer&)            = delete;
    NamedPipeServer& operator=(const NamedPipeServer&) = delete;
    NamedPipeServer(NamedPipeServer&& other) noexcept;
    NamedPipeServer& operator=(NamedPipeServer&& other) noexcept;

    /// @brief 创建命名管道服务端；名字已存在时返回 ALREADY_EXISTS。
    static ca::core::StatusResult<NamedPipeServer> create(const std::string& name);
    /// @brief 阻塞等待一个客户端连接，返回可用连接。
    ca::core::StatusResult<NamedPipeConnection>    accept();
    /// @brief 关闭监听句柄；不会移除可能被其它进程仍使用的名字。
    void                                           close() noexcept;

private:
    explicit NamedPipeServer(std::intptr_t native_handle) noexcept;
    std::intptr_t native_handle_{-1};
};

/// @brief 命名管道客户端（仅静态工厂）。
class NamedPipeClient
{
public:
    /// @brief 连接到已存在的命名管道；管道不存在时返回 NOT_FOUND。
    static ca::core::StatusResult<NamedPipeConnection> connect(const std::string& name);
};

/// @brief 共享内存段，move-only。
/// @details Windows 用文件映射 + 视图，Linux 用 shm_open + mmap。create() 创建并映射，
///          open() 打开已存在的段。close() 只释放本地句柄与映射，不移除名字。
class SharedMemory
{
public:
    SharedMemory() = default;
    ~SharedMemory();
    SharedMemory(const SharedMemory&)            = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&& other) noexcept;
    SharedMemory& operator=(SharedMemory&& other) noexcept;

    /// @brief 创建并映射 size 字节的共享内存段；名字已存在时返回 ALREADY_EXISTS。
    static ca::core::StatusResult<SharedMemory> create(const std::string& name, usize size);
    /// @brief 打开已存在的共享内存段；不存在时返回 NOT_FOUND。
    static ca::core::StatusResult<SharedMemory> open(const std::string& name);
    /// @brief 返回映射基地址；未映射返回 nullptr。
    void*                                       data() noexcept;
    const void*                                 data() const noexcept;
    /// @brief 返回映射的字节大小。
    usize                                       size() const noexcept;
    bool                                        is_open() const noexcept;
    void                                        close() noexcept;

private:
    SharedMemory(std::intptr_t native_handle, void* data, usize size) noexcept;
    std::intptr_t native_handle_{-1};
    void*         data_{nullptr};
    usize         size_{0};
};

/// @brief 命名信号量，move-only。
/// @details Windows 用命名信号量句柄，Linux 用 sem_open。用于跨进程的计数同步。
class NamedSemaphore
{
public:
    NamedSemaphore() = default;
    ~NamedSemaphore();
    NamedSemaphore(const NamedSemaphore&)            = delete;
    NamedSemaphore& operator=(const NamedSemaphore&) = delete;
    NamedSemaphore(NamedSemaphore&& other) noexcept;
    NamedSemaphore& operator=(NamedSemaphore&& other) noexcept;

    /// @brief 创建初始计数为 initial_count 的命名信号量；名字已存在时返回 ALREADY_EXISTS。
    static ca::core::StatusResult<NamedSemaphore> create(const std::string& name,
                                                         u32                initial_count);
    /// @brief 打开已存在的命名信号量；不存在时返回 NOT_FOUND。
    static ca::core::StatusResult<NamedSemaphore> open(const std::string& name);
    /// @brief 计数减一，计数为 0 时阻塞直到有可用计数。
    ca::core::Status                              acquire();
    /// @brief 限时尝试获取；超时返回 false（区分于系统错误）。
    ca::core::StatusResult<bool> try_acquire_for(std::chrono::milliseconds timeout);
    /// @brief 计数加 count（默认 1），唤醒等待者。
    ca::core::Status             release(u32 count = 1);
    void                         close() noexcept;

private:
    explicit NamedSemaphore(std::intptr_t native_handle) noexcept;
    std::intptr_t native_handle_{-1};
};

/// @brief 保序消息队列，move-only。
/// @details Windows 用 mailslot，Linux 用 POSIX 消息队列，二者都保留单条消息边界。
///          Windows 上有意设计为单向：create() 返回接收端，open() 返回发送端；
///          Linux 上打开的队列可同时收发。超过 max_message_size 的消息在 Linux 上被拒绝，
///          Windows 在接收端按配置上限截断。close() 只释放本地句柄。
class MessageQueue
{
public:
    MessageQueue() = default;
    ~MessageQueue();
    MessageQueue(const MessageQueue&)            = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&& other) noexcept;
    MessageQueue& operator=(MessageQueue&& other) noexcept;

    /// @brief 创建上限为 max_message_size 的消息队列；名字已存在时返回 ALREADY_EXISTS。
    static ca::core::StatusResult<MessageQueue>        create(const std::string& name,
                                                              usize              max_message_size);
    /// @brief 打开已存在的消息队列；不存在时返回 NOT_FOUND。
    static ca::core::StatusResult<MessageQueue>        open(const std::string& name);
    /// @brief 发送一条 length 字节消息。
    ca::core::Status                                   send(const void* data, usize length);
    /// @brief 发送一条字符串消息。
    ca::core::Status                                   send(const std::string& data);
    /// @brief 阻塞接收一条消息。
    ca::core::StatusResult<std::string>                receive();
    /// @brief 限时接收；超时返回空 optional（区分于系统错误）。
    ca::core::StatusResult<std::optional<std::string>> receive_for(
        std::chrono::milliseconds timeout);
    void close() noexcept;

private:
    MessageQueue(std::intptr_t native_handle, usize max_message_size, bool receiver) noexcept;
    std::intptr_t native_handle_{-1};
    usize         max_message_size_{0};
    bool          receiver_{false};
};

/// @brief 共享内存环形消息队列的内存布局细节。
/// @details 仅供 ShmRingQueue 的实现与测试直接构造故障内存布局使用，不属于稳定 API：
///          字段布局一经发布即冻结（跨进程共享内存两端必须一致），但命名与存在性可能调整。
///          所有字段定宽、自然对齐，Windows 与 Linux 布局一致；原子量要求 lock-free
///          （实现内有 static_assert 钉住），因此跨进程映射到不同地址仍可安全操作。
namespace detail {

/// @brief 队列头魔数（ASCII "LIBCRING"），open() 用于识别无关段。
inline constexpr u64 kRingMagic = 0x4C49424352494E47ULL;
/// @brief 布局版本号；open() 拒绝版本不符的段（同批内新旧格式一致性由该值钉住）。
inline constexpr u32 kRingFormatVersion = 1;
/// @brief writer_slot 取值：表示当前没有写者。
inline constexpr u64 kRingWriterFree = 0;
/// @brief writer_slot 取值：表示某进程正在接管（选举中），其余非零值为写者 pid。
/// @note 该哨兵值与 POSIX pid 1 冲突，本库接受该限制（测试进程不会是 pid 1）。
inline constexpr u64 kRingWriterElecting = 1;
/// @brief 头部块大小：对齐到 64 字节缓存行，槽区从该偏移开始。
inline constexpr usize kRingHeaderSize = 128;

/// @brief 单个槽的状态。
enum class RingSlotState : u32
{
    Empty = 0,     ///< 空槽（从未写过，或已被重置清空）
    Writing = 1,   ///< 写者正在写：写者中途死亡后停留在该状态的槽即撕裂槽
    Committed = 2, ///< 写者已发布：payload / length / sequence 对读者完整可见
};

/// @brief 槽头，紧跟 payload 起始处。
struct RingSlotHeader
{
    std::atomic<u32> state;    ///< RingSlotState
    std::atomic<u32> length;   ///< payload 有效字节数
    std::atomic<u64> sequence; ///< 全局消息序号（从 0 起连续递增）
};

/// @brief 队列头，位于共享内存段起始处。
struct RingHeader
{
    u64              magic;            ///< kRingMagic
    u32              format_version;   ///< kRingFormatVersion（初始化时最后写入）
    u32              slot_count;       ///< 环形槽数量
    u64              slot_stride;      ///< 单槽步长（槽头 + payload 向上对齐到 64 字节）
    u64              payload_capacity; ///< 单槽 payload 容量（即 max_message_size）
    std::atomic<u64> generation;       ///< 世代号：接管/重置时 +1，旧读者据此感知
    std::atomic<u64> write_seq;        ///< 已发布消息总数（新读者的接入点）
    std::atomic<u64> writer_slot;      ///< 写者占用标识（kRingWriter* 或写者 pid）
    std::atomic<u64> writer_birth;     ///< 写者进程出生戳（防 pid 复用误判，0 表示未知）
    std::atomic<u64> writer_heartbeat; ///< 写者最近一次心跳（机器单调时钟毫秒）
};

/// @brief 计算单槽步长：槽头 + payload 向上对齐到 64 字节。
inline usize ring_slot_stride(usize payload_capacity) noexcept
{
    const usize raw = sizeof(RingSlotHeader) + payload_capacity;
    return (raw + 63) / 64 * 64;
}

/// @brief 计算共享内存段总大小：头部块 + slot_count 个槽。
inline usize ring_segment_size(usize slot_count, usize payload_capacity) noexcept
{
    return kRingHeaderSize + slot_count * ring_slot_stride(payload_capacity);
}

}   // namespace detail

/// @brief 共享内存环形消息队列：单写者、多读者、写者崩溃可恢复，move-only。
/// @details 与基于内核对象（mailslot / POSIX mq）的 MessageQueue 互补：消息放进命名
///          共享内存的环形槽位，仅依赖跨进程原子操作同步，不持有任何阻塞内核对象。
///          头部记录活跃写者 pid、出生戳与心跳（send() 顺带按间隔节流刷新）；写者进程
///          死亡后，读者能跳过撕裂槽安全排空已提交消息（绝不读到撕裂内容）；新写者在
///          「心跳超时且进程已退出」后可接管并重置队列，世代号 +1 让旧读者显式感知。
///          POSIX 上名字残留 /dev/shm，用后需 remove_shared_memory() 回收名字。
///
///          并发约定：跨进程同一时刻至多一个活跃写者（由认领协议保证）；同一进程内
///          多线程调用同一队列的 send()/receive_for() 需调用方自行串行化。
class ShmRingQueue
{
public:
    /// @brief 队列参数。create() 使用全部字段；open() 只使用心跳参数（布局从头部读取）。
    struct Options
    {
        /// @brief 单条消息最大字节数（即单槽 payload 容量），范围 [1, 64 MiB]。
        usize                     max_message_size = 4096;
        /// @brief 环形槽数量，范围 [1, 65536]。
        usize                     slot_count       = 64;
        /// @brief 写者心跳刷新的最小间隔：send() 顺带刷新，按该间隔节流。
        std::chrono::milliseconds heartbeat_interval{100};
        /// @brief 心跳超时：接管/重置要求写者心跳落后超过该时长且进程已退出。
        std::chrono::milliseconds heartbeat_timeout{1000};
    };

    ShmRingQueue() = default;
    ~ShmRingQueue();
    ShmRingQueue(const ShmRingQueue&)            = delete;
    ShmRingQueue& operator=(const ShmRingQueue&) = delete;
    ShmRingQueue(ShmRingQueue&& other) noexcept;
    ShmRingQueue& operator=(ShmRingQueue&& other) noexcept;

    /// @brief 创建队列（默认参数）；名字已存在时返回 ALREADY_EXISTS。
    /// @note 不用 `= {}` 默认实参：外层类定义内取嵌套类型 NSDMI 是 GCC 拒绝的写法。
    static ca::core::StatusResult<ShmRingQueue> create(const std::string& name);
    /// @brief 创建队列；名字已存在时返回 ALREADY_EXISTS，参数越界返回 INVALID_ARGUMENT。
    static ca::core::StatusResult<ShmRingQueue> create(const std::string& name,
                                                       const Options&     options);
    /// @brief 打开已存在的队列（默认心跳参数）。
    static ca::core::StatusResult<ShmRingQueue> open(const std::string& name);
    /// @brief 打开已存在的队列；不存在时返回 NOT_FOUND，魔数/版本/布局校验失败或段
    ///        尚在初始化时返回 FAILED_PRECONDITION。只使用 options 的心跳参数。
    static ca::core::StatusResult<ShmRingQueue> open(const std::string& name,
                                                     const Options&     options);

    /// @brief 发送一条 length 字节消息（写者身份懒惰认领）。
    /// @details 无写者时直接认领；写者已死亡且心跳超时时接管并重置队列（世代号 +1）；
    ///          其它进程的写者仍活跃（或心跳未超时）时返回 UNAVAILABLE。
    ca::core::Status                            send(const void* data, usize length);
    /// @brief 发送一条字符串消息。
    ca::core::Status                            send(const std::string& data);

    /// @brief 阻塞接收一条消息。
    /// @details 内部按短时限轮询 receive_for()；写者进程已退出且消息（含撕裂槽跳过）
    ///          排空后返回 UNAVAILABLE，读者循环因此能随写者死亡自然结束。
    ca::core::StatusResult<std::string>         receive();

    /// @brief 限时接收一条消息。
    /// @return 超时且写者仍存活（或尚无写者）返回空 optional；消息到达返回内容。
    ///         错误情形：UNAVAILABLE 表示写者进程已退出且无更多消息；FAILED_PRECONDITION
    ///         表示队列被接管/重置（世代号变化），需重新 open；DATA_LOSS 表示读者被
    ///         写者套圈，待读消息已被覆盖。撕裂槽在内部跳过并修复读指针，不对外暴露。
    /// @note 读者游标保存在本进程内；首次接收从最旧的保留消息接入（被覆盖的更早
    ///       消息报 DATA_LOSS）。
    ca::core::StatusResult<std::optional<std::string>> receive_for(
        std::chrono::milliseconds timeout);

    /// @brief 若写者已死亡（心跳超时且进程退出）则接管并重置队列。
    /// @return true 表示本次调用完成重置：世代号 +1、环形槽清空、写者身份释放，本对象
    ///         的读者游标随之重新接入新流；false 表示无需或无法接管（无写者、心跳未
    ///         超时、进程仍存活或其它进程接管中）。
    ca::core::StatusResult<bool>                reset_if_writer_dead();

    /// @brief 当前是否存在活跃写者（写者身份已认领且对应进程仍在运行）。
    bool                                        is_writer_alive();
    /// @brief 立即刷新写者心跳（仅当本进程持有写者身份时生效）。
    /// @details 长时间不发送消息的写者可按需调用；send() 本身会顺带按
    ///          Options::heartbeat_interval 节流刷新。
    void                                        refresh_heartbeat();
    /// @brief 关闭并解除映射；若本进程持有写者身份则一并释放（下个写者立即可认领）。
    void                                        close() noexcept;
    bool                                        is_open() const noexcept;

private:
    ShmRingQueue(SharedMemory segment, const Options& options) noexcept;

    detail::RingHeader*       header() noexcept;
    detail::RingSlotHeader*   slot_at(u64 sequence) noexcept;
    char*                     slot_payload(detail::RingSlotHeader* slot) noexcept;
    /// @brief 确保本进程持有写者身份：认领 / 接管（含心跳超时与进程死亡判定）。
    ca::core::Status          ensure_writer_claimed();
    /// @brief 接管成功后的重置：世代号 +1、清空槽位，claim 决定是否同时占住写者身份。
    void                      reset_ring(u64 pid, u64 birth, bool claim) noexcept;
    /// @brief 首次 receive 时接入当前世代与消息流（读者游标保存在本进程内）。
    void                      attach_reader() noexcept;

    SharedMemory segment_{};
    Options      options_{};
    /// @brief 读者侧游标：世代号与下一条期望的消息序号（仅本进程可见）。
    bool         reader_attached_{false};
    u64          reader_generation_{0};
    u64          reader_next_seq_{0};
    /// @brief 写者侧缓存：本进程认领的写者身份与上次心跳时刻。
    bool         writer_claimed_{false};
    u64          writer_pid_{0};
    u64          writer_birth_{0};
    u64          last_heartbeat_ms_{0};
};

/// @brief 移除命名共享内存段的名字（POSIX shm_unlink）。
/// @note Windows 命名对象随最后一个句柄关闭自动回收，恒成功（空操作）。
///       Linux 上 /dev/shm 下的对象必须显式移除，否则残留至重启；
///       名字不存在视为成功（幂等清理）。
ca::core::Status remove_shared_memory(const std::string& name);

/// @brief 移除命名信号量的名字（POSIX sem_unlink）。
/// @note 语义同 remove_shared_memory：Windows 空操作，不存在视为成功。
ca::core::Status remove_semaphore(const std::string& name);

/// @brief 移除消息队列的名字（POSIX mq_unlink）。
/// @note 语义同 remove_shared_memory：Windows 空操作，不存在视为成功。
ca::core::Status remove_message_queue(const std::string& name);

}   // namespace ca::process::ipc
