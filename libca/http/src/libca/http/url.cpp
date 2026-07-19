#include "libca/http/url.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

#include "libca/net/address.hpp"

namespace ca::http {
namespace {

bool ascii_equals_ignore_case(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;
    for (usize index = 0; index < lhs.size(); ++index) {
        const auto left  = static_cast<unsigned char>(lhs[index]);
        const auto right = static_cast<unsigned char>(rhs[index]);
        if (std::tolower(left) != std::tolower(right))
            return false;
    }
    return true;
}

bool contains_forbidden_url_byte(std::string_view value) noexcept
{
    return std::any_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte <= 0x20 || byte == 0x7f || character == '\\';
    });
}

bool valid_reg_name(std::string_view host) noexcept
{
    if (host.empty())
        return false;
    return std::all_of(host.begin(), host.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
               (byte >= '0' && byte <= '9') || character == '.' || character == '-' ||
               character == '_';
    });
}

HttpResult<u16> parse_port(std::string_view value)
{
    if (value.empty())
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidUrl, "HTTP URL port is empty"));
    u32 port = 0;
    for (const char character : value) {
        if (character < '0' || character > '9')
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL port contains a non-decimal byte"));
        port = port * 10 + static_cast<u32>(character - '0');
        if (port > std::numeric_limits<u16>::max())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL port exceeds 65535"));
    }
    if (port == 0)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidUrl, "HTTP URL port must be nonzero"));
    return ca::core::Ok(static_cast<u16>(port));
}

}   // namespace

HttpUrl::HttpUrl(HttpScheme scheme, std::string host, u16 port, bool explicit_port,
                 std::string target) noexcept
    : scheme_(scheme)
    , host_(std::move(host))
    , port_(port)
    , explicit_port_(explicit_port)
    , target_(std::move(target))
{}

HttpResult<HttpUrl> HttpUrl::parse(std::string_view value)
{
    if (value.empty() || contains_forbidden_url_byte(value))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidUrl, "HTTP URL is empty or contains a forbidden byte"));

    const auto scheme_end = value.find("://");
    if (scheme_end == std::string_view::npos)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidUrl, "HTTP URL must contain an http or https scheme"));

    HttpScheme scheme;
    if (ascii_equals_ignore_case(value.substr(0, scheme_end), "http"))
        scheme = HttpScheme::Http;
    else if (ascii_equals_ignore_case(value.substr(0, scheme_end), "https"))
        scheme = HttpScheme::Https;
    else
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidUrl, "HTTP URL scheme must be http or https"));

    const auto authority_start = scheme_end + 3;
    const auto target_start     = value.find_first_of("/?#", authority_start);
    const auto authority = value.substr(
        authority_start,
        target_start == std::string_view::npos ? value.size() - authority_start
                                               : target_start - authority_start);
    if (authority.empty() || authority.find('@') != std::string_view::npos)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidUrl, "HTTP URL authority is empty or contains userinfo"));

    std::string host;
    u16         port          = scheme == HttpScheme::Http ? 80 : 443;
    bool        explicit_port = false;
    if (authority.front() == '[') {
        const auto closing = authority.find(']');
        if (closing == std::string_view::npos || closing == 1)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL has an invalid bracketed IPv6 host"));
        host = std::string(authority.substr(1, closing - 1));
        auto parsed_ip = net::IpAddress::parse(host);
        if (parsed_ip.is_err() || !parsed_ip.unwrap().is_ipv6())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL bracketed host is not a valid IPv6 address"));
        const auto suffix = authority.substr(closing + 1);
        if (!suffix.empty()) {
            if (suffix.front() != ':')
                return ca::core::Err(HttpError::from_kind(
                    HttpErrorKind::InvalidUrl, "HTTP URL has bytes after the IPv6 host"));
            auto parsed_port = parse_port(suffix.substr(1));
            if (parsed_port.is_err())
                return ca::core::Err(parsed_port.unwrap_err());
            port          = parsed_port.unwrap();
            explicit_port = true;
        }
    }
    else {
        if (authority.find('[') != std::string_view::npos ||
            authority.find(']') != std::string_view::npos)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL contains unmatched host brackets"));
        const auto colon = authority.rfind(':');
        if (colon != std::string_view::npos) {
            if (authority.find(':') != colon)
                return ca::core::Err(HttpError::from_kind(
                    HttpErrorKind::InvalidUrl, "HTTP URL IPv6 host must use brackets"));
            host = std::string(authority.substr(0, colon));
            auto parsed_port = parse_port(authority.substr(colon + 1));
            if (parsed_port.is_err())
                return ca::core::Err(parsed_port.unwrap_err());
            port          = parsed_port.unwrap();
            explicit_port = true;
        }
        else {
            host = std::string(authority);
        }
        if (!valid_reg_name(host))
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL host contains an unsupported byte"));
    }

    std::string target = "/";
    if (target_start != std::string_view::npos && value[target_start] != '#') {
        const auto fragment = value.find('#', target_start);
        auto target_view = value.substr(
            target_start,
            fragment == std::string_view::npos ? value.size() - target_start
                                               : fragment - target_start);
        if (!target_view.empty() && target_view.front() == '?') {
            target = "/";
            target.append(target_view.data(), target_view.size());
        }
        else if (!target_view.empty()) {
            target.assign(target_view.data(), target_view.size());
        }
    }
    if (target.empty() || target.front() != '/')
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidUrl, "HTTP URL target must use origin-form"));

    return ca::core::Ok(
        HttpUrl(scheme, std::move(host), port, explicit_port, std::move(target)));
}

HttpScheme HttpUrl::scheme() const noexcept
{
    return scheme_;
}

const std::string& HttpUrl::host() const noexcept
{
    return host_;
}

u16 HttpUrl::port() const noexcept
{
    return port_;
}

bool HttpUrl::has_explicit_port() const noexcept
{
    return explicit_port_;
}

const std::string& HttpUrl::target() const noexcept
{
    return target_;
}

std::string HttpUrl::authority() const
{
    std::string result;
    if (host_.find(':') != std::string::npos)
        result = "[" + host_ + "]";
    else
        result = host_;

    const u16 default_port = scheme_ == HttpScheme::Http ? 80 : 443;
    if (port_ != default_port) {
        result += ':';
        result += std::to_string(port_);
    }
    return result;
}

}   // namespace ca::http
