#pragma once

#include <string>
#include <string_view>

#include "libca/core/datatype.hpp"
#include "libca/http/http_error.hpp"

namespace ca::http {

/// @brief HTTP URL scheme。
enum class HttpScheme
{
    Http,
    Https
};

/// @brief 拥有 http/https absolute URL 分解结果的值类型。
class HttpUrl
{
public:
    /// @brief 解析 absolute URL；拒绝 userinfo、空 host、非法端口和控制字符。
    static HttpResult<HttpUrl> parse(std::string_view value);

    /// @brief 返回 URL scheme。
    HttpScheme scheme() const noexcept;

    /// @brief 返回不含 IPv6 方括号的 host。
    const std::string& host() const noexcept;

    /// @brief 返回显式端口或 scheme 默认端口。
    u16 port() const noexcept;

    /// @brief 返回输入是否显式指定端口。
    bool has_explicit_port() const noexcept;

    /// @brief 返回 origin-form path + query，至少为 `/`。
    const std::string& target() const noexcept;

    /// @brief 返回 Host header 使用的 authority，按需补 IPv6 方括号和非默认端口。
    std::string authority() const;

private:
    HttpUrl(HttpScheme scheme, std::string host, u16 port, bool explicit_port,
            std::string target) noexcept;

    HttpScheme  scheme_{HttpScheme::Http};
    std::string host_;
    u16         port_{80};
    bool        explicit_port_{false};
    std::string target_{"/"};
};

}   // namespace ca::http
