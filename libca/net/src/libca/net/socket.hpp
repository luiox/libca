#pragma once

#include <cstdint>
#include <limits>

#include "libca/io/error.hpp"

namespace ca::net {

/// @brief 跨平台原生 socket 值。
using RawSocket = std::uintptr_t;

/// @brief 返回当前平台使用的无效原生 socket 值。
constexpr RawSocket invalid_raw_socket() noexcept
{
    return std::numeric_limits<RawSocket>::max();
}

/// @brief Windows SOCKET 或 POSIX socket fd 的 move-only RAII 唯一所有者。
///
/// Windows 使用 closesocket()，不能使用 CloseHandle()。POSIX fd 0 是有效 socket。
class OwnedSocket
{
public:
    OwnedSocket() noexcept = default;
    ~OwnedSocket();

    OwnedSocket(const OwnedSocket&)            = delete;
    OwnedSocket& operator=(const OwnedSocket&) = delete;
    OwnedSocket(OwnedSocket&& other) noexcept;
    OwnedSocket& operator=(OwnedSocket&& other) noexcept;

    /// @brief 接管有效原生 socket 的关闭责任。
    static io::IoResult<OwnedSocket> adopt(RawSocket socket);

    /// @brief 创建指向同一 socket 状态的独立所有者。
    io::IoResult<OwnedSocket> duplicate() const;

    bool      is_valid() const noexcept;
    RawSocket get() const noexcept;

    /// @brief 显式关闭；关闭失败后仍放弃所有权，避免重复关闭。
    io::IoResult<void> close();

    /// @brief 放弃 RAII 并把原生值交给调用方。
    RawSocket release() noexcept;

private:
    explicit OwnedSocket(RawSocket socket) noexcept;
    void close_noexcept() noexcept;

    RawSocket socket_{invalid_raw_socket()};
};

/// @brief TCP shutdown 的方向。
enum class Shutdown
{
    Read,
    Write,
    Both
};

/// @brief DNS 和 socket 创建使用的地址族筛选。
enum class AddressFamily
{
    Unspecified,
    Ipv4,
    Ipv6
};

/// @brief DNS 解析提示的 socket 类型。
enum class SocketKind
{
    Stream,
    Datagram
};

}   // namespace ca::net
