#include "libca/http/url.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>
#include <vector>

#include "libca/net/address.hpp"

namespace ca::http {
namespace {

constexpr char HEX_UPPER[] = "0123456789ABCDEF";

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

bool is_unreserved_byte(unsigned char byte) noexcept
{
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
           (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' || byte == '_' ||
           byte == '~';
}

bool is_sub_delim_byte(unsigned char byte) noexcept
{
    return byte == '!' || byte == '$' || byte == '&' || byte == '\'' || byte == '(' ||
           byte == ')' || byte == '*' || byte == '+' || byte == ',' || byte == ';' || byte == '=';
}

int hex_digit_value(char character) noexcept
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;
    if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
    return -1;
}

// '%' 之后必须紧跟两个十六进制位（RFC 3986 pct-encoded）。
bool valid_percent_sequences(std::string_view value) noexcept
{
    for (usize index = 0; index < value.size(); ++index) {
        if (value[index] != '%')
            continue;
        if (index + 2 >= value.size())
            return false;
        if (hex_digit_value(value[index + 1]) < 0 || hex_digit_value(value[index + 2]) < 0)
            return false;
        index += 2;
    }
    return true;
}

// RFC 3986 userinfo = *( unreserved / pct-encoded / sub-delims / ":" )。分隔按最后一个
// '@' 进行，这里对 userinfo 内部出现的 '@' 保持宽松；'%' 的序列合法性由
// valid_percent_sequences 单独校验。
bool valid_userinfo(std::string_view value) noexcept
{
    return std::all_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return is_unreserved_byte(byte) || is_sub_delim_byte(byte) || byte == ':' || byte == '@' ||
               byte == '%';
    });
}

std::string ascii_lowercase(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        auto byte = static_cast<unsigned char>(character);
        if (byte >= 'A' && byte <= 'Z')
            byte = static_cast<unsigned char>(byte + ('a' - 'A'));
        result.push_back(static_cast<char>(byte));
    }
    return result;
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

// RFC 3986 §6.2.2.2 percent 编码规范化：unreserved 字节解码，其余统一为大写 %XX。
std::string normalize_percent(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (usize index = 0; index < value.size(); ++index) {
        const char character = value[index];
        if (character != '%') {
            result.push_back(character);
            continue;
        }
        // parse 保证 percent 序列合法。
        const auto byte = static_cast<unsigned char>(hex_digit_value(value[index + 1]) * 16 +
                                                     hex_digit_value(value[index + 2]));
        index += 2;
        if (is_unreserved_byte(byte)) {
            result.push_back(static_cast<char>(byte));
        }
        else {
            result.push_back('%');
            result.push_back(HEX_UPPER[byte >> 4]);
            result.push_back(HEX_UPPER[byte & 0x0f]);
        }
    }
    return result;
}

// 消解 path 中的 "." 与 ".." 完整段（RFC 3986 §5.2.4 语义）："." 丢弃，".." 弹出上一段
// （根之上忽略），其余段含空段原样保留，末尾斜杠语义保持。输入是 parse 产生的绝对 path；
// 相对 RFC 伪代码面向合并后引用的假设，这里对 "/A/../B" 这类直接给定的绝对 path 采用
// 与 WHATWG/Go path.Clean 一致的逐段消解结果。
std::string remove_dot_segments(std::string_view path)
{
    std::vector<std::string_view> kept;
    usize  pos      = (!path.empty() && path.front() == '/') ? 1 : 0;
    bool   trailing = false;
    while (pos < path.size()) {
        const auto slash     = path.find('/', pos);
        const bool has_slash = slash != std::string_view::npos;
        const auto segment =
            path.substr(pos, has_slash ? slash - pos : std::string_view::npos);
        pos = has_slash ? slash + 1 : path.size();
        if (segment == ".") {
            trailing = true;
            continue;
        }
        if (segment == "..") {
            if (!kept.empty())
                kept.pop_back();
            trailing = true;
            continue;
        }
        kept.push_back(segment);
        trailing = has_slash;
    }
    std::string output;
    for (const auto segment : kept) {
        output.push_back('/');
        output.append(segment);
    }
    if (trailing)
        output.push_back('/');
    if (output.empty())
        output = "/";
    return output;
}

}   // namespace

std::string percent_encode(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (is_unreserved_byte(byte)) {
            result.push_back(character);
            continue;
        }
        result.push_back('%');
        result.push_back(HEX_UPPER[byte >> 4]);
        result.push_back(HEX_UPPER[byte & 0x0f]);
    }
    return result;
}

