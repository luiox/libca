#pragma once

#include "datatype.hpp"
#include "result.hpp"

#include <string>
#include <utility>

/// @file status.hpp
/// @brief 轻量状态类型。用于统一只需错误码和错误文本的 Result 风格 API。

namespace ca::core {

/// @brief 通用状态码集合，命名参考常见系统/API 错误分类。
enum class StatusCode : i32 {
    OK = 0,
    CANCELLED,
    UNKNOWN,
    INVALID_ARGUMENT,
    DEADLINE_EXCEEDED,
    NOT_FOUND,
    ALREADY_EXISTS,
    PERMISSION_DENIED,
    RESOURCE_EXHAUSTED,
    FAILED_PRECONDITION,
    ABORTED,
    OUT_OF_RANGE,
    UNIMPLEMENTED,
    INTERNAL,
    UNAVAILABLE,
    DATA_LOSS,
};

/// @brief 返回状态码的稳定文本名称。
inline const char* status_code_name(StatusCode code) noexcept {
    switch (code) {
    case StatusCode::OK: return "OK";
    case StatusCode::CANCELLED: return "CANCELLED";
    case StatusCode::UNKNOWN: return "UNKNOWN";
    case StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case StatusCode::DEADLINE_EXCEEDED: return "DEADLINE_EXCEEDED";
    case StatusCode::NOT_FOUND: return "NOT_FOUND";
    case StatusCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
    case StatusCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
    case StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
    case StatusCode::FAILED_PRECONDITION: return "FAILED_PRECONDITION";
    case StatusCode::ABORTED: return "ABORTED";
    case StatusCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
    case StatusCode::UNIMPLEMENTED: return "UNIMPLEMENTED";
    case StatusCode::INTERNAL: return "INTERNAL";
    case StatusCode::UNAVAILABLE: return "UNAVAILABLE";
    case StatusCode::DATA_LOSS: return "DATA_LOSS";
    }
    return "UNKNOWN";
}

/// @brief 返回状态码的 std::string 文本。
inline std::string to_string(StatusCode code) {
    return status_code_name(code);
}

/// @brief 轻量错误状态，OK 表示成功，其它状态码表示失败。
/// @note 需要返回值时使用 `StatusResult<T>`；只需成败时可直接返回 `Status`。
class Status {
public:
    /// @brief 默认构造为 OK。
    Status() = default;

    /// @brief 由状态码和可选消息构造。OK 状态会忽略消息。
    Status(StatusCode code, std::string message = {})
        : code_(code), message_(code == StatusCode::OK ? std::string() : std::move(message)) {}

    /// @brief 构造 OK 状态。
    static Status ok() {
        return Status();
    }

    /// @brief 构造错误状态。若传入 OK，会归一化为 UNKNOWN。
    static Status error(StatusCode code, std::string message = {}) {
        if (code == StatusCode::OK) {
            code = StatusCode::UNKNOWN;
        }
        return Status(code, std::move(message));
    }

    /// @brief 是否为成功状态。
    bool is_ok() const noexcept {
        return code_ == StatusCode::OK;
    }

    /// @brief 是否为错误状态。
    bool is_err() const noexcept {
        return !is_ok();
    }

    /// @brief 返回状态码。
    StatusCode code() const noexcept {
        return code_;
    }

    /// @brief 返回错误消息；OK 状态消息为空。
    const std::string& message() const noexcept {
        return message_;
    }

    /// @brief 转为可读文本。无消息时只返回状态码名称。
    std::string to_string() const {
        if (is_ok() || message_.empty()) {
            return status_code_name(code_);
        }
        return std::string(status_code_name(code_)) + ": " + message_;
    }

private:
    StatusCode code_{StatusCode::OK};
    std::string message_;
};

/// @brief 比较两个状态是否完全相同。
inline bool operator==(const Status& lhs, const Status& rhs) {
    return lhs.code() == rhs.code() && lhs.message() == rhs.message();
}

/// @brief 比较两个状态是否不同。
inline bool operator!=(const Status& lhs, const Status& rhs) {
    return !(lhs == rhs);
}

/// @brief 返回 Status 的可读文本。
inline std::string to_string(const Status& status) {
    return status.to_string();
}

/// @brief 构造 OK 状态的便捷函数。
inline Status OkStatus() {
    return Status::ok();
}

/// @brief 构造错误状态的便捷函数。
inline Status ErrStatus(StatusCode code, std::string message = {}) {
    return Status::error(code, std::move(message));
}

/// @brief 统一使用 Status 作为错误类型的 Result 别名。
template<typename T>
using StatusResult = Result<T, Status>;

/// @brief StatusResult 的语义别名，便于表达“可能返回 T 或 Status 错误”。
template<typename T>
using StatusOr = StatusResult<T>;

} // namespace ca::core

namespace ca {
    using core::ErrStatus;
    using core::OkStatus;
    using core::Status;
    using core::StatusCode;

    template<typename T>
    using StatusResult = core::StatusResult<T>;

    template<typename T>
    using StatusOr = core::StatusOr<T>;
}
