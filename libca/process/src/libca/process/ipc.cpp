#include "libca/process/ipc.hpp"

#include "libca/str/format.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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
#    include <semaphore.h>
#    include <signal.h>
#    include <sys/mman.h>
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
                     ca::str::format_std("{} failed with Windows error {}",
                                         operation,
                                         static_cast<unsigned long>(GetLastError())));
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
                     ca::str::format_std("{} failed: {}", operation, std::strerror(errno)));
}

// POSIX 命名管道在 Linux 上回退为 AF_UNIX socket：把简单名字映射到 /tmp 下
// 固定前缀的套接字文件，禁止含 '/' 防止越权写任意路径。长度上限由 sockaddr_un
// 的 sun_path 决定，超长直接报错（截断会产生不可连接的路径）。
StatusResult<std::string> unix_socket_path(const std::string& name)
{
    if (name.empty() || name.find('/') != std::string::npos)
        return Err(
            ErrStatus(StatusCode::INVALID_ARGUMENT, "named pipe name must be a simple token"));
    const std::string path = ca::str::format_std("/tmp/libca_process_{}.sock", name);
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
    return Ok(ca::str::format_std("/libca_process_{}", name));
}
#endif

// 名字合法性双平台同口径：remove_* 在 Windows 上虽是空操作，仍拒绝路径型名字，
// 避免「Windows 通过、Linux 拒绝」的平台行为分叉。
Status validate_simple_token(const std::string& name)
{
    if (name.empty() || name.find('/') != std::string::npos)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "name must be a simple token");
    return OkStatus();
}

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
    // ReadFile 长度参数是 DWORD：capacity 超其上限会静默截断成错误长度。
    if (capacity > static_cast<usize>(std::numeric_limits<DWORD>::max()))
        return Err(ErrStatus(StatusCode::OUT_OF_RANGE, "read capacity exceeds DWORD limit"));
    DWORD count = 0;
    if (!ReadFile(
            to_handle(native_handle_), buffer, static_cast<DWORD>(capacity), &count, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE)
            return Ok(static_cast<usize>(0));
        return Err(windows_error("ReadFile"));
    }
    return Ok(static_cast<usize>(count));
#else
    // POSIX read 长度超过 SSIZE_MAX 是 UB（POSIX 规定），单次封顶。
    const usize capped =
        std::min<usize>(capacity, static_cast<usize>(std::numeric_limits<ssize_t>::max()));
    const ssize_t count = ::read(to_fd(native_handle_), buffer, capped);
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
        // POSIX write 长度超过 SSIZE_MAX 是 UB，单次封顶（循环继续写剩余部分）。
        const usize   capped = std::min<usize>(
            length - offset, static_cast<usize>(std::numeric_limits<ssize_t>::max()));
        const ssize_t count =
            ::write(to_fd(native_handle_), static_cast<const char*>(data) + offset, capped);
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
    if (handle == INVALID_HANDLE_VALUE) {
        // 同名管道已存在且 nMaxInstances=1 时报 ERROR_PIPE_BUSY（实例数打满），
        // 映射为文档承诺的 ALREADY_EXISTS。
        if (GetLastError() == ERROR_PIPE_BUSY)
            return Err(ErrStatus(StatusCode::ALREADY_EXISTS, "named pipe already exists"));
        return Err(windows_error("CreateNamedPipeW"));
    }
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
        const auto error = errno == EADDRINUSE
                               ? ErrStatus(StatusCode::ALREADY_EXISTS, "named pipe already exists")
                               : posix_error("bind");
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
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return Err(ErrStatus(StatusCode::NOT_FOUND, "named pipe not found"));
        return Err(windows_error("CreateFileW"));
    }
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
        const auto error = errno == ENOENT
                               ? ErrStatus(StatusCode::NOT_FOUND, "named pipe not found")
                               : posix_error("connect");
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
    if (!handle) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return Err(ErrStatus(StatusCode::NOT_FOUND, "shared memory not found"));
        return Err(windows_error("OpenFileMappingW"));
    }
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
    if (!handle) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return Err(ErrStatus(StatusCode::NOT_FOUND, "semaphore not found"));
        return Err(windows_error("OpenSemaphoreW"));
    }
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
    // DWORD-1 作为上限避开溢出（与 MessageQueue::receive_for 同口径）。
    const auto  milliseconds = std::max<i64>(0, timeout.count());
    const DWORD wait        = static_cast<DWORD>(std::min<i64>(
        milliseconds, static_cast<i64>(std::numeric_limits<DWORD>::max() - 1)));
    const DWORD result      = WaitForSingleObject(to_handle(native_handle_), wait);
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
    // ReleaseSemaphore 计数参数是 LONG：超 LONG_MAX 会截断成错误计数，提前拒绝。
    if (count > static_cast<u32>(std::numeric_limits<LONG>::max()))
        return ErrStatus(StatusCode::OUT_OF_RANGE, "semaphore release count exceeds LONG limit");
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
    if (handle == INVALID_HANDLE_VALUE) {
        // 同名 mailslot 已存在时报 ERROR_ALREADY_EXISTS，映射为文档承诺的 ALREADY_EXISTS。
        if (GetLastError() == ERROR_ALREADY_EXISTS)
            return Err(ErrStatus(StatusCode::ALREADY_EXISTS, "message queue already exists"));
        return Err(windows_error("CreateMailslotW"));
    }
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
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return Err(ErrStatus(StatusCode::NOT_FOUND, "message queue not found"));
        return Err(windows_error("CreateFileW"));
    }
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