HttpResult<std::string> percent_decode(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (usize index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            result.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size())
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidUrl,
                                                      "percent-encoded sequence is truncated"));
        const int high = hex_digit_value(value[index + 1]);
        const int low  = hex_digit_value(value[index + 2]);
        if (high < 0 || low < 0)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "percent-encoded sequence contains a non-hex byte"));
        result.push_back(static_cast<char>(static_cast<unsigned char>(high * 16 + low)));
        index += 2;
    }
    return ca::core::Ok(std::move(result));
}

HttpResult<std::vector<HttpQueryParam>> parse_query_params(std::string_view query)
{
    std::vector<HttpQueryParam> params;
    usize                       start = 0;
    while (start < query.size()) {
        auto       end     = query.find('&', start);
        const bool last    = end == std::string_view::npos;
        const auto segment = query.substr(start, last ? query.size() - start : end - start);
        start              = last ? query.size() : end + 1;
        if (segment.empty())
            continue;
        const auto equals = segment.find('=');
        if (equals == std::string_view::npos) {
            auto key = percent_decode(segment);
            if (key.is_err())
                return ca::core::Err(std::move(key).unwrap_err());
            HttpQueryParam param;
            param.key = std::move(key).unwrap();
            params.push_back(std::move(param));
            continue;
        }
        auto key = percent_decode(segment.substr(0, equals));
        if (key.is_err())
            return ca::core::Err(std::move(key).unwrap_err());
        auto value = percent_decode(segment.substr(equals + 1));
        if (value.is_err())
            return ca::core::Err(std::move(value).unwrap_err());
        HttpQueryParam param;
        param.key   = std::move(key).unwrap();
        param.value = std::move(value).unwrap();
        params.push_back(std::move(param));
    }
    return ca::core::Ok(std::move(params));
}

HttpUrl::HttpUrl(HttpScheme scheme, std::string userinfo, bool has_userinfo, std::string host,
                 u16 port, bool explicit_port, std::string path, bool has_query,
                 std::string query, bool has_fragment, std::string fragment) noexcept
    : scheme_(scheme)
    , userinfo_(std::move(userinfo))
    , has_userinfo_(has_userinfo)
    , host_(std::move(host))
    , port_(port)
    , explicit_port_(explicit_port)
    , path_(std::move(path))
    , has_query_(has_query)
    , query_(std::move(query))
    , has_fragment_(has_fragment)
    , fragment_(std::move(fragment))
{
    target_ = path_;
    if (has_query_) {
        target_.push_back('?');
        target_ += query_;
    }
}

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
    const auto target_start    = value.find_first_of("/?#", authority_start);
    const auto authority       = value.substr(
        authority_start,
        target_start == std::string_view::npos ? value.size() - authority_start
                                               : target_start - authority_start);
    if (authority.empty())
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidUrl, "HTTP URL authority is empty"));

    // userinfo：按最后一个 '@' 切分；未出现 '@' 时不携带 userinfo。
    std::string      userinfo;
    bool             has_userinfo = false;
    std::string_view host_port    = authority;
    const auto       userinfo_end = authority.rfind('@');
    if (userinfo_end != std::string_view::npos) {
        has_userinfo = true;
        userinfo     = std::string(authority.substr(0, userinfo_end));
        host_port    = authority.substr(userinfo_end + 1);
        if (!valid_userinfo(userinfo) || !valid_percent_sequences(userinfo))
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL userinfo contains an unsupported byte"));
    }

    std::string host;
    u16         port          = scheme == HttpScheme::Http ? 80 : 443;
    bool        explicit_port = false;
    if (host_port.empty())
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidUrl, "HTTP URL host is empty"));
    if (host_port.front() == '[') {
        const auto closing = host_port.find(']');
        if (closing == std::string_view::npos || closing == 1)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL has an invalid bracketed IPv6 host"));
        host = std::string(host_port.substr(1, closing - 1));
        auto parsed_ip = net::IpAddress::parse(host);
        if (parsed_ip.is_err() || !parsed_ip.unwrap().is_ipv6())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL bracketed host is not a valid IPv6 address"));
        const auto suffix = host_port.substr(closing + 1);
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
        if (host_port.find('[') != std::string_view::npos ||
            host_port.find(']') != std::string_view::npos)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL contains unmatched host brackets"));
        const auto colon = host_port.rfind(':');
        if (colon != std::string_view::npos) {
            if (host_port.find(':') != colon)
                return ca::core::Err(HttpError::from_kind(
                    HttpErrorKind::InvalidUrl, "HTTP URL IPv6 host must use brackets"));
            host = std::string(host_port.substr(0, colon));
            auto parsed_port = parse_port(host_port.substr(colon + 1));
            if (parsed_port.is_err())
                return ca::core::Err(parsed_port.unwrap_err());
            port          = parsed_port.unwrap();
            explicit_port = true;
        }
        else {
            host = std::string(host_port);
        }
        if (!valid_reg_name(host))
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidUrl, "HTTP URL host contains an unsupported byte"));
    }

    // path、query 与 fragment 按 '?"/'#' 结构切分；fragment 之后的字节全部属于 fragment。
    std::string path            = "/";
    std::string query;
    std::string fragment;
    bool        has_query    = false;
    bool        has_fragment = false;
    if (target_start != std::string_view::npos) {
        const auto rest = value.substr(target_start);
        std::string_view message_part = rest;
        const auto fragment_split = rest.find('#');
        if (fragment_split != std::string_view::npos) {
            has_fragment = true;
            fragment     = std::string(rest.substr(fragment_split + 1));
            message_part = rest.substr(0, fragment_split);
        }
        std::string_view path_view = message_part;
        if (!message_part.empty() && message_part.front() == '?') {
            path_view = {};
            has_query = true;
            query     = std::string(message_part.substr(1));
        }
        else {
            const auto query_split = message_part.find('?');
            if (query_split != std::string_view::npos) {
                path_view = message_part.substr(0, query_split);
                has_query = true;
                query     = std::string(message_part.substr(query_split + 1));
            }
        }
        if (!path_view.empty())
            path = std::string(path_view);
    }
    if (!valid_percent_sequences(path) || !valid_percent_sequences(query) ||
        !valid_percent_sequences(fragment))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidUrl, "HTTP URL contains an invalid percent-encoded sequence"));

    return ca::core::Ok(HttpUrl(scheme, std::move(userinfo), has_userinfo, std::move(host), port,
                                explicit_port, std::move(path), has_query, std::move(query),
                                has_fragment, std::move(fragment)));
}

