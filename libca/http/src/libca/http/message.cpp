#include "libca/http/message.hpp"

namespace ca::http {
namespace {

unsigned char ascii_lower(unsigned char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}

bool ascii_equals(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;
    for (usize index = 0; index < lhs.size(); ++index) {
        if (ascii_lower(static_cast<unsigned char>(lhs[index])) !=
            ascii_lower(static_cast<unsigned char>(rhs[index])))
            return false;
    }
    return true;
}

bool contains_connection_token(const HttpHeaders& headers, std::string_view expected) noexcept
{
    for (const auto& header : headers.entries()) {
        if (!ascii_equals(header.name, "Connection"))
            continue;
        const std::string_view value = header.value;
        usize position = 0;
        while (position <= value.size()) {
            const auto comma = value.find(',', position);
            auto token = value.substr(position,
                                      comma == std::string_view::npos ? value.size() - position
                                                                      : comma - position);
            while (!token.empty() && (token.front() == ' ' || token.front() == '\t'))
                token.remove_prefix(1);
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t'))
                token.remove_suffix(1);
            if (ascii_equals(token, expected))
                return true;
            if (comma == std::string_view::npos)
                break;
            position = comma + 1;
        }
    }
    return false;
}

}   // namespace

const char* http_version_name(HttpVersion version) noexcept
{
    return version == HttpVersion::Http10 ? "HTTP/1.0" : "HTTP/1.1";
}

const char* default_reason_phrase(u16 status) noexcept
{
    switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Content Too Large";
    case 414: return "URI Too Long";
    case 415: return "Unsupported Media Type";
    case 422: return "Unprocessable Content";
    case 426: return "Upgrade Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    default: return "";
    }
}

bool status_forbids_body(u16 status) noexcept
{
    return (status >= 100 && status < 200) || status == 204 || status == 304;
}

bool should_keep_alive(HttpVersion version, const HttpHeaders& headers) noexcept
{
    if (contains_connection_token(headers, "close"))
        return false;
    if (version == HttpVersion::Http11)
        return true;
    return contains_connection_token(headers, "keep-alive");
}

}   // namespace ca::http
