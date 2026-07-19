#pragma once

#include <string>

#include "libca/core/bytes.hpp"
#include "libca/http/headers.hpp"

namespace ca::http {

/// @brief 当前 codec 支持的 HTTP wire version。
enum class HttpVersion
{
    Http10,
    Http11
};

/// @brief 返回 HTTP/1.0 或 HTTP/1.1 文本。
const char* http_version_name(HttpVersion version) noexcept;

/// @brief 返回常用状态码的 reason phrase；未知状态码返回空字符串。
const char* default_reason_phrase(u16 status) noexcept;

/// @brief 判断状态码是否禁止携带 response body。
bool status_forbids_body(u16 status) noexcept;

/// @brief 按 HTTP version 和 Connection token 判断报文后是否默认保持连接。
bool should_keep_alive(HttpVersion version, const HttpHeaders& headers) noexcept;

/// @brief HTTP/1 报文解析限制；所有限制必须大于 0。
struct HttpLimits
{
    usize max_start_line_bytes{8192};     ///< request/status line 最大字节数。
    usize max_header_bytes{64 * 1024};    ///< headers 与 trailers 累计最大字节数。
    usize max_header_count{100};          ///< headers 与 trailers 合计最大字段数。
    usize max_body_bytes{8 * 1024 * 1024};///< 解码后 body 最大字节数。
};

/// @brief 完整缓冲的 HTTP request。
struct HttpRequest
{
    std::string     method{"GET"};              ///< 区分大小写的 method token。
    std::string     target{"/"};                ///< request-target 原始文本。
    HttpVersion     version{HttpVersion::Http11};///< HTTP wire version。
    HttpHeaders     headers;                     ///< 普通 headers。
    ca::core::Bytes body;                        ///< 解码 transfer coding 后的 body。
    HttpHeaders     trailers;                    ///< chunked trailers。
};

/// @brief 完整缓冲的 HTTP response。
struct HttpResponse
{
    HttpVersion     version{HttpVersion::Http11};///< HTTP wire version。
    u16             status{200};                 ///< 三位十进制状态码。
    std::string     reason;                      ///< reason phrase；空值由 writer 补常用文本。
    HttpHeaders     headers;                     ///< 普通 headers。
    ca::core::Bytes body;                        ///< 解码 transfer coding 后的 body。
    HttpHeaders     trailers;                    ///< chunked trailers。
};

}   // namespace ca::http
