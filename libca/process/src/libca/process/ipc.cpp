#include "libca/process/ipc.hpp"

#include <algorithm>
#include <limits>
#include <thread>
#include <utility>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <cerrno>
#    include <cstring>
#    include <fcntl.h>
#    include <mqueue.h>
#    include <sys/mman.h>
#    include <semaphore.h>
#    include <sys/socket.h>
#    include <sys/stat.h>
#    include <sys/un.h>
#    include <time.h>
#    include <unistd.h>
#endif

namespace ca::process::ipc {
namespace {

using ca::core::Err;
using ca::core::ErrStatus;
using ca::core::Ok;
using ca::core::OkStatus;
using ca::core::Status;
using ca::core::StatusCode;
template<typename T>
using StatusResult = ca::core::StatusResult<T>;

#if defined(_WIN32)
HANDLE to_handle(std::intptr_t value)
{
    return reinterpret_cast<HANDLE>(value);
}
std::intptr_t to_native(HANDLE value)
{
    return reinterpret_cast<std::intptr_t>(value);
}

Status windows_error(const char* operation)
{
    return ErrStatus(StatusCode::INTERNAL,
                     std::string(operation) + " failed with Windows error " +
                         std::to_string(static_cast<unsigned long>(GetLastError())));
}

StatusResult<std::wstring> utf8_to_utf16(const std::string& value)
{
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "name is not valid UTF-8"));
    std::wstring converted(static_cast<usize>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            &converted[0],
                            length) == 0)
        return Err(windows_error("MultiByteToWideChar"));
    return Ok(std::move(converted));
}

StatusResult<std::wstring> pipe_name(const std::string& name)
{
    auto converted = utf8_to_utf16(name);
    if (converted.is_err())
        return Err(converted.unwrap_err());
    std::wstring       value  = std::move(converted).unwrap();
    const std::wstring prefix = L"\\\\.\\pipe\\";
    if (value.compare(0, prefix.size(), prefix) != 0)
        value = prefix + value;
    return Ok(std::move(value));
}

StatusResult<std::wstring> mailslot_name(const std::string& name)
{
    auto converted = utf8_to_utf16(name);
    if (converted.is_err())
        return Err(converted.unwrap_err());
    return Ok(std::wstring(L"\\\\.\\mailslot\\") + std::move(converted).unwrap());
}
#endif

#if !defined(_WIN32)
int to_fd(std::intptr_t value)
{
    return static_cast<int>(value);
}

Status posix_error(const char* operation)
{
    return ErrStatus(StatusCode::INTERNAL,
                     std::string(operation) + " failed: " + std::strerror(errno));
}

StatusResult<std::string> unix_socket_path(const std::string& name)
{
    if (name.empty() || name.find('/') != std::string::npos)
        return Err(
            ErrStatus(StatusCode::INVALID_ARGUMENT, "named pipe name must be a simple token"));
    const std::string path = "/tmp/libca_process_" + name + ".sock";
    if (path.size() >= sizeof(sockaddr_un{}.sun_path))
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "named pipe name is too long"));
    return Ok(path);
}

StatusResult<std::string> posix_shared_memory_name(const std::string& name)
{
    if (name.empty() || name.find('/') != std::string::npos)
        return Err(
            ErrStatus(StatusCode::INVALID_ARGUMENT, "shared memory name must be a simple token"));
    return Ok("/libca_process_" + name);
}
#endif

}   // namespace

NamedPipeConnection::NamedPipeConnection(std::intptr_t native_handle) noexcept
    : native_handle_(native_handle)
{}
NamedPipeConnection::~NamedPipeConnection()
{
    close();
}
NamedPipeConnection::NamedPipeConnection(NamedPipeConnection&& other) noexcept
    : native_handle_(other.native_handle_)
{
    other.native_handle_ = -1;
}
NamedPipeConnection& NamedPipeConnection::operator=(NamedPipeConnection&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_       = other.native_handle_;
        other.native_handle_ = -1;
    }
    return *this;
}
bool NamedPipeConnection::is_open() const noexcept
{
    return native_handle_ != -1;
}
void NamedPipeConnection::close() noexcept
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

StatusResult<usize> NamedPipeConnection::read(void* buffer, usize capacity)
{
    if (!is_open())
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION, "read on a closed named pipe"));
#if defined(_WIN32)
    DWORD count = 0;
    if (!ReadFile(
            to_handle(native_handle_), buffer, static_cast<DWORD>(capacity), &count, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE)
            return Ok(static_cast<usize>(0));
        return Err(windows_error("ReadFile"));
    }
    return Ok(static_cast<usize>(count));
