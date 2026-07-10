#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "libca/core/status.hpp"

namespace ca::process::ipc {

class NamedPipeConnection
{
public:
    NamedPipeConnection() = default;
    ~NamedPipeConnection();
    NamedPipeConnection(const NamedPipeConnection&)            = delete;
    NamedPipeConnection& operator=(const NamedPipeConnection&) = delete;
    NamedPipeConnection(NamedPipeConnection&& other) noexcept;
    NamedPipeConnection& operator=(NamedPipeConnection&& other) noexcept;

    ca::core::StatusResult<usize> read(void* buffer, usize capacity);
    ca::core::Status              write_all(const void* data, usize length);
    ca::core::Status              write_all(const std::string& data);
    bool                          is_open() const noexcept;
    void                          close() noexcept;

private:
    explicit NamedPipeConnection(std::intptr_t native_handle) noexcept;
    std::intptr_t native_handle_{-1};

    friend class NamedPipeServer;
    friend class NamedPipeClient;
};

class NamedPipeServer
{
public:
    NamedPipeServer() = default;
    ~NamedPipeServer();
    NamedPipeServer(const NamedPipeServer&)            = delete;
    NamedPipeServer& operator=(const NamedPipeServer&) = delete;
    NamedPipeServer(NamedPipeServer&& other) noexcept;
    NamedPipeServer& operator=(NamedPipeServer&& other) noexcept;

    static ca::core::StatusResult<NamedPipeServer> create(const std::string& name);
    ca::core::StatusResult<NamedPipeConnection>    accept();
    void                                           close() noexcept;

private:
    explicit NamedPipeServer(std::intptr_t native_handle) noexcept;
    std::intptr_t native_handle_{-1};
};

class NamedPipeClient
{
public:
    static ca::core::StatusResult<NamedPipeConnection> connect(const std::string& name);
};

class SharedMemory
{
public:
    SharedMemory() = default;
    ~SharedMemory();
    SharedMemory(const SharedMemory&)            = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&& other) noexcept;
    SharedMemory& operator=(SharedMemory&& other) noexcept;

    static ca::core::StatusResult<SharedMemory> create(const std::string& name, usize size);
    static ca::core::StatusResult<SharedMemory> open(const std::string& name);
    void*                                       data() noexcept;
    const void*                                 data() const noexcept;
    usize                                       size() const noexcept;
    bool                                        is_open() const noexcept;
    void                                        close() noexcept;

private:
    SharedMemory(std::intptr_t native_handle, void* data, usize size) noexcept;
    std::intptr_t native_handle_{-1};
    void*         data_{nullptr};
    usize         size_{0};
};

class NamedSemaphore
{
public:
    NamedSemaphore() = default;
    ~NamedSemaphore();
    NamedSemaphore(const NamedSemaphore&)            = delete;
    NamedSemaphore& operator=(const NamedSemaphore&) = delete;
    NamedSemaphore(NamedSemaphore&& other) noexcept;
    NamedSemaphore& operator=(NamedSemaphore&& other) noexcept;

    static ca::core::StatusResult<NamedSemaphore> create(const std::string& name,
                                                         u32                initial_count);
    static ca::core::StatusResult<NamedSemaphore> open(const std::string& name);
    ca::core::Status                              acquire();
    ca::core::StatusResult<bool> try_acquire_for(std::chrono::milliseconds timeout);
    ca::core::Status             release(u32 count = 1);
    void                         close() noexcept;

private:
    explicit NamedSemaphore(std::intptr_t native_handle) noexcept;
    std::intptr_t native_handle_{-1};
};

class MessageQueue
{
public:
    MessageQueue() = default;
    ~MessageQueue();
    MessageQueue(const MessageQueue&)            = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&& other) noexcept;
    MessageQueue& operator=(MessageQueue&& other) noexcept;

    static ca::core::StatusResult<MessageQueue>        create(const std::string& name,
                                                              usize              max_message_size);
    static ca::core::StatusResult<MessageQueue>        open(const std::string& name);
    ca::core::Status                                   send(const void* data, usize length);
    ca::core::Status                                   send(const std::string& data);
    ca::core::StatusResult<std::string>                receive();
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
