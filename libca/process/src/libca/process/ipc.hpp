#pragma once

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

}   // namespace ca::process::ipc