#else
    const ssize_t count = ::read(to_fd(native_handle_), buffer, capacity);
    if (count < 0)
        return Err(posix_error("read"));
    return Ok(static_cast<usize>(count));
#endif
}

Status NamedPipeConnection::write_all(const void* data, usize length)
{
    if (!is_open())
        return ErrStatus(StatusCode::FAILED_PRECONDITION, "write on a closed named pipe");
#if defined(_WIN32)
    usize offset = 0;
    while (offset < length) {
        DWORD       count = 0;
        const DWORD chunk =
            static_cast<DWORD>(std::min<usize>(length - offset, std::numeric_limits<DWORD>::max()));
        if (!WriteFile(to_handle(native_handle_),
                       static_cast<const char*>(data) + offset,
                       chunk,
                       &count,
                       nullptr) ||
            count == 0)
            return windows_error("WriteFile");
        offset += count;
    }
    return OkStatus();
#else
    usize offset = 0;
    while (offset < length) {
        const ssize_t count = ::write(
            to_fd(native_handle_), static_cast<const char*>(data) + offset, length - offset);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return posix_error("write");
        }
        offset += static_cast<usize>(count);
    }
    return OkStatus();
#endif
}
Status NamedPipeConnection::write_all(const std::string& data)
{
    return write_all(data.data(), data.size());
}

NamedPipeServer::NamedPipeServer(std::intptr_t native_handle) noexcept
    : native_handle_(native_handle)
{}
NamedPipeServer::~NamedPipeServer()
{
    close();
}
NamedPipeServer::NamedPipeServer(NamedPipeServer&& other) noexcept
    : native_handle_(other.native_handle_)
{
    other.native_handle_ = -1;
}
NamedPipeServer& NamedPipeServer::operator=(NamedPipeServer&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_       = other.native_handle_;
        other.native_handle_ = -1;
    }
    return *this;
}
void NamedPipeServer::close() noexcept
{
    if (native_handle_ == -1)
        return;
#if defined(_WIN32)
    CloseHandle(to_handle(native_handle_));
#else
    sockaddr_un address{};
    socklen_t   address_length = sizeof(address);
    if (getsockname(
            to_fd(native_handle_), reinterpret_cast<sockaddr*>(&address), &address_length) == 0)
        unlink(address.sun_path);
    ::close(to_fd(native_handle_));
#endif
    native_handle_ = -1;
}

StatusResult<NamedPipeServer> NamedPipeServer::create(const std::string& name)
{
#if defined(_WIN32)
    auto path = pipe_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    HANDLE handle = CreateNamedPipeW(std::move(path).unwrap().c_str(),
                                     PIPE_ACCESS_DUPLEX,
                                     PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                     1,
                                     4096,
                                     4096,
                                     0,
                                     nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return Err(windows_error("CreateNamedPipeW"));
    return Ok(NamedPipeServer(to_native(handle)));
#else
    auto path = unix_socket_path(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    const int handle = socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle < 0)
        return Err(posix_error("socket"));
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, std::move(path).unwrap().c_str(), sizeof(address.sun_path) - 1);
    if (bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(handle, 1) != 0) {
        const auto error = posix_error("bind/listen");
        ::close(handle);
        return Err(error);
    }
    return Ok(NamedPipeServer(handle));
#endif
}

