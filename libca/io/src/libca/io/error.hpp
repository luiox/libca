#pragma once

#include <string>

#include "libca/core/result.hpp"
#include "libca/core/status.hpp"

namespace ca::io {

/// @brief 跨平台 IO 错误类别，语义参考 Rust std::io::ErrorKind。
enum class IoErrorKind
{
    NotFound,
    PermissionDenied,
    ConnectionRefused,
    ConnectionReset,
    ConnectionAborted,
    NotConnected,
    AddrInUse,
    AddrNotAvailable,
    BrokenPipe,
    AlreadyExists,
    WouldBlock,
    InvalidInput,
    InvalidData,
    TimedOut,
    WriteZero,
    Interrupted,
    UnexpectedEof,
    OutOfMemory,
    Unsupported,
    Other
};

/// @brief 返回稳定的 IO 错误类别名称。
const char* io_error_kind_name(IoErrorKind kind) noexcept;

/// @brief 同时保存稳定错误类别、原生错误码和诊断文本的 IO 错误。
///
/// 原生系统错误通过 from_native_error() / last_os_error() 创建。库内部产生的 EOF、
/// WriteZero 等错误通过 from_kind() 创建，其 native_code() 为 0。
class IoError
{
public:
    /// @brief 创建不带原生错误码的合成错误。
    static IoError from_kind(IoErrorKind kind, std::string message = {});

    /// @brief 从 errno 或 Windows error 创建错误并保留原始错误码。
    static IoError from_native_error(i64 native_code, std::string operation = {});

    /// @brief 读取当前线程的 errno 或 GetLastError() 并创建错误。
    static IoError last_os_error(std::string operation = {});

    /// @brief 返回稳定错误类别。
    IoErrorKind kind() const noexcept;

    /// @brief 返回原生错误码；合成错误返回 0。
    i64 native_code() const noexcept;

    /// @brief 返回带操作上下文的错误消息。
    const std::string& message() const noexcept;

    /// @brief 返回包含错误类别、原生码和消息的诊断文本。
    std::string to_string() const;

    /// @brief 转换为旧模块使用的通用 Status；会保留诊断文本。
    ca::core::Status to_status() const;

private:
    IoError(IoErrorKind kind, i64 native_code, std::string message) noexcept;

    IoErrorKind kind_{IoErrorKind::Other};
    i64         native_code_{0};
    std::string message_;
};

/// @brief 使用 IoError 作为错误类型的 Result。
template<typename T>
using IoResult = ca::core::Result<T, IoError>;

}   // namespace ca::io