// ============================================================================
// ShmRingQueue —— 共享内存环形消息队列（写者探活 + 崩溃可恢复）
// ============================================================================

namespace {

// 布局跨平台一致性依赖两件事：全部定宽字段（头部布局图见设计文档）+ lock-free 原子量。
// lock-free（address-free）意味着原子操作不依赖对象地址，各进程映射到不同基址仍成立。
static_assert(std::atomic<u64>::is_always_lock_free, "cross-process atomics must be lock-free");
static_assert(std::atomic<u32>::is_always_lock_free, "cross-process atomics must be lock-free");
static_assert(sizeof(detail::RingHeader) == 72, "RingHeader layout must stay frozen");
static_assert(sizeof(detail::RingSlotHeader) == 16, "RingSlotHeader layout must stay frozen");
static_assert(alignof(detail::RingHeader) == 8, "RingHeader must be 8-byte aligned");
static_assert(alignof(detail::RingSlotHeader) == 8, "RingSlotHeader must be 8-byte aligned");
static_assert(detail::kRingHeaderSize % 64 == 0, "header block must be cache-line aligned");

// 进程身份：pid + 出生戳。出生戳用于抵御 pid 复用造成的存活误判。
struct ProcessIdentity
{
    u64 pid;
    u64 birth;
};

// 机器单调时钟毫秒（同机跨进程可比），作心跳与本地接收时限的统一时间源。
u64 monotonic_millis()
{
#if defined(_WIN32)
    return static_cast<u64>(GetTickCount64());
#else
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<u64>(now.tv_sec) * 1000 + static_cast<u64>(now.tv_nsec) / 1000000;
#endif
}

// 心跳是否已落后超过 timeout_ms。
// 心跳为 0 视为从未刷新（超时）；时钟出现回拨/未来值时保守视为未超时。
bool heartbeat_stale(u64 heartbeat, i64 timeout_ms)
{
    if (heartbeat == 0)
        return true;
    const u64 now = monotonic_millis();
    if (now < heartbeat)
        return false;
    return static_cast<i64>(now - heartbeat) > timeout_ms;
}

#if defined(_WIN32)
u64 filetime_to_u64(const FILETIME& value)
{
    return (static_cast<u64>(value.dwHighDateTime) << 32) | static_cast<u64>(value.dwLowDateTime);
}

u64 current_process_birth()
{
    FILETIME create_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    return GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time)
               ? filetime_to_u64(create_time)
               : 0;
}
#else
// 读 /proc/<pid>/stat 的 starttime（整体第 22 字段，clock tick）。comm 字段可含空格与
// ')'，先定位最后一个 ')'，其后第 1 个字段是 state（整体第 3），第 20 个是 starttime。
u64 read_process_birth(u64 pid)
{
    char path[64]{};
    std::snprintf(path, sizeof(path), "/proc/%llu/stat", static_cast<unsigned long long>(pid));
    std::FILE*  file = std::fopen(path, "r");
    if (file == nullptr)
        return 0;
    char        buffer[1024]{};
    const bool  read_ok = std::fgets(buffer, sizeof(buffer), file) != nullptr;
    std::fclose(file);
    if (!read_ok)
        return 0;
    const char* cursor = std::strrchr(buffer, ')');
    if (cursor == nullptr)
        return 0;
    for (int field = 1; field <= 20; ++field) {
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        if (*cursor == '\0')
            return 0;
        const char* token = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\n')
            ++cursor;
        if (field == 20)
            return std::strtoull(token, nullptr, 10);
    }
    return 0;
}
#endif