HttpScheme HttpUrl::scheme() const noexcept
{
    return scheme_;
}

const std::string& HttpUrl::userinfo() const noexcept
{
    return userinfo_;
}

bool HttpUrl::has_userinfo() const noexcept
{
    return has_userinfo_;
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

const std::string& HttpUrl::path() const noexcept
{
    return path_;
}

bool HttpUrl::has_query() const noexcept
{
    return has_query_;
}

const std::string& HttpUrl::query() const noexcept
{
    return query_;
}

bool HttpUrl::has_fragment() const noexcept
{
    return has_fragment_;
}

const std::string& HttpUrl::fragment() const noexcept
{
    return fragment_;
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

std::vector<HttpQueryParam> HttpUrl::query_params() const
{
    auto parsed = parse_query_params(query_);
    if (parsed.is_err())
        return {};
    return std::move(parsed).unwrap();
}

std::string HttpUrl::to_string() const
{
    std::string result = scheme_ == HttpScheme::Http ? "http://" : "https://";
    if (has_userinfo_) {
        result += userinfo_;
        result.push_back('@');
    }
    if (host_.find(':') != std::string::npos) {
        result.push_back('[');
        result += host_;
        result.push_back(']');
    }
    else {
        result += host_;
    }
    if (explicit_port_) {
        result.push_back(':');
        result += std::to_string(port_);
    }
    result += path_;
    if (has_query_) {
        result.push_back('?');
        result += query_;
    }
    if (has_fragment_) {
        result.push_back('#');
        result += fragment_;
    }
    return result;
}

HttpUrl HttpUrl::normalize() const
{
    const u16 default_port = scheme_ == HttpScheme::Http ? 80 : 443;
    HttpUrl   result(*this);
    result.userinfo_ = normalize_percent(userinfo_);
    result.host_     = ascii_lowercase(host_);
    if (result.explicit_port_ && result.port_ == default_port)
        result.explicit_port_ = false;
    // RFC 3986 §6.2.2 顺序：先规范化 percent 编码，再消解 path 的 dot 段。
    result.path_ = remove_dot_segments(normalize_percent(path_));
    if (result.path_.empty())
        result.path_ = "/";
    result.query_    = normalize_percent(query_);
    result.fragment_ = normalize_percent(fragment_);
    result.target_   = result.path_;
    if (result.has_query_) {
        result.target_.push_back('?');
        result.target_ += result.query_;
    }
    return result;
}

}   // namespace ca::http
