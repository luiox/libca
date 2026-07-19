#include "libca/process/ipc.hpp"

#include <algorithm>
#include <limits>
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
    // Windows 命名管道必须位于 \\.\pipe\ 命名空间，调用方传简单名字时自动补前缀，
    // 已带前缀则原样保留，避免重复拼接。
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

// POSIX 命名管道在 Linux 上回退为 AF_UNIX socket：把简单名字映射到 /tmp 下
// 固定前缀的套接字文件，禁止含 '/' 防止越权写任意路径。长度上限由 sockaddr_un
// 的 sun_path 决定，超长直接报错（截断会产生不可连接的路径）。
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

// POSIX 共享内存 / 命名信号量 / 消息队列的名字必须以 '/' 开头且不含其它 '/'，
// 否则 shm_open / sem_open / mq_open 会失败。统一加前缀保证合法。
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
    // Windows 命名管道服务端实例：PIPE_ACCESS_DUPLEX 双向，nMaxInstances=1 表示
    // 同名只允许一个实例（多客户端需自行加锁或起多服务端）。这里用阻塞模式
    // (PIPE_WAIT)，accept() 时再被 ConnectNamedPipe 唤醒。
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
    // POSIX 用 AF_UNIX SOCK_STREAM 模拟命名管道。bind 前若已有同名 socket 文件
    // 会失败，所以路径里的 name 必须是简单 token（unix_socket_path 已校验）。
    auto path = unix_socket_path(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    const int handle = socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle < 0)
        return Err(posix_error("socket"));
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, std::move(path).unwrap().c_str(), sizeof(address.sun_path) - 1);
    if (bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        const auto error = posix_error("bind");
        ::close(handle);
        return Err(error);
    }
    if (listen(handle, 1) != 0) {
        const auto error = posix_error("listen");
        ::close(handle);
        unlink(address.sun_path);
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
    // ConnectNamedPipe 在客户端已先连上时会返回 FALSE 且 GetLastError == ERROR_PIPE_CONNECTED，
    // 这是合法的"已连接"状态而非错误，必须显式放行，否则会在客户端先于服务端 connect 的
    // 竞态下误报失败。
    HANDLE handle = to_handle(native_handle_);
    if (!ConnectNamedPipe(handle, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
        return Err(windows_error("ConnectNamedPipe"));
    // Windows 命名管道的实例只能 accept 一次：连接建立后原 server handle 直接
    // 转为 connection handle（句柄不变，角色切换），server 端状态置为已关闭。
    native_handle_ = -1;
    return Ok(NamedPipeConnection(to_native(handle)));
#else
    const int handle = ::accept(to_fd(native_handle_), nullptr, nullptr);
    if (handle < 0)
        return Err(posix_error("accept"));
    // 与 Windows 侧"一次 accept 即关闭 server"对齐：单连接服务端，accept 后关闭
    // 监听 socket 并 unlink 路径（close() 内部完成 unlink）。
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
    if (::connect(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
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
    // Windows 文件映射：view 和 mapping handle 是两个独立对象，都要释放。
    UnmapViewOfFile(data_);
    CloseHandle(to_handle(native_handle_));
#else
    // POSIX：mmap 之后底层 fd 即可关闭，映射独立存活。这里两个都释放；
    // 注意本类不负责 shm_unlink（命名对象回收由创建者决定，见 create()）。
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
    // CreateFileMappingW 即使返回成功句柄，若同名对象已存在也会设 GetLastError = ERROR_ALREADY_EXISTS，
    // 必须用这个标志区分"新建者"与"复用者"——只有新建者才被视为 create 成功。
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
    auto path_result = posix_shared_memory_name(name);
    if (path_result.is_err())
        return Err(path_result.unwrap_err());
    const std::string path = std::move(path_result).unwrap();
    // O_CREAT | O_EXCL 保证 create 语义：已存在则失败（errno=EEXIST）。结合
    // 后续 ftruncate + mmap，失败路径必须回收 shm_unlink 防止泄露空对象。
    const int handle = shm_open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (handle < 0) {
        if (errno == EEXIST)
            return Err(ErrStatus(StatusCode::ALREADY_EXISTS, "shared memory already exists"));
        return Err(posix_error("shm_open"));
    }
    if (ftruncate(handle, static_cast<off_t>(size)) != 0) {
        const auto error = posix_error("ftruncate");
        ::close(handle);
        shm_unlink(path.c_str());
        return Err(error);
    }
    void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
    if (view == MAP_FAILED) {
        const auto error = posix_error("mmap");
        ::close(handle);
        shm_unlink(path.c_str());
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
    // 传 0 给 MapViewOfFile 表示映射整个 mapping 对象，再通过 VirtualQuery 查询
    // 实际 RegionSize——Windows API 不提供"查询命名共享内存大小"的独立接口。
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
    // POSIX 共享内存打开后大小由 fstat 推断；创建者已经 ftruncate 设过，这里只读。
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
    // Windows 命名信号量是内核对象，CloseHandle 即释放当前句柄；引用计数到 0 时
    // 系统回收，无需显式 unlink。
    CloseHandle(to_handle(native_handle_));
#else
    // POSIX：sem_close 仅解除"本进程"对该命名信号量的映射（sem_t*），不影响其它
    // 进程。命名对象的最终回收需要创建者调用 sem_unlink（本类未提供，由调用方
    // 在合适的生命周期点显式处理）。
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
    // Windows WaitForSingleObject 相对超时即可，超时返回 WAIT_TIMEOUT。
    const DWORD result = WaitForSingleObject(to_handle(native_handle_),
                                             static_cast<DWORD>(std::max<i64>(0, timeout.count())));
    if (result == WAIT_OBJECT_0)
        return Ok(true);
    if (result == WAIT_TIMEOUT)
        return Ok(false);
    return Err(windows_error("WaitForSingleObject"));
#else
    // sem_timedwait 用绝对截止时间，需基于 CLOCK_REALTIME 计算 deadline
    // （sem_* 系列不保证支持 monotonic）。先取当前绝对时间再加偏移，纳秒溢出部分进位到秒。
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
    if (max_message_size > static_cast<usize>(std::numeric_limits<DWORD>::max()))
        return Err(ErrStatus(StatusCode::OUT_OF_RANGE, "message size exceeds DWORD capacity"));
    auto path = mailslot_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    // Windows mailslot 是单向的：CreateMailslotW 返回的句柄只能读（服务端角色），
    // 客户端必须用 CreateFileW 以 GENERIC_WRITE 打开同名 slot 写入。故 receiver_=true。
    HANDLE handle = CreateMailslotW(
        std::move(path).unwrap().c_str(), static_cast<DWORD>(max_message_size), 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return Err(windows_error("CreateMailslotW"));
    return Ok(MessageQueue(to_native(handle), max_message_size, true));
#else
    auto path = posix_shared_memory_name(name);
    if (path.is_err())
        return Err(path.unwrap_err());
    // POSIX mq 双向：create 一侧既可 send 也可 receive。mq_maxmsg 固定 10（本类的
    // 容量约定），mq_msgsize 取调用方指定上限，mq_send 时会据此校验单条长度。
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
    // mailslot 客户端：GENERIC_WRITE 打开，FILE_SHARE_READ 允许其它客户端并发写。
    // receiver_=false 与 max_message_size_=0 标记只写状态（receive 会拒绝）。
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
    // POSIX mq 的 open 不带 O_CREAT，要求对象已存在。mq_getattr 查出 msgsize，
    // 这样后续 receive 才知道按多大缓冲接收。receiver_=true（POSIX 双向）。
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
    if (data == nullptr && length != 0)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "message data must not be null");
#if defined(_WIN32)
    if (length > static_cast<usize>(std::numeric_limits<DWORD>::max()))
        return ErrStatus(StatusCode::OUT_OF_RANGE, "message exceeds DWORD capacity");
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
    // mailslot 服务端默认无限等待：MAIRSLOT_WAIT_FOREVER 显式设定（防止之前
    // receive_for 改过超时值残留）。接收缓冲按 max_message_size_ 预分配，ReadFile
    // 后用 count resize 到真实长度。
    if (!SetMailslotInfo(to_handle(native_handle_), MAILSLOT_WAIT_FOREVER))
        return Err(windows_error("SetMailslotInfo"));
    std::string result(max_message_size_, '\0');
    DWORD       count = 0;
    if (!ReadFile(to_handle(native_handle_),
                  &result[0],
                  static_cast<DWORD>(max_message_size_),
                  &count,
                  nullptr))
        return Err(windows_error("ReadFile"));
    result.resize(count);
    return Ok(std::move(result));
#else
    // mq_receive 可能被信号中断（EINTR），需循环重试；缓冲大小必须 >= mq_msgsize，
    // 这里直接用 max_message_size_（由 mq_getattr 获取）。
    std::string   result(max_message_size_, '\0');
    ssize_t count = -1;
    do {
        count = mq_receive(static_cast<mqd_t>(native_handle_), &result[0], result.size(), nullptr);
    } while (count < 0 && errno == EINTR);
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
    // mailslot 超时通过 SetMailslotInfo 设置整个句柄的读超时（毫秒级，影响后续所有
    // ReadFile）。DWORD-1 作为上限避开溢出，ERROR_SEM_TIMEOUT 是 mailslot 专有的
    // 超时码（不是 WAIT_TIMEOUT，因为这里是同步 ReadFile 而非 wait 函数）。
    const auto milliseconds = std::max<i64>(0, timeout.count());
    const DWORD wait = static_cast<DWORD>(std::min<i64>(
        milliseconds, static_cast<i64>(std::numeric_limits<DWORD>::max() - 1)));
    if (!SetMailslotInfo(to_handle(native_handle_), wait))
        return Err(windows_error("SetMailslotInfo"));
    std::string result(max_message_size_, '\0');
    DWORD       count = 0;
    if (!ReadFile(to_handle(native_handle_),
                  &result[0],
                  static_cast<DWORD>(max_message_size_),
                  &count,
                  nullptr)) {
        if (GetLastError() == ERROR_SEM_TIMEOUT)
            return Ok(std::optional<std::string>{});
        return Err(windows_error("ReadFile"));
    }
    result.resize(count);
    return Ok(std::optional<std::string>(std::move(result)));
#else
    // mq_timedwait 用 CLOCK_REALTIME 绝对截止时间。归一化纳秒进位，避免 tv_nsec
    // 超过 1e9 被 mq_timedreceive 拒绝（EINVAL）。
    timespec deadline{};
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0)
        return Err(posix_error("clock_gettime"));
    const i64 milliseconds = std::max<i64>(0, timeout.count());
    const i64 ns = static_cast<i64>(deadline.tv_nsec) + milliseconds * 1000000;
    deadline.tv_sec += ns / 1000000000;
    deadline.tv_nsec = ns % 1000000000;
    std::string   result(max_message_size_, '\0');
    ssize_t count = -1;
    for (;;) {
        count = mq_timedreceive(
            static_cast<mqd_t>(native_handle_), &result[0], result.size(), nullptr, &deadline);
        if (count >= 0)
            break;
        if (errno == ETIMEDOUT)
            return Ok(std::optional<std::string>{});
        if (errno == EINTR)
            continue;
        return Err(posix_error("mq_timedreceive"));
    }
    result.resize(static_cast<usize>(count));
    return Ok(std::optional<std::string>(std::move(result)));
#endif
}

}   // namespace ca::process::ipc