ProcessIdentity current_process_identity()
{
#if defined(_WIN32)
    return ProcessIdentity{static_cast<u64>(GetCurrentProcessId()), current_process_birth()};
#else
    const u64 pid = static_cast<u64>(::getpid());
    return ProcessIdentity{pid, read_process_birth(pid)};
#endif
}

// 判定「pid + 出生戳」是否仍是那个存活的原进程。
// 返回 false 表示原写者进程已确认退出（含 pid 被复用的情况）。
//       Windows：OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION) + GetExitCodeProcess
//       判退出，GetProcessTimes 比对出生戳抵御 pid 复用；打开句柄被拒（ACCESS_DENIED）
//       时保守视为存活，宁可推迟接管也不误接管。POSIX：kill(pid, 0) 判存在（EPERM 视
//       为存在），/proc 出生戳比对抵御 pid 复用；/proc 不可读时出生戳记 0，存活判定
//       退化为仅查 pid。
bool process_identity_alive(u64 pid, u64 birth)
{
    if (pid < 2)   // 0/1 不是合法写者 pid（1 与接管哨兵冲突，见 kRingWriterElecting）
        return false;
#if defined(_WIN32)
    HANDLE handle =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr)
        return GetLastError() == ERROR_ACCESS_DENIED;
    FILETIME create_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    bool     alive = true;
    if (GetProcessTimes(handle, &create_time, &exit_time, &kernel_time, &user_time) &&
        birth != 0 && filetime_to_u64(create_time) != birth) {
        CloseHandle(handle);
        return false;   // 出生戳不符：pid 已被复用，原进程必然已退出
    }
    DWORD exit_code = 0;
    alive = GetExitCodeProcess(handle, &exit_code) && exit_code == STILL_ACTIVE;
    CloseHandle(handle);
    return alive;
#else
    if (::kill(static_cast<pid_t>(pid), 0) != 0)
        return errno != ESRCH;   // EPERM：进程存在但无权限，保守视为存活
    if (birth != 0) {
        const u64 now_birth = read_process_birth(pid);
        if (now_birth != 0 && now_birth != birth)
            return false;   // 出生戳不符：pid 已被复用
    }
    return true;
#endif
}

Status validate_ring_options(const ShmRingQueue::Options& options)
{
    if (options.max_message_size == 0 || options.max_message_size > (usize{64} << 20))
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "max_message_size must be in [1, 64 MiB]");
    if (options.slot_count == 0 || options.slot_count > 65536)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "slot_count must be in [1, 65536]");
    if (options.heartbeat_interval.count() < 0)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "heartbeat_interval must be non-negative");
    if (options.heartbeat_timeout.count() <= 0)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "heartbeat_timeout must be positive");
    // 组合上限防溢出：段大小（头部 + 槽区）不得超过 1 GiB。
    if (static_cast<u64>(options.slot_count) *
            static_cast<u64>(detail::ring_slot_stride(options.max_message_size)) >
        (u64{1} << 30))
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "ring queue segment size exceeds 1 GiB");
    return OkStatus();
}

}   // namespace

