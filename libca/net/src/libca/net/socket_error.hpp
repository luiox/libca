#pragma once

#include "libca/core/datatype.hpp"
#include "libca/io/error.hpp"

/// @file socket_error.hpp
/// @brief socket 层原生错误码归一化词表。命名空间 `ca::net`。
///
/// socket 层的原生错误码有三个来源：Windows 的 WSAGetLastError()、POSIX 的 errno、
/// 以及非阻塞 connect 的 getsockopt(SO_ERROR)。from_native() 把这些平台相关码映射到
/// 本模块定义的最小归一化集合 SocketError，供调用方做平台无关的错误分诊。
///
/// @note 模块对外错误模型仍是 io::IoError（经 IoResult 返回，保留原生码与系统文本），
///       SocketError 是独立可用的补充词表，不改变任何对外签名。两个词表的对应关系
///       见 to_io_error_kind()。
/// @note h_errno / gethostbyname 不在本模块使用范围内：DNS 走 getaddrinfo，其错误是
///       第三套错误空间（gai 错误码），按设计文档另行映射，不经 from_native()。

namespace ca::net {

/// @brief socket 层归一化错误类别（最小集合）。
enum class SocketError
{
    WouldBlock,         ///< 非阻塞操作暂不可完成（含 EINPROGRESS/EALREADY/WSAEINPROGRESS）
    TimedOut,           ///< 操作超时
    ConnectionRefused,  ///< 连接被拒绝（目标端口无监听或被拒绝）
    ConnectionReset,    ///< 连接被对端重置（含连接中止类错误）
    BrokenPipe,         ///< 管道断裂：对端已关闭仍写入（POSIX EPIPE）
    AddrInUse,          ///< 地址/端口已被占用
    AccessDenied,       ///< 权限不足（WSAEACCES/EACCES/EPERM）
    NotConnected,       ///< socket 未建立连接
    NetworkUnreachable, ///< 网络不可达（含主机不可达）
    Unknown             ///< 未分类错误
};

/// @brief 原生错误码归一化。
///
/// Windows 接受 WSAGetLastError() 返回的 WSA 码（及共用参数空间的 Win32 ERROR_* 码）；
/// POSIX 接受 errno。无法识别的码返回 Unknown。
/// @param native_code 原生错误码。
/// @return 归一化类别。
SocketError from_native(i64 native_code) noexcept;

/// @brief 归一化类别的稳定名称（"WouldBlock".."Unknown"），用于日志与测试。
const char* socket_error_name(SocketError error) noexcept;

/// @brief 桥接到 io::IoErrorKind（模块对外错误词表）。
///
/// SocketError::AccessDenied 对应 io::IoErrorKind::PermissionDenied；
/// NetworkUnreachable 对应 io::IoErrorKind::AddrNotAvailable（io 词表暂无
/// NetworkUnreachable，取语义最接近者）；Unknown 对应 Other。
io::IoErrorKind to_io_error_kind(SocketError error) noexcept;

}  // namespace ca::net
