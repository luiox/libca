#include "libca/http/headers.hpp"

#include <algorithm>
#include <utility>

namespace ca::http {
namespace {

bool is_token_character(unsigned char value) noexcept
{
    if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9'))
        return true;
    switch (value) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~': return true;
    default: return false;
    }
}

unsigned char ascii_lower(unsigned char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}

}   // namespace

bool HttpHeaders::valid_name(std::string_view name) noexcept
{
    if (name.empty())
        return false;
    return std::all_of(name.begin(), name.end(), [](char value) {
        return is_token_character(static_cast<unsigned char>(value));
    });
}

bool HttpHeaders::valid_value(std::string_view value) noexcept
{
    return std::all_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte == '\t' || (byte >= 0x20 && byte != 0x7f);
    });
}

HttpResult<void> HttpHeaders::append(std::string name, std::string value)
{
    if (!valid_name(name))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP header name is not a valid token"));
    if (!valid_value(value))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP header value contains a forbidden control byte"));
    entries_.push_back(HttpHeader{std::move(name), std::move(value)});
    return ca::core::Ok();
}

HttpResult<void> HttpHeaders::set(std::string name, std::string value)
{
    if (!valid_name(name))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP header name is not a valid token"));
    if (!valid_value(value))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP header value contains a forbidden control byte"));
    remove(name);
    entries_.push_back(HttpHeader{std::move(name), std::move(value)});
    return ca::core::Ok();
}

usize HttpHeaders::remove(std::string_view name) noexcept
{
    const auto previous_size = entries_.size();
    entries_.erase(
        std::remove_if(entries_.begin(),
                       entries_.end(),
                       [&](const HttpHeader& header) { return name_equals(header.name, name); }),
        entries_.end());
    return previous_size - entries_.size();
}

std::optional<std::string_view> HttpHeaders::get(std::string_view name) const noexcept
{
    for (const auto& header : entries_) {
        if (name_equals(header.name, name))
            return std::string_view(header.value);
    }
    return std::nullopt;
}

std::vector<std::string_view> HttpHeaders::get_all(std::string_view name) const
{
    std::vector<std::string_view> result;
    for (const auto& header : entries_) {
        if (name_equals(header.name, name))
            result.emplace_back(header.value);
    }
    return result;
}

bool HttpHeaders::contains(std::string_view name) const noexcept
{
    return get(name).has_value();
}

usize HttpHeaders::size() const noexcept
{
    return entries_.size();
}

bool HttpHeaders::is_empty() const noexcept
{
    return entries_.empty();
}

const std::vector<HttpHeader>& HttpHeaders::entries() const noexcept
{
    return entries_;
}

bool HttpHeaders::name_equals(std::string_view lhs, std::string_view rhs) noexcept
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

}   // namespace ca::http