ShmRingQueue::ShmRingQueue(SharedMemory segment, const Options& options) noexcept
    : segment_(std::move(segment))
    , options_(options)
{}
ShmRingQueue::~ShmRingQueue()
{
    close();
}
ShmRingQueue::ShmRingQueue(ShmRingQueue&& other) noexcept
    : segment_(std::move(other.segment_))
    , options_(other.options_)
    , reader_attached_(other.reader_attached_)
    , reader_generation_(other.reader_generation_)
    , reader_next_seq_(other.reader_next_seq_)
    , writer_claimed_(other.writer_claimed_)
    , writer_pid_(other.writer_pid_)
    , writer_birth_(other.writer_birth_)
    , last_heartbeat_ms_(other.last_heartbeat_ms_)
{
    other.reader_attached_  = false;
    other.reader_generation_ = 0;
    other.reader_next_seq_  = 0;
    other.writer_claimed_   = false;
    other.writer_pid_       = 0;
    other.writer_birth_     = 0;
    other.last_heartbeat_ms_ = 0;
}
ShmRingQueue& ShmRingQueue::operator=(ShmRingQueue&& other) noexcept
{
    if (this != &other) {
        close();
        segment_                 = std::move(other.segment_);
        options_                 = other.options_;
        reader_attached_         = other.reader_attached_;
        reader_generation_       = other.reader_generation_;
        reader_next_seq_         = other.reader_next_seq_;
        writer_claimed_          = other.writer_claimed_;
        writer_pid_              = other.writer_pid_;
        writer_birth_            = other.writer_birth_;
        last_heartbeat_ms_       = other.last_heartbeat_ms_;
        other.reader_attached_   = false;
        other.reader_generation_ = 0;
        other.reader_next_seq_   = 0;
        other.writer_claimed_    = false;
        other.writer_pid_        = 0;
        other.writer_birth_      = 0;
        other.last_heartbeat_ms_ = 0;
    }
    return *this;
}
bool ShmRingQueue::is_open() const noexcept
{
    return segment_.is_open();
}
void ShmRingQueue::close() noexcept
{
    // 优雅释放写者身份：下个写者可立即走快路径认领，无需经历接管判定。
    // 注意这只清空闲标识，真正的互斥由 pid + 出生戳 + 进程存活判定兜底。
    if (segment_.is_open() && writer_claimed_) {
        detail::RingHeader* head = header();
        if (head->writer_slot.load() == writer_pid_ &&
            head->writer_birth.load(std::memory_order_relaxed) == writer_birth_)
            head->writer_slot.store(detail::kRingWriterFree);
    }
    writer_claimed_    = false;
    writer_pid_        = 0;
    writer_birth_      = 0;
    last_heartbeat_ms_ = 0;
    reader_attached_   = false;
    reader_generation_ = 0;
    reader_next_seq_   = 0;
    segment_.close();
}

ipc::detail::RingHeader* ShmRingQueue::header() noexcept
{
    return static_cast<detail::RingHeader*>(segment_.data());
}
ipc::detail::RingSlotHeader* ShmRingQueue::slot_at(u64 sequence) noexcept
{
    // 槽区紧跟头部块，槽内偏移用头部记录的步长（open 时已校验其一致性）。
    char* base = static_cast<char*>(segment_.data());
    return reinterpret_cast<detail::RingSlotHeader*>(
        base + detail::kRingHeaderSize + (sequence % header()->slot_count) * header()->slot_stride);
}
char* ShmRingQueue::slot_payload(detail::RingSlotHeader* slot) noexcept
{
    return reinterpret_cast<char*>(slot) + sizeof(detail::RingSlotHeader);
}

StatusResult<ShmRingQueue> ShmRingQueue::create(const std::string& name)
{
    return create(name, Options{});
}

