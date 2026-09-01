#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"
#include "libca/net/address.hpp"
#include "libca/net/socket.hpp"

namespace ca::net {

class TcpListener;

/// @brief 拥有 TCP 连接并实现 io::Reader / io::Writer 的同步字节流。
class TcpStream final : public io::Reader, public io::Writer
{
public:
    /// @brief 接管外部已连接的 TCP socket。
    static io::IoResult<TcpStream> from_socket(OwnedSocket socket);
    static io::IoResult<TcpStream> connect(const SocketAddress& address);
    static io::IoResult<TcpStream> connect(const std::string& host, u16 port);
    static io::IoResult<TcpStream> connect_timeout(const SocketAddress&      address,
                                                   std::chrono::milliseconds timeout);

    /// @brief 在总连接期限内解析主机名并按系统顺序尝试所有地址。
    /// @note 同步 DNS 解析不计入 timeout；timeout 只约束解析完成后的连接尝试。
    static io::IoResult<TcpStream> connect_timeout(const std::string& host, u16 port,
                                                   std::chrono::milliseconds timeout);

    TcpStream(const TcpStream&)                = delete;
    TcpStream& operator=(const TcpStream&)     = delete;
    TcpStream(TcpStream&&) noexcept            = default;
    TcpStream& operator=(TcpStream&&) noexcept = default;
    ~TcpStream() override                      = default;

    io::IoResult<usize> read(u8* buffer, usize capacity) override;
    io::IoResult<usize> write(const u8* data, usize length) override;
    io::IoResult<void>  flush() override;

    io::IoResult<SocketAddress> local_address() const;
    io::IoResult<SocketAddress> peer_address() const;
    io::IoResult<void>          shutdown(Shutdown direction);
    io::IoResult<void>          set_nonblocking(bool enabled);
    io::IoResult<void>          set_read_timeout(std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<void>          set_write_timeout(std::optional<std::chrono::milliseconds> timeout);
    io::IoResult<std::optional<std::chrono::milliseconds>> read_timeout() const;
    io::IoResult<std::optional<std::chrono::milliseconds>> write_timeout() const;
    io::IoResult<void>                                     set_nodelay(bool enabled);
    io::IoResult<bool>                                     nodelay() const;
    io::IoResult<TcpStream>                                try_clone() const;

    bool               is_open() const noexcept;
    RawSocket          native_socket() const noexcept;
    OwnedSocket&       socket() noexcept;
    const OwnedSocket& socket() const noexcept;
    OwnedSocket        into_socket() noexcept;

private:
    explicit TcpStream(OwnedSocket socket) noexcept;

    friend class TcpListener;
    OwnedSocket socket_;
};

/// @brief TcpListener::accept() 返回的连接和对端地址。
struct TcpAcceptResult
{
    TcpStream     stream;
    SocketAddress peer_address;
};

/// @brief TCP listener 在 bind/listen 前应用的创建选项。
struct TcpListenerOptions
{
    /// listen() 的待处理连接队列长度，必须大于 0。
    i32 backlog{128};

    /// 是否在 bind() 前启用 SO_REUSEADDR；默认关闭以保持保守的端口独占行为。
    bool reuse_address{false};

    /// IPv6 socket 的 IPV6_V6ONLY 设置；nullopt 保留操作系统默认值。
    /// @warning IPv4 地址不能指定此选项。
    std::optional<bool> ipv6_only{};
};

/// @brief 拥有监听 socket 的同步 TCP 接收器。
class TcpListener
{
public:
    /// @brief 接管外部已经 bind/listen 的 TCP socket。
    static io::IoResult<TcpListener> from_socket(OwnedSocket socket);

    /// @brief 创建 socket，应用 options，然后执行 bind/listen。
    static io::IoResult<TcpListener> bind(const SocketAddress&      address,
                                          const TcpListenerOptions& options = TcpListenerOptions());

    TcpListener(const TcpListener&)                = delete;
    TcpListener& operator=(const TcpListener&)     = delete;
    TcpListener(TcpListener&&) noexcept            = default;
    TcpListener& operator=(TcpListener&&) noexcept = default;
    ~TcpListener()                                 = default;

    io::IoResult<TcpAcceptResult> accept();
    io::IoResult<SocketAddress>   local_address() const;
    io::IoResult<void>            set_nonblocking(bool enabled);
    io::IoResult<TcpListener>     try_clone() const;

    /// @brief 显式关闭 listener；重复关闭成功返回 Ok。
    io::IoResult<void> close();

    bool        is_open() const noexcept;
    RawSocket   native_socket() const noexcept;
    OwnedSocket into_socket() noexcept;

private:
    explicit TcpListener(OwnedSocket socket) noexcept;

    OwnedSocket socket_;
};

}   // namespace ca::net