StatusResult<NamedPipeConnection> NamedPipeServer::accept()
{
    if (native_handle_ == -1)
        return Err(
            ErrStatus(StatusCode::FAILED_PRECONDITION, "accept on a closed named pipe server"));
#if defined(_WIN32)
    HANDLE handle = to_handle(native_handle_);
    if (!ConnectNamedPipe(handle, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
        return Err(windows_error("ConnectNamedPipe"));
    native_handle_ = -1;
    return Ok(NamedPipeConnection(to_native(handle)));
#else
    const int handle = accept(to_fd(native_handle_), nullptr, nullptr);
    if (handle < 0)
        return Err(posix_error("accept"));
    close();
    return Ok(NamedPipeConnection(handle));
#endif
}

StatusResult<NamedPipeConnection> NamedPipeClient::connect(const std::string& name)
{
#if defined(_WIN32)
    auto path = pipe_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    HANDLE handle = CreateFileW(std::move(path).unwrap().c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return Err(windows_error("CreateFileW"));
    return Ok(NamedPipeConnection(to_native(handle)));
#else
    auto path = unix_socket_path(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    const int handle = socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle < 0)
        return Err(posix_error("socket"));
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, std::move(path).unwrap().c_str(), sizeof(address.sun_path) - 1);
    if (connect(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        const auto error = posix_error("connect");
        ::close(handle);
        return Err(error);
    }
    return Ok(NamedPipeConnection(handle));
#endif
}

SharedMemory::SharedMemory(std::intptr_t native_handle, void* data, usize size) noexcept
    : native_handle_(native_handle)
    , data_(data)
    , size_(size)
{}
SharedMemory::~SharedMemory()
{
    close();
}
SharedMemory::SharedMemory(SharedMemory&& other) noexcept
    : native_handle_(other.native_handle_)
    , data_(other.data_)
    , size_(other.size_)
{
    other.native_handle_ = -1;
    other.data_          = nullptr;
    other.size_          = 0;
}
SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_       = other.native_handle_;
        data_                = other.data_;
        size_                = other.size_;
        other.native_handle_ = -1;
        other.data_          = nullptr;
        other.size_          = 0;
    }
    return *this;
}
bool SharedMemory::is_open() const noexcept
{
    return native_handle_ != -1 && data_ != nullptr;
}
void* SharedMemory::data() noexcept
{
    return data_;
}
const void* SharedMemory::data() const noexcept
{
    return data_;
}
usize SharedMemory::size() const noexcept
{
    return size_;
}
void SharedMemory::close() noexcept
{
    if (!is_open())
        return;
#if defined(_WIN32)
    UnmapViewOfFile(data_);
    CloseHandle(to_handle(native_handle_));
#else
    munmap(data_, size_);
    ::close(to_fd(native_handle_));
#endif
    native_handle_ = -1;
    data_          = nullptr;
    size_          = 0;
}

StatusResult<SharedMemory> SharedMemory::create(const std::string& name, usize size)
{
    if (size == 0)
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "shared memory size must be nonzero"));
#if defined(_WIN32)
    auto wide_name = utf8_to_utf16(name);
    if (wide_name.is_err())
        return Err(wide_name.unwrap_err());
    const u64 length = static_cast<u64>(size);
    HANDLE    handle = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                       nullptr,
                                       PAGE_READWRITE,
                                       static_cast<DWORD>(length >> 32),
                                       static_cast<DWORD>(length),
                                       std::move(wide_name).unwrap().c_str());
    if (handle == nullptr)
        return Err(windows_error("CreateFileMappingW"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        return Err(ErrStatus(StatusCode::ALREADY_EXISTS, "shared memory already exists"));
    }
    void* view = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!view) {
        CloseHandle(handle);
        return Err(windows_error("MapViewOfFile"));
    }
    return Ok(SharedMemory(to_native(handle), view, size));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    const int handle = shm_open(std::move(path).unwrap().c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (handle < 0) {
        if (errno == EEXIST)
            return Err(ErrStatus(StatusCode::ALREADY_EXISTS, "shared memory already exists"));
        return Err(posix_error("shm_open"));
    }
    if (ftruncate(handle, static_cast<off_t>(size)) != 0) {
        const auto error = posix_error("ftruncate");
        ::close(handle);
        return Err(error);
    }
    void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
    if (view == MAP_FAILED) {
        const auto error = posix_error("mmap");
        ::close(handle);
        return Err(error);
    }
    return Ok(SharedMemory(handle, view, size));
#endif
}

StatusResult<SharedMemory> SharedMemory::open(const std::string& name)
{
#if defined(_WIN32)
    auto wide_name = utf8_to_utf16(name);
    if (wide_name.is_err())
        return Err(wide_name.unwrap_err());
    HANDLE handle =
        OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, std::move(wide_name).unwrap().c_str());
    if (!handle)
        return Err(windows_error("OpenFileMappingW"));
    MEMORY_BASIC_INFORMATION info{};
    void*                    view = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view) {
        CloseHandle(handle);
        return Err(windows_error("MapViewOfFile"));
    }
    VirtualQuery(view, &info, sizeof(info));
    return Ok(SharedMemory(to_native(handle), view, info.RegionSize));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    const int handle = shm_open(std::move(path).unwrap().c_str(), O_RDWR, 0600);
    if (handle < 0) {
        if (errno == ENOENT)
            return Err(ErrStatus(StatusCode::NOT_FOUND, "shared memory not found"));
        return Err(posix_error("shm_open"));
    }
    struct stat info
    {};
    if (fstat(handle, &info) != 0 || info.st_size <= 0) {
        const auto error = posix_error("fstat");
        ::close(handle);
        return Err(error);
    }
    const usize size = static_cast<usize>(info.st_size);
    void*       view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
    if (view == MAP_FAILED) {
        const auto error = posix_error("mmap");
        ::close(handle);
        return Err(error);
    }
    return Ok(SharedMemory(handle, view, size));