StatusResult<ShmRingQueue> ShmRingQueue::create(const std::string& name, const Options& options)
{
    if (const Status valid = validate_ring_options(options); valid.is_err())
        return Err(valid);
    const usize size = detail::ring_segment_size(options.slot_count, options.max_message_size);
    auto        segment = SharedMemory::create(name, size);
    if (segment.is_err())
        return Err(segment.unwrap_err());
    ShmRingQueue        queue(std::move(segment).unwrap(), options);
    detail::RingHeader* head = queue.header();
    // 初始化顺序：magic 先行、format_version 最后发布。并发 open() 以版本号判断初始化
    // 是否完成——未完成时 open 返回 FAILED_PRECONDITION，调用方稍后重试即可。
    head->magic            = detail::kRingMagic;
    head->slot_count       = static_cast<u32>(options.slot_count);
    head->slot_stride      = detail::ring_slot_stride(options.max_message_size);
    head->payload_capacity = options.max_message_size;
    head->generation.store(1);
    head->write_seq.store(0);
    head->writer_slot.store(detail::kRingWriterFree);
    head->writer_birth.store(0, std::memory_order_relaxed);
    head->writer_heartbeat.store(0, std::memory_order_relaxed);
    // 段本身保证零填充（Windows 新建映射 / POSIX ftruncate），这里显式清一遍槽头，
    // 不依赖平台零化语义。
    for (u32 index = 0; index < head->slot_count; ++index) {
        detail::RingSlotHeader* slot = queue.slot_at(index);
        slot->state.store(static_cast<u32>(detail::RingSlotState::Empty));
        slot->length.store(0);
        slot->sequence.store(0);
    }
    head->format_version = detail::kRingFormatVersion;   // 最后写入 = 就绪信号
    return Ok(std::move(queue));
}

StatusResult<ShmRingQueue> ShmRingQueue::open(const std::string& name)
{
    return open(name, Options{});
}

StatusResult<ShmRingQueue> ShmRingQueue::open(const std::string& name, const Options& options)
{
    if (const Status valid = validate_ring_options(options); valid.is_err())
        return Err(valid);
    auto segment = SharedMemory::open(name);
    if (segment.is_err())
        return Err(segment.unwrap_err());
    ShmRingQueue        queue(std::move(segment).unwrap(), options);
    detail::RingHeader* head = queue.header();
    if (head->magic != detail::kRingMagic)
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION, "segment is not a ShmRingQueue"));
    if (head->format_version != detail::kRingFormatVersion)
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION,
                             "ring queue is being initialized or uses an incompatible format"));
    // 布局自描述：以头部记录为准做一致性校验，防止拿到被破坏的段。
    if (head->slot_count == 0 || head->slot_count > 65536 || head->payload_capacity == 0 ||
        head->payload_capacity > (usize{64} << 20) ||
        head->slot_stride < sizeof(detail::RingSlotHeader) + head->payload_capacity)
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION, "ring queue header is corrupted"));
    return Ok(std::move(queue));
}

