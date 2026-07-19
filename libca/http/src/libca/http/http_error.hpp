#pragma once

#include <optional>
#include <string>

#include "libca/core/result.hpp"
#include "libca/io/error.hpp"

namespace ca::http {

/// @brief HTTP URL、报文和传输失败的稳定错误类别。
enum class HttpErrorKind
{
    Io,
    InvalidUrl,
    InvalidMessage,
    InvalidState,
    ExpectationFailed,
    HeaderLimitExceeded,
    BodyLimitExceeded,
    Unsupported
};

/// @brief 返回稳定的 HTTP 错误类别名称。
const char* http_error_kind_name(HttpErrorKind kind) noexcept;

/// @brief 同时保存 HTTP 错误类别、诊断文本和可选底层 IO 错误。
class HttpError
{
public:
    /// @brief 创建 URL、协议或限制类错误。
    static HttpError from_kind(HttpErrorKind kind, std::string message);

    /// @brief 包装底层 IO 错误并保留其原生错误信息。
    static HttpError from_io(io::IoError error, std::string operation = {});

    /// @brief 返回稳定错误类别。
    HttpErrorKind kind() const noexcept;

    /// @brief 返回诊断文本。
    const std::string& message() const noexcept;

    /// @brief 返回底层 IO 错误；非传输错误返回 nullptr。
    const io::IoError* io_error() const noexcept;

    /// @brief 返回包含类别、消息和底层错误的诊断字符串。
    std::string to_string() const;

private:
    HttpError(HttpErrorKind kind, std::string message,
              std::optional<io::IoError> io_error) noexcept;

    HttpErrorKind              kind_{HttpErrorKind::InvalidMessage};
    std::string                message_;
    std::optional<io::IoError> io_error_;
};

/// @brief 使用 HttpError 作为错误类型的 Result。
template<typename T>
using HttpResult = ca::core::Result<T, HttpError>;

}   // namespace ca::http