#endif
}

NamedSemaphore::NamedSemaphore(std::intptr_t native_handle) noexcept
    : native_handle_(native_handle)
{}
NamedSemaphore::~NamedSemaphore()
{
    close();
}
NamedSemaphore::NamedSemaphore(NamedSemaphore&& other) noexcept
    : native_handle_(other.native_handle_)
{
    other.native_handle_ = -1;
}
NamedSemaphore& NamedSemaphore::operator=(NamedSemaphore&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_       = other.native_handle_;
        other.native_handle_ = -1;
    }
    return *this;
}
void NamedSemaphore::close() noexcept
{
    if (native_handle_ == -1)
        return;
#if defined(_WIN32)
    CloseHandle(to_handle(native_handle_));
#else
    sem_close(reinterpret_cast<sem_t*>(native_handle_));
#endif
    native_handle_ = -1;
}

StatusResult<NamedSemaphore> NamedSemaphore::create(const std::string& name, u32 initial_count)
{
#if defined(_WIN32)
    auto wide_name = utf8_to_utf16(name);
    if (wide_name.is_err())
        return Err(wide_name.unwrap_err());
    HANDLE handle = CreateSemaphoreW(
        nullptr, static_cast<LONG>(initial_count), LONG_MAX, std::move(wide_name).unwrap().c_str());
    if (!handle)
        return Err(windows_error("CreateSemaphoreW"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        return Err(ErrStatus(StatusCode::ALREADY_EXISTS, "semaphore already exists"));
    }
    return Ok(NamedSemaphore(to_native(handle)));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    sem_t* handle =
        sem_open(std::move(path).unwrap().c_str(), O_CREAT | O_EXCL, 0600, initial_count);
    if (handle == SEM_FAILED)
        return Err(errno == EEXIST
                       ? ErrStatus(StatusCode::ALREADY_EXISTS, "semaphore already exists")
                       : posix_error("sem_open"));
    return Ok(NamedSemaphore(reinterpret_cast<std::intptr_t>(handle)));
#endif
}

StatusResult<NamedSemaphore> NamedSemaphore::open(const std::string& name)
{
#if defined(_WIN32)
    auto wide_name = utf8_to_utf16(name);
    if (wide_name.is_err())
        return Err(wide_name.unwrap_err());
    HANDLE handle =
        OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, std::move(wide_name).unwrap().c_str());
    if (!handle)
        return Err(windows_error("OpenSemaphoreW"));
    return Ok(NamedSemaphore(to_native(handle)));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    sem_t* handle = sem_open(std::move(path).unwrap().c_str(), 0);
    if (handle == SEM_FAILED)
        return Err(errno == ENOENT ? ErrStatus(StatusCode::NOT_FOUND, "semaphore not found")
                                   : posix_error("sem_open"));
    return Ok(NamedSemaphore(reinterpret_cast<std::intptr_t>(handle)));
#endif
}

Status NamedSemaphore::acquire()
{
    if (native_handle_ == -1)
        return ErrStatus(StatusCode::FAILED_PRECONDITION, "acquire on a closed semaphore");
#if defined(_WIN32)
    return WaitForSingleObject(to_handle(native_handle_), INFINITE) == WAIT_OBJECT_0
               ? OkStatus()
               : windows_error("WaitForSingleObject");
#else
    while (sem_wait(reinterpret_cast<sem_t*>(native_handle_)) != 0) {
        if (errno != EINTR)
            return posix_error("sem_wait");
    }
    return OkStatus();
#endif
}

StatusResult<bool> NamedSemaphore::try_acquire_for(std::chrono::milliseconds timeout)
{
    if (native_handle_ == -1)
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION, "wait on a closed semaphore"));
#if defined(_WIN32)
    const DWORD result = WaitForSingleObject(to_handle(native_handle_),
                                             static_cast<DWORD>(std::max<i64>(0, timeout.count())));
    if (result == WAIT_OBJECT_0)
        return Ok(true);
    if (result == WAIT_TIMEOUT)
        return Ok(false);
    return Err(windows_error("WaitForSingleObject"));