Status ShmRingQueue::ensure_writer_claimed()
{
    const ProcessIdentity self    = current_process_identity();
    const i64             timeout = options_.heartbeat_timeout.count();
    writer_pid_                   = self.pid;
    writer_birth_                 = self.birth;
    for (;;) {
        detail::RingHeader* head = header();
        const u64           held = head->writer_slot.load();
        if (held == self.pid) {
            if (head->writer_birth.load(std::memory_order_relaxed) == self.birth) {
                writer_claimed_ = true;   // 本进程已持有（可能来自同进程其它队列对象）
                return OkStatus();
            }
            // pid 相同但出生戳不符（异常残留）：清成空闲后重读。
            u64 expected = held;
            head->writer_slot.compare_exchange_strong(expected, detail::kRingWriterFree);
            continue;
        }
        if (held == detail::kRingWriterFree) {
            u64 expected = detail::kRingWriterFree;
            if (head->writer_slot.compare_exchange_strong(expected, self.pid)) {
                const u64 now = monotonic_millis();
                head->writer_birth.store(self.birth, std::memory_order_relaxed);
                head->writer_heartbeat.store(now, std::memory_order_relaxed);
                last_heartbeat_ms_ = now;
                writer_claimed_    = true;
                return OkStatus();
            }
            continue;   // 与其它进程竞争认领，重读状态
        }
        if (held == detail::kRingWriterElecting)
            return ErrStatus(StatusCode::UNAVAILABLE,
                             "another process is taking over the ring queue");
        // 其它 pid 持有：仅当「心跳超时且进程已退出」才允许接管。进程活着（哪怕心跳
        // 停更，如写者卡死）绝不接管——双写者会破坏环形槽协议。
        if (!heartbeat_stale(head->writer_heartbeat.load(std::memory_order_relaxed), timeout))
            return ErrStatus(StatusCode::UNAVAILABLE,
                             "another writer is active and its heartbeat is fresh");
        if (process_identity_alive(held, head->writer_birth.load(std::memory_order_relaxed)))
            return ErrStatus(StatusCode::UNAVAILABLE, "another writer is active");
        // 选举：CAS 抢占 ELECTING，同一时刻只允许一个接管者进重置流程。
        u64 expected = held;
        if (head->writer_slot.compare_exchange_strong(expected, detail::kRingWriterElecting)) {
            reset_ring(self.pid, self.birth, true);
            reader_attached_ = false;   // 世代已变，本对象的读者游标需重新接入
            return OkStatus();
        }
        // CAS 失败：占用状态并发变化（释放/其它进程接管），重读。
    }
}

void ShmRingQueue::reset_ring(u64 pid, u64 birth, bool claim) noexcept
{
    detail::RingHeader* head = header();
    // 先抬世代：并发的旧读者在下一次世代校验时立即感知并报错退出（保证见设计文档）。
    head->generation.fetch_add(1);
    for (u32 index = 0; index < head->slot_count; ++index) {
        detail::RingSlotHeader* slot = slot_at(index);
        slot->state.store(static_cast<u32>(detail::RingSlotState::Empty));
        slot->length.store(0);
        slot->sequence.store(0);
    }
    head->write_seq.store(0);
    const u64 now = monotonic_millis();
    if (claim) {
        head->writer_birth.store(birth, std::memory_order_relaxed);
        head->writer_heartbeat.store(now, std::memory_order_relaxed);
        head->writer_slot.store(pid);
        writer_claimed_    = true;
        last_heartbeat_ms_ = now;
    } else {
        head->writer_birth.store(0, std::memory_order_relaxed);
        head->writer_heartbeat.store(0, std::memory_order_relaxed);
        head->writer_slot.store(detail::kRingWriterFree);
    }
}

Status ShmRingQueue::send(const void* data, usize length)
{
    if (!segment_.is_open())
        return ErrStatus(StatusCode::FAILED_PRECONDITION, "send on a closed ring queue");
    if (data == nullptr && length != 0)
        return ErrStatus(StatusCode::INVALID_ARGUMENT, "message data must not be null");
    if (length > options_.max_message_size)
        return ErrStatus(StatusCode::OUT_OF_RANGE, "message exceeds slot payload capacity");
    if (const Status claimed = ensure_writer_claimed(); claimed.is_err())
        return claimed;
    detail::RingHeader* head     = header();
    const u64           sequence = head->write_seq.load();
    detail::RingSlotHeader* slot = slot_at(sequence);
    // 发布协议：先置 Writing 占槽（此后任一时刻崩溃，读者看到的都是可识别的撕裂槽），
    // 再写 payload/length/sequence，最后以 Committed 发布。读者 acquire 到 Committed 后，
    // 其余字段必然完整可见——撕裂只可能停留在 Writing 态，可检测、可跳过。
    slot->state.store(static_cast<u32>(detail::RingSlotState::Writing));
    if (length != 0)
        std::memcpy(slot_payload(slot), data, length);
    slot->length.store(static_cast<u32>(length));
    slot->sequence.store(sequence);
    slot->state.store(static_cast<u32>(detail::RingSlotState::Committed));
    head->write_seq.fetch_add(1);
    refresh_heartbeat();   // 写操作顺带心跳（按 interval 节流）
    return OkStatus();
}
Status ShmRingQueue::send(const std::string& data)
{
    return send(data.data(), data.size());
}

