#include "libca/http/http_error.hpp"

#include <utility>

namespace ca::http {

const char* http_error_kind_name(HttpErrorKind kind) noexcept
{
    switch (kind) {
    case HttpErrorKind::Io: return "Io";
    case HttpErrorKind::InvalidUrl: return "InvalidUrl";
    case HttpErrorKind::InvalidMessage: return "InvalidMessage";
    case HttpErrorKind::InvalidState: return "InvalidState";
    case HttpErrorKind::ExpectationFailed: return "ExpectationFailed";
    case HttpErrorKind::HeaderLimitExceeded: return "HeaderLimitExceeded";
    case HttpErrorKind::BodyLimitExceeded: return "BodyLimitExceeded";
    case HttpErrorKind::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

HttpError::HttpError(HttpErrorKind kind, std::string message,
                     std::optional<io::IoError> io_error) noexcept
    : kind_(kind)
    , message_(std::move(message))
    , io_error_(std::move(io_error))
{}

HttpError HttpError::from_kind(HttpErrorKind kind, std::string message)
{
    return HttpError(kind, std::move(message), std::nullopt);
}

HttpError HttpError::from_io(io::IoError error, std::string operation)
{
    std::string message;
    if (!operation.empty()) {
        message = std::move(operation);
        message += ": ";
    }
    message += error.message();
    return HttpError(HttpErrorKind::Io, std::move(message), std::move(error));
}

HttpErrorKind HttpError::kind() const noexcept
{
    return kind_;
}

const std::string& HttpError::message() const noexcept
{
    return message_;
}

const io::IoError* HttpError::io_error() const noexcept
{
    return io_error_.has_value() ? &*io_error_ : nullptr;
}

std::string HttpError::to_string() const
{
    std::string result = http_error_kind_name(kind_);
    if (!message_.empty()) {
        result += ": ";
        result += message_;
    }
    return result;
}

}   // namespace ca::http