#else
    timespec deadline{};
    clock_gettime(CLOCK_REALTIME, &deadline);
    const i64 nanoseconds = static_cast<i64>(deadline.tv_nsec) + timeout.count() * 1000000;
    deadline.tv_sec += nanoseconds / 1000000000;
    deadline.tv_nsec = nanoseconds % 1000000000;
    while (sem_timedwait(reinterpret_cast<sem_t*>(native_handle_), &deadline) != 0) {
        if (errno == EINTR)
            continue;
        if (errno == ETIMEDOUT)
            return Ok(false);
        return Err(posix_error("sem_timedwait"));
    }
    return Ok(true);
#endif
}

Status NamedSemaphore::release(u32 count)
{
    if (native_handle_ == -1 || count == 0)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "invalid semaphore release");
#if defined(_WIN32)
    return ReleaseSemaphore(to_handle(native_handle_), static_cast<LONG>(count), nullptr)
               ? OkStatus()
               : windows_error("ReleaseSemaphore");
#else
    for (u32 index = 0; index < count; ++index)
        if (sem_post(reinterpret_cast<sem_t*>(native_handle_)) != 0)
            return posix_error("sem_post");
    return OkStatus();
#endif
}

MessageQueue::MessageQueue(std::intptr_t native_handle, usize max_message_size,
                           bool receiver) noexcept
    : native_handle_(native_handle)
    , max_message_size_(max_message_size)
    , receiver_(receiver)
{}
MessageQueue::~MessageQueue()
{
    close();
}
MessageQueue::MessageQueue(MessageQueue&& other) noexcept
    : native_handle_(other.native_handle_)
    , max_message_size_(other.max_message_size_)
    , receiver_(other.receiver_)
{
    other.native_handle_    = -1;
    other.max_message_size_ = 0;
    other.receiver_         = false;
}
MessageQueue& MessageQueue::operator=(MessageQueue&& other) noexcept
{
    if (this != &other) {
        close();
        native_handle_          = other.native_handle_;
        max_message_size_       = other.max_message_size_;
        receiver_               = other.receiver_;
        other.native_handle_    = -1;
        other.max_message_size_ = 0;
        other.receiver_         = false;
    }
    return *this;
}
void MessageQueue::close() noexcept
{
    if (native_handle_ == -1)
        return;
#if defined(_WIN32)
    CloseHandle(to_handle(native_handle_));
#else
    mq_close(static_cast<mqd_t>(native_handle_));
#endif
    native_handle_    = -1;
    max_message_size_ = 0;
    receiver_         = false;
}