void ShmRingQueue::attach_reader() noexcept
{
    detail::RingHeader* head   = header();
    reader_generation_         = head->generation.load();
    // 从「最旧的保留消息」接入：保留窗为 [write_seq - slot_count, write_seq)，读者落在
    // 窗口起点即接到最旧的未覆盖消息；若写者继续套圈把窗口起点覆盖，receive_for 以
    // DATA_LOSS 显式暴露而不是静默丢消息。
    const u64 written   = head->write_seq.load();
    const u64 retained  = head->slot_count;
    reader_next_seq_    = written > retained ? written - retained : 0;
    reader_attached_    = true;
}

StatusResult<std::optional<std::string>> ShmRingQueue::receive_for(
    std::chrono::milliseconds timeout)
{
    if (!segment_.is_open())
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION, "receive on a closed ring queue"));
    if (!reader_attached_)
        attach_reader();
    detail::RingHeader* head          = header();
    const i64 deadline = static_cast<i64>(monotonic_millis()) + std::max<i64>(0, timeout.count());
    u64         spin_count = 0;
    for (;;) {
        // 世代校验：接管/重置使世代 +1，旧读者显式报错（文档化选择：报错不重同步）。
        if (head->generation.load() != reader_generation_)
            return Err(ErrStatus(StatusCode::FAILED_PRECONDITION,
                                 "ring queue was reset by a takeover; reopen the queue"));
        detail::RingSlotHeader* slot  = slot_at(reader_next_seq_);
        const u32               state = slot->state.load();
        if (state == static_cast<u32>(detail::RingSlotState::Committed)) {
            const u64 sequence = slot->sequence.load();
            if (sequence == reader_next_seq_) {
                const u32 length = slot->length.load();
                if (length > head->payload_capacity)
                    return Err(ErrStatus(StatusCode::DATA_LOSS, "ring slot length is corrupted"));
                std::string message(length, '\0');
                if (length != 0)
                    std::memcpy(&message[0], slot_payload(slot), length);
                reader_next_seq_ += 1;
                return Ok(std::optional<std::string>(std::move(message)));
            }
            // 槽内序号与期望不符：写者套圈覆盖了未读消息，明确报数据丢失而非静默跳读。
            return Err(ErrStatus(
                StatusCode::DATA_LOSS,
                ca::str::format_std("ring reader fell behind: slot sequence {} but expected {}",
                                    sequence,
                                    reader_next_seq_)));
        }
        // Writing / Empty：写者正在写（或死于写中），或尚无新消息。
        const u64 held = head->writer_slot.load();
        if (held != detail::kRingWriterFree && held != detail::kRingWriterElecting &&
            !process_identity_alive(held, head->writer_birth.load(std::memory_order_relaxed))) {
            if (state == static_cast<u32>(detail::RingSlotState::Writing)) {
                // 撕裂槽：写者死于写中。跳过并修复读指针，绝不读出撕裂内容。
                reader_next_seq_ += 1;
                continue;
            }
            // Empty + 写者已退出：消息已排空，给读者一个明确终止条件。
            return Err(
                ErrStatus(StatusCode::UNAVAILABLE, "writer process is dead and the queue is drained"));
        }
        if (static_cast<i64>(monotonic_millis()) >= deadline)
            return Ok(std::optional<std::string>{});
        // 等待策略：先短暂自旋让步（覆盖本机 ping-pong 的时延敏感路径），再进入短睡眠
        // （覆盖跨进程低频流，避免空转烧 CPU）。
        if (spin_count < 64)
            std::this_thread::yield();
        else
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        ++spin_count;
    }
}

StatusResult<std::string> ShmRingQueue::receive()
{
    for (;;) {
        auto pending = receive_for(std::chrono::milliseconds(20));
        if (pending.is_err())
            return Err(pending.unwrap_err());
        std::optional<std::string> message = std::move(pending).unwrap();
        if (message.has_value())
            return Ok(std::move(*message));
    }
}

StatusResult<bool> ShmRingQueue::reset_if_writer_dead()
{
    if (!segment_.is_open())
        return Err(ErrStatus(StatusCode::FAILED_PRECONDITION, "reset on a closed ring queue"));
    const ProcessIdentity self = current_process_identity();
    for (;;) {
        detail::RingHeader* head = header();
        const u64           held = head->writer_slot.load();
        if (held == detail::kRingWriterFree || held == detail::kRingWriterElecting ||
            held == self.pid)
            return Ok(false);
        // 接管门槛与写者认领同口径：心跳超时 且 进程确认退出。
        if (!heartbeat_stale(head->writer_heartbeat.load(std::memory_order_relaxed),
                             options_.heartbeat_timeout.count()))
            return Ok(false);
        if (process_identity_alive(held, head->writer_birth.load(std::memory_order_relaxed)))
            return Ok(false);
        u64 expected = held;
        if (head->writer_slot.compare_exchange_strong(expected, detail::kRingWriterElecting)) {
            reset_ring(self.pid, self.birth, false);   // 只重置，不占写者身份
            reader_attached_ = false;   // 重置后本对象重新接入新流
            return Ok(true);
        }
        // CAS 失败：占用状态并发变化（释放/其它进程接管），重读。
    }
}

bool ShmRingQueue::is_writer_alive()
{
    if (!segment_.is_open())
        return false;
    detail::RingHeader* head = header();
    const u64           held = head->writer_slot.load();
    if (held == detail::kRingWriterFree || held == detail::kRingWriterElecting)
        return false;
    return process_identity_alive(held, head->writer_birth.load(std::memory_order_relaxed));
}

void ShmRingQueue::refresh_heartbeat()
{
    if (!writer_claimed_ || !segment_.is_open())
        return;
    const u64 now = monotonic_millis();
    if (now < last_heartbeat_ms_)
        last_heartbeat_ms_ = now;   // 时钟回拨防御（单调时钟上不应发生）
    if (static_cast<i64>(now - last_heartbeat_ms_) < options_.heartbeat_interval.count())
        return;
    header()->writer_heartbeat.store(now, std::memory_order_relaxed);
    last_heartbeat_ms_ = now;
}

// ============================================================================
// POSIX 命名对象移除
// ============================================================================

#if defined(_WIN32)

// Windows 命名对象随最后一个句柄关闭自动回收，无对应 unlink 概念；
// 但名字合法性校验与 POSIX 同口径（拒绝路径型名字）。
Status remove_shared_memory(const std::string& name)
{
    return validate_simple_token(name);
}
Status remove_semaphore(const std::string& name)
{
    return validate_simple_token(name);
}
Status remove_message_queue(const std::string& name)
{
    return validate_simple_token(name);
}

#else

// 共用骨架：名字规范化 → unlink → ENOENT 视为幂等成功。
Status posix_unlink(const std::string& name, const char* operation,
                    const std::function<int(const std::string&)>& unlink_fn)
{
    auto path_result = posix_shared_memory_name(name);
    if (path_result.is_err())
        return path_result.unwrap_err();
    if (unlink_fn(std::move(path_result).unwrap()) != 0 && errno != ENOENT)
        return posix_error(operation);
    return OkStatus();
}

Status remove_shared_memory(const std::string& name)
{
    return posix_unlink(name, "shm_unlink",
                        [](const std::string& path) { return shm_unlink(path.c_str()); });
}

Status remove_semaphore(const std::string& name)
{
    return posix_unlink(name, "sem_unlink",
                        [](const std::string& path) { return sem_unlink(path.c_str()); });
}

Status remove_message_queue(const std::string& name)
{
    return posix_unlink(name, "mq_unlink",
                        [](const std::string& path) { return mq_unlink(path.c_str()); });
}

#endif

}   // namespace ca::process::ipc