StatusResult<MessageQueue> MessageQueue::create(const std::string& name, usize max_message_size)
{
    if (max_message_size == 0)
        return Err(ErrStatus(StatusCode::INVALID_ARGUMENT, "message size must be nonzero"));
#if defined(_WIN32)
    auto path = mailslot_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    HANDLE handle = CreateMailslotW(
        std::move(path).unwrap().c_str(), static_cast<DWORD>(max_message_size), 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return Err(windows_error("CreateMailslotW"));
    return Ok(MessageQueue(to_native(handle), max_message_size, true));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    mq_attr attributes{};
    attributes.mq_maxmsg  = 10;
    attributes.mq_msgsize = static_cast<long>(max_message_size);
    const mqd_t handle =
        mq_open(std::move(path).unwrap().c_str(), O_CREAT | O_EXCL | O_RDWR, 0600, &attributes);
    if (handle == static_cast<mqd_t>(-1))
        return Err(errno == EEXIST
                       ? ErrStatus(StatusCode::ALREADY_EXISTS, "message queue already exists")
                       : posix_error("mq_open"));
    return Ok(MessageQueue(static_cast<std::intptr_t>(handle), max_message_size, true));
#endif
}

StatusResult<MessageQueue> MessageQueue::open(const std::string& name)
{
#if defined(_WIN32)
    auto path = mailslot_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    HANDLE handle = CreateFileW(std::move(path).unwrap().c_str(),
                                GENERIC_WRITE,
                                FILE_SHARE_READ,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return Err(windows_error("CreateFileW"));
    return Ok(MessageQueue(to_native(handle), 0, false));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    const mqd_t handle = mq_open(std::move(path).unwrap().c_str(), O_RDWR);
    if (handle == static_cast<mqd_t>(-1))
        return Err(errno == ENOENT ? ErrStatus(StatusCode::NOT_FOUND, "message queue not found")
                                   : posix_error("mq_open"));
    mq_attr attributes{};
    if (mq_getattr(handle, &attributes) != 0) {
        const auto error = posix_error("mq_getattr");
        mq_close(handle);
        return Err(error);
    }
    return Ok(MessageQueue(
        static_cast<std::intptr_t>(handle), static_cast<usize>(attributes.mq_msgsize), true));
#endif
}

Status MessageQueue::send(const void* data, usize length)
{
    if (native_handle_ == -1)
        return ErrStatus(StatusCode::FAILED_PRECONDITION, "send on a closed message queue");
#if defined(_WIN32)
    DWORD count = 0;
    if (!WriteFile(to_handle(native_handle_), data, static_cast<DWORD>(length), &count, nullptr) ||
        count != length)
        return windows_error("WriteFile");
    return OkStatus();
#else
    if (length > max_message_size_)
        return ErrStatus(StatusCode::OUT_OF_RANGE, "message exceeds queue capacity");
    return mq_send(static_cast<mqd_t>(native_handle_), static_cast<const char*>(data), length, 0) ==
                   0
               ? OkStatus()
               : posix_error("mq_send");
#endif
}
Status MessageQueue::send(const std::string& data)
{
    return send(data.data(), data.size());
}

StatusResult<std::string> MessageQueue::receive()
{
    if (native_handle_ == -1 || !receiver_)
        return Err(
            ErrStatus(StatusCode::FAILED_PRECONDITION, "receive on a sender-only message queue"));
#if defined(_WIN32)
    DWORD next_size = 0;
    if (!GetMailslotInfo(to_handle(native_handle_), nullptr, &next_size, nullptr, nullptr))
        return Err(windows_error("GetMailslotInfo"));
    if (next_size == MAILSLOT_NO_MESSAGE)
        return Err(ErrStatus(StatusCode::UNAVAILABLE, "message queue is empty"));
    std::string result(next_size, '\0');
    DWORD       count = 0;
    if (!ReadFile(to_handle(native_handle_), &result[0], next_size, &count, nullptr))
        return Err(windows_error("ReadFile"));
    result.resize(count);
    return Ok(std::move(result));
#else
    std::string   result(max_message_size_, '\0');
    const ssize_t count =
        mq_receive(static_cast<mqd_t>(native_handle_), &result[0], result.size(), nullptr);
    if (count < 0)
        return Err(posix_error("mq_receive"));
    result.resize(static_cast<usize>(count));
    return Ok(std::move(result));
#endif
}

StatusResult<std::optional<std::string>> MessageQueue::receive_for(
    std::chrono::milliseconds timeout)
{
    if (native_handle_ == -1 || !receiver_)
        return Err(
            ErrStatus(StatusCode::FAILED_PRECONDITION, "receive on a sender-only message queue"));
#if defined(_WIN32)
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        DWORD next_size = 0;
        if (!GetMailslotInfo(to_handle(native_handle_), nullptr, &next_size, nullptr, nullptr))
            return Err(windows_error("GetMailslotInfo"));
        if (next_size != MAILSLOT_NO_MESSAGE) {
            auto value = receive();
            if (value.is_err())
                return Err(value.unwrap_err());
            return Ok(std::optional<std::string>(std::move(value).unwrap()));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return Ok(std::optional<std::string>{});
#else
    timespec deadline{};
    clock_gettime(CLOCK_REALTIME, &deadline);
    const i64 ns = static_cast<i64>(deadline.tv_nsec) + timeout.count() * 1000000;
    deadline.tv_sec += ns / 1000000000;
    deadline.tv_nsec = ns % 1000000000;
    std::string   result(max_message_size_, '\0');
    const ssize_t count = mq_timedreceive(
        static_cast<mqd_t>(native_handle_), &result[0], result.size(), nullptr, &deadline);
    if (count < 0) {
        if (errno == ETIMEDOUT)
            return Ok(std::optional<std::string>{});
        return Err(posix_error("mq_timedreceive"));
    }
    result.resize(static_cast<usize>(count));
    return Ok(std::optional<std::string>(std::move(result)));
#endif
}

}   // namespace ca::process::ipc
