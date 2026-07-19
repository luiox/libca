#include "libca/http/http1_codec.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

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

std::string_view trim_ows(std::string_view value) noexcept
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool valid_request_target(std::string_view value) noexcept
{
    if (value.empty())
        return false;
    return std::all_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte > 0x20 && byte != 0x7f;
    });
}

HttpResult<HttpVersion> parse_http_version(std::string_view value)
{
    if (value == "HTTP/1.0")
        return ca::core::Ok(HttpVersion::Http10);
    if (value == "HTTP/1.1")
        return ca::core::Ok(HttpVersion::Http11);
    return ca::core::Err(HttpError::from_kind(HttpErrorKind::Unsupported,
                                              "only HTTP/1.0 and HTTP/1.1 are supported"));
}

HttpResult<std::optional<usize>> parse_content_length(const HttpHeaders& headers)
{
    std::optional<usize> parsed_length;
    for (const auto field_value : headers.get_all("Content-Length")) {
        usize position = 0;
        while (position <= field_value.size()) {
            const auto comma = field_value.find(',', position);
            const auto token = trim_ows(field_value.substr(position,
                                                           comma == std::string_view::npos
                                                               ? field_value.size() - position
                                                               : comma - position));
            if (token.empty())
                return ca::core::Err(HttpError::from_kind(
                    HttpErrorKind::InvalidMessage, "Content-Length contains an empty value"));

            usize length = 0;
            for (const char character : token) {
                if (character < '0' || character > '9')
                    return ca::core::Err(
                        HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                             "Content-Length contains a non-decimal byte"));
                const usize digit = static_cast<usize>(character - '0');
                if (length > (std::numeric_limits<usize>::max() - digit) / 10)
                    return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                              "Content-Length overflows usize"));
                length = length * 10 + digit;
            }
            if (parsed_length.has_value() && *parsed_length != length)
                return ca::core::Err(
                    HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                         "conflicting Content-Length values are not allowed"));
            parsed_length = length;

            if (comma == std::string_view::npos)
                break;
            position = comma + 1;
        }
    }
    return ca::core::Ok(parsed_length);
}

HttpResult<bool> parse_chunked_transfer_encoding(const HttpHeaders& headers)
{
    const auto values = headers.get_all("Transfer-Encoding");
    if (values.empty())
        return ca::core::Ok(false);

    std::vector<std::string_view> codings;
    for (const auto value : values) {
        usize position = 0;
        while (position <= value.size()) {
            const auto comma = value.find(',', position);
            const auto token = trim_ows(value.substr(
                position,
                comma == std::string_view::npos ? value.size() - position : comma - position));
            if (token.empty())
                return ca::core::Err(HttpError::from_kind(
                    HttpErrorKind::InvalidMessage, "Transfer-Encoding contains an empty coding"));
            codings.push_back(token);
            if (comma == std::string_view::npos)
                break;
            position = comma + 1;
        }
    }
    if (codings.size() != 1 || !ascii_equals(codings.front(), "chunked"))
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::Unsupported,
                                 "only a single final chunked transfer coding is supported"));
    return ca::core::Ok(true);
}

HttpResult<HttpBodyInfo> body_framing(const HttpHeaders& headers, bool close_delimited)
{
    auto content_length = parse_content_length(headers);
    if (content_length.is_err())
        return ca::core::Err(content_length.unwrap_err());
    auto chunked = parse_chunked_transfer_encoding(headers);
    if (chunked.is_err())
        return ca::core::Err(chunked.unwrap_err());
    if (chunked.unwrap() && content_length.unwrap().has_value())
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                 "Transfer-Encoding and Content-Length cannot appear together"));
    if (chunked.unwrap())
        return ca::core::Ok(HttpBodyInfo{HttpBodyKind::Chunked, 0});
    if (content_length.unwrap().has_value())
        return ca::core::Ok(HttpBodyInfo{HttpBodyKind::ContentLength, *content_length.unwrap()});
    return ca::core::Ok(
        HttpBodyInfo{close_delimited ? HttpBodyKind::CloseDelimited : HttpBodyKind::None, 0});
}

HttpResult<usize> parse_chunk_size(std::string_view chunk_line)
{
    if (!HttpHeaders::valid_value(chunk_line))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "chunk-size line contains a forbidden byte"));
    const auto extension = chunk_line.find(';');
    const auto size_text = chunk_line.substr(0, extension);
    if (size_text.empty())
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidMessage, "chunk-size is empty"));

    usize chunk_size = 0;
    for (const char character : size_text) {
        usize digit;
        if (character >= '0' && character <= '9')
            digit = static_cast<usize>(character - '0');
        else if (character >= 'a' && character <= 'f')
            digit = static_cast<usize>(character - 'a' + 10);
        else if (character >= 'A' && character <= 'F')
            digit = static_cast<usize>(character - 'A' + 10);
        else
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "chunk-size is not hexadecimal"));
        if (chunk_size > (std::numeric_limits<usize>::max() - digit) / 16)
            return ca::core::Err(
                HttpError::from_kind(HttpErrorKind::InvalidMessage, "chunk-size overflows usize"));
        chunk_size = chunk_size * 16 + digit;
    }
    return ca::core::Ok(chunk_size);
}

bool status_forbids_framing(u16 status) noexcept
{
    return (status >= 100 && status < 200) || status == 204;
}

bool forbidden_trailer_name(std::string_view name) noexcept
{
    return ascii_equals(name, "Content-Length") || ascii_equals(name, "Transfer-Encoding") ||
           ascii_equals(name, "Host") || ascii_equals(name, "Trailer");
}

HttpResult<void> validate_trailers(const HttpHeaders& trailers)
{
    for (const auto& trailer : trailers.entries()) {
        if (forbidden_trailer_name(trailer.name))
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage,
                "HTTP trailers cannot contain framing, Host, or Trailer fields"));
    }
    return ca::core::Ok();
}

HttpResult<void> set_content_length(HttpHeaders& headers, usize length)
{
    auto result = headers.set("Content-Length", std::to_string(length));
    if (result.is_err())
        return ca::core::Err(result.unwrap_err());
    return ca::core::Ok();
}

}   // namespace

Http1Reader::Http1Reader(io::Reader& reader, HttpLimits limits) noexcept
    : reader_(&reader)
    , limits_(limits)
{}

HttpResult<void> Http1Reader::validate_limits() const
{
    if (limits_.max_start_line_bytes == 0 || limits_.max_header_bytes == 0 ||
        limits_.max_header_count == 0 || limits_.max_body_bytes == 0)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "all HTTP parsing limits must be greater than zero"));
    return ca::core::Ok();
}

HttpResult<void> Http1Reader::ensure_ready()
{
    if (state_ != ReadState::Ready)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidState,
            "the previous HTTP body must be finished before reading another head"));
    auto valid_limits = validate_limits();
    if (valid_limits.is_err())
        return ca::core::Err(valid_limits.unwrap_err());
    state_ = ReadState::ReadingHead;
    return ca::core::Ok();
}

HttpResult<void> Http1Reader::begin_body(HttpBodyInfo body, usize used_header_bytes,
                                         usize used_header_count)
{
    body_info_         = body;
    body_remaining_    = body.content_length;
    body_decoded_      = 0;
    used_header_bytes_ = used_header_bytes;
    used_header_count_ = used_header_count;
    body_trailers_     = HttpHeaders();

    switch (body.kind) {
    case HttpBodyKind::None: state_ = ReadState::BodyComplete; break;
    case HttpBodyKind::ContentLength:
        if (body.content_length > limits_.max_body_bytes) {
            state_ = ReadState::Failed;
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::BodyLimitExceeded,
                                                      "HTTP body exceeds configured limit"));
        }
        state_ = body.content_length == 0 ? ReadState::BodyComplete : ReadState::FixedBody;
        break;
    case HttpBodyKind::Chunked: state_ = ReadState::ChunkSize; break;
    case HttpBodyKind::CloseDelimited: state_ = ReadState::CloseDelimitedBody; break;
    }
    return ca::core::Ok();
}

HttpResult<bool> Http1Reader::fill_buffer()
{
    if (position_ < filled_)
        return ca::core::Ok(true);
    for (;;) {
        auto read = reader_->read(buffer_.data(), buffer_.size());
        if (read.is_err()) {
            auto error = read.unwrap_err();
            if (error.kind() == io::IoErrorKind::Interrupted)
                continue;
            return ca::core::Err(HttpError::from_io(std::move(error), "read HTTP stream"));
        }
        const usize count = read.unwrap();
        if (count > buffer_.size())
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "Reader returned more bytes than requested"));
        position_ = 0;
        filled_   = count;
        return ca::core::Ok(count != 0);
    }
}

HttpResult<std::optional<u8>> Http1Reader::read_byte()
{
    auto filled = fill_buffer();
    if (filled.is_err())
        return ca::core::Err(filled.unwrap_err());
    if (!filled.unwrap())
        return ca::core::Ok(std::optional<u8>{});
    return ca::core::Ok(std::optional<u8>(buffer_[position_++]));
}

HttpResult<std::optional<std::string>> Http1Reader::read_line(usize limit, bool allow_clean_eof)
{
    std::string line;
    line.reserve(std::min<usize>(limit, 256));
    bool saw_carriage_return = false;
    bool saw_any_byte        = false;
    for (;;) {
        auto next = read_byte();
        if (next.is_err())
            return ca::core::Err(next.unwrap_err());
        if (!next.unwrap().has_value()) {
            if (allow_clean_eof && !saw_any_byte)
                return ca::core::Ok(std::optional<std::string>{});
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "unexpected EOF inside an HTTP line"));
        }

        saw_any_byte   = true;
        const u8 value = *next.unwrap();
        if (saw_carriage_return) {
            if (value != static_cast<u8>('\n'))
                return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                          "HTTP lines must end with CRLF"));
            return ca::core::Ok(std::optional<std::string>(std::move(line)));
        }
        if (value == static_cast<u8>('\r')) {
            saw_carriage_return = true;
            continue;
        }
        if (value == static_cast<u8>('\n'))
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "bare LF is not allowed in HTTP/1"));
        if (line.size() >= limit)
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::HeaderLimitExceeded,
                                                      "HTTP line exceeds configured limit"));
        line.push_back(static_cast<char>(value));
    }
}

HttpResult<HttpHeaders> Http1Reader::read_headers(usize& used_bytes, usize& used_count)
{
    HttpHeaders headers;
    for (;;) {
        if (used_bytes >= limits_.max_header_bytes)
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::HeaderLimitExceeded,
                                                      "HTTP headers exceed configured byte limit"));
        auto line = read_line(limits_.max_header_bytes - used_bytes, false);
        if (line.is_err())
            return ca::core::Err(line.unwrap_err());
        auto        line_value = std::move(line).unwrap();
        std::string value      = std::move(*line_value);
        if (value.size() + 2 > limits_.max_header_bytes - used_bytes)
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::HeaderLimitExceeded,
                                                      "HTTP headers exceed configured byte limit"));
        used_bytes += value.size() + 2;
        if (value.empty())
            return ca::core::Ok(std::move(headers));
        if (value.front() == ' ' || value.front() == '\t')
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage, "obsolete folded HTTP headers are not allowed"));
        if (used_count >= limits_.max_header_count)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::HeaderLimitExceeded, "HTTP header count exceeds configured limit"));

        const auto colon = value.find(':');
        if (colon == std::string::npos)
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "HTTP header line is missing ':'"));
        std::string name(value.substr(0, colon));
        const auto  field_value = trim_ows(std::string_view(value).substr(colon + 1));
        auto        appended    = headers.append(std::move(name), std::string(field_value));
        if (appended.is_err())
            return ca::core::Err(appended.unwrap_err());
        ++used_count;
    }
}

HttpResult<std::optional<HttpRequestHead>> Http1Reader::read_request_head()
{
    auto ready = ensure_ready();
    if (ready.is_err())
        return ca::core::Err(ready.unwrap_err());
    auto line = read_line(limits_.max_start_line_bytes, true);
    if (line.is_err())
        return ca::core::Err(line.unwrap_err());
    if (!line.unwrap().has_value()) {
        state_ = ReadState::Ready;
        return ca::core::Ok(std::optional<HttpRequestHead>{});
    }

    auto        line_value   = std::move(line).unwrap();
    std::string request_line = std::move(*line_value);
    const auto  first_space  = request_line.find(' ');
    const auto  second_space = first_space == std::string::npos
                                   ? std::string::npos
                                   : request_line.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos ||
        request_line.find(' ', second_space + 1) != std::string::npos)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP request line must contain method target version"));

    HttpRequestHead request;
    request.method = request_line.substr(0, first_space);
    request.target = request_line.substr(first_space + 1, second_space - first_space - 1);
    if (!HttpHeaders::valid_name(request.method) || !valid_request_target(request.target))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP request method or target is invalid"));
    auto version = parse_http_version(std::string_view(request_line).substr(second_space + 1));
    if (version.is_err())
        return ca::core::Err(version.unwrap_err());
    request.version = version.unwrap();

    usize used_header_bytes = 0;
    usize used_header_count = 0;
    auto  headers           = read_headers(used_header_bytes, used_header_count);
    if (headers.is_err())
        return ca::core::Err(headers.unwrap_err());
    request.headers = std::move(headers).unwrap();
    if (request.version == HttpVersion::Http11) {
        const auto hosts = request.headers.get_all("Host");
        if (hosts.size() != 1 || trim_ows(hosts.front()).empty() ||
            hosts.front().find(',') != std::string_view::npos)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage,
                "HTTP/1.1 request must contain exactly one nonempty Host header"));
    }
    if (request.version == HttpVersion::Http10 && request.headers.contains("Transfer-Encoding"))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP/1.0 request cannot use Transfer-Encoding"));

    auto framing = body_framing(request.headers, false);
    if (framing.is_err())
        return ca::core::Err(framing.unwrap_err());
    auto begun = begin_body(framing.unwrap(), used_header_bytes, used_header_count);
    if (begun.is_err())
        return ca::core::Err(begun.unwrap_err());
    return ca::core::Ok(std::optional<HttpRequestHead>(std::move(request)));
}

HttpResult<std::optional<HttpResponseHead>> Http1Reader::read_response_head(
    std::string_view request_method)
{
    auto ready = ensure_ready();
    if (ready.is_err())
        return ca::core::Err(ready.unwrap_err());
    auto line = read_line(limits_.max_start_line_bytes, true);
    if (line.is_err())
        return ca::core::Err(line.unwrap_err());
    if (!line.unwrap().has_value()) {
        state_ = ReadState::Ready;
        return ca::core::Ok(std::optional<HttpResponseHead>{});
    }

    auto        line_value  = std::move(line).unwrap();
    std::string status_line = std::move(*line_value);
    const auto  first_space = status_line.find(' ');
    if (first_space == std::string::npos || status_line.size() < first_space + 4)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidMessage, "HTTP status line is incomplete"));
    auto version = parse_http_version(std::string_view(status_line).substr(0, first_space));
    if (version.is_err())
        return ca::core::Err(version.unwrap_err());

    const auto status_text = std::string_view(status_line).substr(first_space + 1, 3);
    if (!std::all_of(status_text.begin(), status_text.end(), [](char value) {
            return value >= '0' && value <= '9';
        }))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP status code must contain three decimal digits"));
    if (status_line.size() > first_space + 4 && status_line[first_space + 4] != ' ')
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP status code must be followed by SP"));

    HttpResponseHead response;
    response.version = version.unwrap();
    response.status  = static_cast<u16>((status_text[0] - '0') * 100 + (status_text[1] - '0') * 10 +
                                       status_text[2] - '0');
    if (response.status < 100)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP status code must be at least 100"));
    if (status_line.size() > first_space + 4)
        response.reason = status_line.substr(first_space + 5);
    if (!HttpHeaders::valid_value(response.reason))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP reason phrase contains a forbidden byte"));

    usize used_header_bytes = 0;
    usize used_header_count = 0;
    auto  headers           = read_headers(used_header_bytes, used_header_count);
    if (headers.is_err())
        return ca::core::Err(headers.unwrap_err());
    response.headers = std::move(headers).unwrap();

    const bool head_response = request_method == "HEAD";
    const bool connect_tunnel =
        request_method == "CONNECT" && response.status >= 200 && response.status < 300;
    if (response.version == HttpVersion::Http10 && response.headers.contains("Transfer-Encoding"))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP/1.0 response cannot use Transfer-Encoding"));
    if (status_forbids_framing(response.status) && (response.headers.contains("Content-Length") ||
                                                    response.headers.contains("Transfer-Encoding")))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage,
            "informational and 204 responses cannot contain body framing fields"));

    HttpBodyInfo framing;
    if (status_forbids_body(response.status) || head_response || connect_tunnel) {
        framing = HttpBodyInfo();
    }
    else {
        auto parsed_framing = body_framing(response.headers, true);
        if (parsed_framing.is_err())
            return ca::core::Err(parsed_framing.unwrap_err());
        framing = parsed_framing.unwrap();
    }
    auto begun = begin_body(framing, used_header_bytes, used_header_count);
    if (begun.is_err())
        return ca::core::Err(begun.unwrap_err());
    return ca::core::Ok(std::optional<HttpResponseHead>(std::move(response)));
}

HttpBodyInfo Http1Reader::body_info() const noexcept
{
    return body_info_;
}

HttpResult<usize> Http1Reader::read_fixed_body_part(u8* output, usize capacity)
{
    auto filled = fill_buffer();
    if (filled.is_err())
        return ca::core::Err(filled.unwrap_err());
    if (!filled.unwrap())
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidMessage, "unexpected EOF inside HTTP body"));
    const usize count = std::min({capacity, body_remaining_, filled_ - position_});
    std::memcpy(output, buffer_.data() + position_, count);
    position_ += count;
    body_remaining_ -= count;
    body_decoded_ += count;
    if (body_remaining_ == 0)
        state_ = ReadState::BodyComplete;
    return ca::core::Ok(count);
}

HttpResult<usize> Http1Reader::read_chunked_body_part(u8* output, usize capacity)
{
    for (;;) {
        if (state_ == ReadState::ChunkSize) {
            auto line = read_line(limits_.max_start_line_bytes, false);
            if (line.is_err())
                return ca::core::Err(line.unwrap_err());
            auto line_value = std::move(line).unwrap();
            auto chunk_size = parse_chunk_size(*line_value);
            if (chunk_size.is_err())
                return ca::core::Err(chunk_size.unwrap_err());
            if (chunk_size.unwrap() == 0) {
                auto trailers = read_headers(used_header_bytes_, used_header_count_);
                if (trailers.is_err())
                    return ca::core::Err(trailers.unwrap_err());
                body_trailers_ = std::move(trailers).unwrap();
                auto valid     = validate_trailers(body_trailers_);
                if (valid.is_err())
                    return ca::core::Err(valid.unwrap_err());
                state_ = ReadState::BodyComplete;
                return ca::core::Ok(static_cast<usize>(0));
            }
            if (chunk_size.unwrap() > limits_.max_body_bytes - body_decoded_)
                return ca::core::Err(
                    HttpError::from_kind(HttpErrorKind::BodyLimitExceeded,
                                         "chunked HTTP body exceeds configured limit"));
            body_remaining_ = chunk_size.unwrap();
            state_          = ReadState::ChunkData;
        }

        if (state_ == ReadState::ChunkData) {
            auto filled = fill_buffer();
            if (filled.is_err())
                return ca::core::Err(filled.unwrap_err());
            if (!filled.unwrap())
                return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                          "unexpected EOF inside HTTP chunk"));
            const usize count = std::min({capacity, body_remaining_, filled_ - position_});
            std::memcpy(output, buffer_.data() + position_, count);
            position_ += count;
            body_remaining_ -= count;
            body_decoded_ += count;
            if (body_remaining_ == 0)
                state_ = ReadState::ChunkDataEnding;
            return ca::core::Ok(count);
        }

        auto carriage_return = read_byte();
        if (carriage_return.is_err())
            return ca::core::Err(carriage_return.unwrap_err());
        auto line_feed = read_byte();
        if (line_feed.is_err())
            return ca::core::Err(line_feed.unwrap_err());
        if (!carriage_return.unwrap().has_value() || !line_feed.unwrap().has_value() ||
            *carriage_return.unwrap() != static_cast<u8>('\r') ||
            *line_feed.unwrap() != static_cast<u8>('\n'))
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "chunk data must be followed by CRLF"));
        state_ = ReadState::ChunkSize;
    }
}

HttpResult<usize> Http1Reader::read_close_delimited_body_part(u8* output, usize capacity)
{
    auto filled = fill_buffer();
    if (filled.is_err())
        return ca::core::Err(filled.unwrap_err());
    if (!filled.unwrap()) {
        state_ = ReadState::BodyComplete;
        return ca::core::Ok(static_cast<usize>(0));
    }
    const usize count = std::min(capacity, filled_ - position_);
    if (count > limits_.max_body_bytes - body_decoded_)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::BodyLimitExceeded,
                                 "close-delimited HTTP body exceeds configured limit"));
    std::memcpy(output, buffer_.data() + position_, count);
    position_ += count;
    body_decoded_ += count;
    return ca::core::Ok(count);
}

HttpResult<usize> Http1Reader::read_body(u8* buffer, usize capacity)
{
    if (state_ == ReadState::Ready || state_ == ReadState::ReadingHead ||
        state_ == ReadState::Failed)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "there is no readable HTTP body"));
    if (state_ == ReadState::BodyComplete || capacity == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (buffer == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP body output buffer is null"));

    HttpResult<usize> result =
        state_ == ReadState::FixedBody            ? read_fixed_body_part(buffer, capacity)
        : state_ == ReadState::CloseDelimitedBody ? read_close_delimited_body_part(buffer, capacity)
                                                  : read_chunked_body_part(buffer, capacity);
    if (result.is_err())
        state_ = ReadState::Failed;
    return result;
}

bool Http1Reader::body_finished() const noexcept
{
    return state_ == ReadState::BodyComplete;
}

HttpResult<HttpHeaders> Http1Reader::finish_body()
{
    if (state_ != ReadState::BodyComplete)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidState, "HTTP body has not reached its message boundary"));
    HttpHeaders trailers = std::move(body_trailers_);
    body_trailers_       = HttpHeaders();
    body_info_           = HttpBodyInfo();
    body_remaining_      = 0;
    body_decoded_        = 0;
    used_header_bytes_   = 0;
    used_header_count_   = 0;
    state_               = ReadState::Ready;
    return ca::core::Ok(std::move(trailers));
}

HttpResult<HttpHeaders> Http1Reader::discard_body()
{
    if (state_ == ReadState::Ready || state_ == ReadState::ReadingHead ||
        state_ == ReadState::Failed)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "there is no discardable HTTP body"));
    std::array<u8, 8192> discarded{};
    while (!body_finished()) {
        auto read = read_body(discarded.data(), discarded.size());
        if (read.is_err())
            return ca::core::Err(read.unwrap_err());
    }
    return finish_body();
}

HttpResult<ca::core::Bytes> Http1Reader::read_body_all(HttpHeaders& trailers)
{
    const usize          expected_body_size = body_info_.kind == HttpBodyKind::ContentLength
                                                  ? body_info_.content_length
                                                  : limits_.max_body_bytes;
    const usize          initial_capacity   = std::min<usize>(expected_body_size, 8192);
    auto                 output             = ca::core::BytesMut::with_capacity(initial_capacity);
    std::array<u8, 8192> chunk{};
    while (!body_finished()) {
        auto read = read_body(chunk.data(), chunk.size());
        if (read.is_err())
            return ca::core::Err(read.unwrap_err());
        if (read.unwrap() != 0)
            output.put_slice(chunk.data(), read.unwrap());
    }
    auto finished = finish_body();
    if (finished.is_err())
        return ca::core::Err(finished.unwrap_err());
    trailers = std::move(finished).unwrap();
    return ca::core::Ok(output.freeze());
}

HttpResult<std::optional<HttpRequest>> Http1Reader::read_request()
{
    auto head_result = read_request_head();
    if (head_result.is_err())
        return ca::core::Err(head_result.unwrap_err());
    auto head = std::move(head_result).unwrap();
    if (!head.has_value())
        return ca::core::Ok(std::optional<HttpRequest>{});

    HttpRequest request;
    request.method  = std::move(head->method);
    request.target  = std::move(head->target);
    request.version = head->version;
    request.headers = std::move(head->headers);
    auto body       = read_body_all(request.trailers);
    if (body.is_err())
        return ca::core::Err(body.unwrap_err());
    request.body = std::move(body).unwrap();
    return ca::core::Ok(std::optional<HttpRequest>(std::move(request)));
}

HttpResult<std::optional<HttpResponse>> Http1Reader::read_response(std::string_view request_method)
{
    auto head_result = read_response_head(request_method);
    if (head_result.is_err())
        return ca::core::Err(head_result.unwrap_err());
    auto head = std::move(head_result).unwrap();
    if (!head.has_value())
        return ca::core::Ok(std::optional<HttpResponse>{});

    HttpResponse response;
    response.version = head->version;
    response.status  = head->status;
    response.reason  = std::move(head->reason);
    response.headers = std::move(head->headers);
    auto body        = read_body_all(response.trailers);
    if (body.is_err())
        return ca::core::Err(body.unwrap_err());
    response.body = std::move(body).unwrap();
    return ca::core::Ok(std::optional<HttpResponse>(std::move(response)));
}

usize Http1Reader::buffered_len() const noexcept
{
    return filled_ - position_;
}

Http1ChunkedBodyWriter::Http1ChunkedBodyWriter(Http1Writer& owner) noexcept
    : owner_(&owner)
{}

Http1ChunkedBodyWriter::Http1ChunkedBodyWriter(Http1ChunkedBodyWriter&& other) noexcept
    : owner_(other.owner_)
    , finished_(other.finished_)
{
    other.owner_    = nullptr;
    other.finished_ = true;
}

HttpResult<void> Http1ChunkedBodyWriter::write_chunk(const u8* data, usize length)
{
    if (owner_ == nullptr || finished_ || !owner_->chunked_body_active_)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP chunked body writer is not active"));
    if (length == 0)
        return ca::core::Ok();
    if (data == nullptr)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidState, "HTTP chunk data pointer is null"));

    char       size_buffer[2 * sizeof(usize)]{};
    const auto converted =
        std::to_chars(size_buffer, size_buffer + sizeof(size_buffer), length, 16);
    if (converted.ec != std::errc{})
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "failed to encode HTTP chunk size"));
    auto size = owner_->write_bytes(std::string_view(size_buffer, converted.ptr - size_buffer));
    if (size.is_err())
        return ca::core::Err(size.unwrap_err());
    auto prefix_ending = owner_->write_bytes("\r\n");
    if (prefix_ending.is_err())
        return ca::core::Err(prefix_ending.unwrap_err());
    auto written = owner_->writer_->write_all(data, length);
    if (written.is_err())
        return ca::core::Err(HttpError::from_io(written.unwrap_err(), "write HTTP chunk"));
    return owner_->write_bytes("\r\n");
}

HttpResult<void> Http1ChunkedBodyWriter::write_chunk(const ca::core::Bytes& data)
{
    return write_chunk(data.as_ptr(), data.remaining());
}

HttpResult<void> Http1ChunkedBodyWriter::write_chunk(std::string_view data)
{
    return write_chunk(reinterpret_cast<const u8*>(data.data()), data.size());
}

HttpResult<void> Http1ChunkedBodyWriter::flush()
{
    if (owner_ == nullptr || finished_ || !owner_->chunked_body_active_)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP chunked body writer is not active"));
    auto flushed = owner_->writer_->flush();
    if (flushed.is_err())
        return ca::core::Err(HttpError::from_io(flushed.unwrap_err(), "flush HTTP chunks"));
    return ca::core::Ok();
}

HttpResult<void> Http1ChunkedBodyWriter::finish(const HttpHeaders& trailers)
{
    if (owner_ == nullptr || finished_ || !owner_->chunked_body_active_)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidState,
                                                  "HTTP chunked body writer is not active"));
    auto valid = validate_trailers(trailers);
    if (valid.is_err())
        return ca::core::Err(valid.unwrap_err());
    auto final_chunk = owner_->write_bytes("0\r\n");
    if (final_chunk.is_err())
        return ca::core::Err(final_chunk.unwrap_err());
    auto fields = owner_->write_headers(trailers);
    if (fields.is_err())
        return ca::core::Err(fields.unwrap_err());
    auto ending = owner_->write_bytes("\r\n");
    if (ending.is_err())
        return ca::core::Err(ending.unwrap_err());
    owner_->chunked_body_active_ = false;
    owner_                       = nullptr;
    finished_                    = true;
    return ca::core::Ok();
}

bool Http1ChunkedBodyWriter::is_finished() const noexcept
{
    return finished_;
}

Http1Writer::Http1Writer(io::Writer& writer) noexcept
    : writer_(&writer)
{}

HttpResult<void> Http1Writer::ensure_idle() const
{
    if (chunked_body_active_)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidState,
            "finish the active HTTP chunked body before writing another message"));
    return ca::core::Ok();
}

HttpResult<void> Http1Writer::write_bytes(std::string_view value)
{
    if (value.empty())
        return ca::core::Ok();
    auto written = writer_->write_all(reinterpret_cast<const u8*>(value.data()), value.size());
    if (written.is_err())
        return ca::core::Err(HttpError::from_io(written.unwrap_err(), "write HTTP stream"));
    return ca::core::Ok();
}

HttpResult<void> Http1Writer::write_headers(const HttpHeaders& headers)
{
    for (const auto& header : headers.entries()) {
        auto name = write_bytes(header.name);
        if (name.is_err())
            return ca::core::Err(name.unwrap_err());
        auto separator = write_bytes(": ");
        if (separator.is_err())
            return ca::core::Err(separator.unwrap_err());
        auto value = write_bytes(header.value);
        if (value.is_err())
            return ca::core::Err(value.unwrap_err());
        auto ending = write_bytes("\r\n");
        if (ending.is_err())
            return ca::core::Err(ending.unwrap_err());
    }
    return ca::core::Ok();
}

HttpResult<void> Http1Writer::write_body(const ca::core::Bytes& body, const HttpHeaders& trailers,
                                         bool chunked, bool send_body)
{
    if (!send_body)
        return ca::core::Ok();
    const usize length = body.remaining();
    if (!chunked) {
        if (length == 0)
            return ca::core::Ok();
        auto written = writer_->write_all(body.as_ptr(), length);
        if (written.is_err())
            return ca::core::Err(HttpError::from_io(written.unwrap_err(), "write HTTP body"));
        return ca::core::Ok();
    }

    if (length != 0) {
        char       size_buffer[2 * sizeof(usize)]{};
        const auto converted =
            std::to_chars(size_buffer, size_buffer + sizeof(size_buffer), length, 16);
        if (converted.ec != std::errc{})
            return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                      "failed to encode HTTP chunk size"));
        auto size = write_bytes(std::string_view(size_buffer, converted.ptr - size_buffer));
        if (size.is_err())
            return ca::core::Err(size.unwrap_err());
        auto prefix_ending = write_bytes("\r\n");
        if (prefix_ending.is_err())
            return ca::core::Err(prefix_ending.unwrap_err());
        auto written = writer_->write_all(body.as_ptr(), length);
        if (written.is_err())
            return ca::core::Err(HttpError::from_io(written.unwrap_err(), "write HTTP chunk"));
        auto chunk_ending = write_bytes("\r\n");
        if (chunk_ending.is_err())
            return ca::core::Err(chunk_ending.unwrap_err());
    }

    auto final_chunk = write_bytes("0\r\n");
    if (final_chunk.is_err())
        return ca::core::Err(final_chunk.unwrap_err());
    auto trailer_fields = write_headers(trailers);
    if (trailer_fields.is_err())
        return ca::core::Err(trailer_fields.unwrap_err());
    return write_bytes("\r\n");
}

HttpResult<void> Http1Writer::write_request(const HttpRequest& request)
{
    auto idle = ensure_idle();
    if (idle.is_err())
        return ca::core::Err(idle.unwrap_err());
    if (!HttpHeaders::valid_name(request.method) || !valid_request_target(request.target))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP request method or target is invalid"));
    if (request.version == HttpVersion::Http11) {
        const auto hosts = request.headers.get_all("Host");
        if (hosts.size() != 1 || trim_ows(hosts.front()).empty() ||
            hosts.front().find(',') != std::string_view::npos)
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage,
                "HTTP/1.1 request must contain exactly one nonempty Host header"));
    }
    if (request.version == HttpVersion::Http10 && request.headers.contains("Transfer-Encoding"))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP/1.0 request cannot use Transfer-Encoding"));
    auto valid_trailers = validate_trailers(request.trailers);
    if (valid_trailers.is_err())
        return ca::core::Err(valid_trailers.unwrap_err());

    HttpHeaders headers        = request.headers;
    auto        framing_result = body_framing(headers, false);
    if (framing_result.is_err())
        return ca::core::Err(framing_result.unwrap_err());
    auto framing = framing_result.unwrap();
    if (framing.kind == HttpBodyKind::None && request.body.remaining() != 0) {
        auto length = set_content_length(headers, request.body.remaining());
        if (length.is_err())
            return ca::core::Err(length.unwrap_err());
        framing = HttpBodyInfo{HttpBodyKind::ContentLength, request.body.remaining()};
    }
    if (framing.kind == HttpBodyKind::ContentLength &&
        framing.content_length != request.body.remaining())
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "Content-Length does not match HTTP request body"));
    if (framing.kind != HttpBodyKind::Chunked && !request.trailers.is_empty())
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP trailers require chunked transfer coding"));

    std::string start_line =
        request.method + " " + request.target + " " + http_version_name(request.version) + "\r\n";
    auto start = write_bytes(start_line);
    if (start.is_err())
        return ca::core::Err(start.unwrap_err());
    auto fields = write_headers(headers);
    if (fields.is_err())
        return ca::core::Err(fields.unwrap_err());
    auto end = write_bytes("\r\n");
    if (end.is_err())
        return ca::core::Err(end.unwrap_err());
    return write_body(request.body, request.trailers, framing.kind == HttpBodyKind::Chunked, true);
}

HttpResult<void> Http1Writer::write_response(const HttpResponse& response,
                                             std::string_view    request_method)
{
    auto idle = ensure_idle();
    if (idle.is_err())
        return ca::core::Err(idle.unwrap_err());
    if (response.status < 100 || response.status > 999 ||
        !HttpHeaders::valid_value(response.reason))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP response status or reason phrase is invalid"));
    auto valid_trailers = validate_trailers(response.trailers);
    if (valid_trailers.is_err())
        return ca::core::Err(valid_trailers.unwrap_err());

    const bool head_response = request_method == "HEAD";
    const bool connect_tunnel =
        request_method == "CONNECT" && response.status >= 200 && response.status < 300;
    const bool status_without_body = status_forbids_body(response.status);
    if ((status_without_body || connect_tunnel) && response.body.remaining() != 0)
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP response status or CONNECT tunnel forbids body"));

    HttpHeaders headers   = response.headers;
    const bool  send_body = !status_without_body && !head_response && !connect_tunnel;
    if (response.version == HttpVersion::Http10 && headers.contains("Transfer-Encoding"))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP/1.0 response cannot use Transfer-Encoding"));
    if ((status_forbids_framing(response.status) || connect_tunnel) &&
        (headers.contains("Content-Length") || headers.contains("Transfer-Encoding")))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage,
            "response status or CONNECT tunnel cannot contain body framing fields"));
    if (!send_body && !response.trailers.is_empty())
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "a response without a body cannot contain trailers"));

    bool chunked = false;
    if (send_body) {
        auto framing_result = body_framing(headers, false);
        if (framing_result.is_err())
            return ca::core::Err(framing_result.unwrap_err());
        auto framing = framing_result.unwrap();
        if (framing.kind == HttpBodyKind::None) {
            auto length = set_content_length(headers, response.body.remaining());
            if (length.is_err())
                return ca::core::Err(length.unwrap_err());
            framing = HttpBodyInfo{HttpBodyKind::ContentLength, response.body.remaining()};
        }
        if (framing.kind == HttpBodyKind::ContentLength &&
            framing.content_length != response.body.remaining())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage, "Content-Length does not match HTTP response body"));
        if (framing.kind != HttpBodyKind::Chunked && !response.trailers.is_empty())
            return ca::core::Err(HttpError::from_kind(
                HttpErrorKind::InvalidMessage, "HTTP trailers require chunked transfer coding"));
        chunked = framing.kind == HttpBodyKind::Chunked;
    }
    else if (head_response && !headers.contains("Content-Length") &&
             !headers.contains("Transfer-Encoding")) {
        auto length = set_content_length(headers, response.body.remaining());
        if (length.is_err())
            return ca::core::Err(length.unwrap_err());
    }

    const char* reason =
        response.reason.empty() ? default_reason_phrase(response.status) : response.reason.c_str();
    std::string start_line = http_version_name(response.version);
    start_line += ' ';
    start_line += std::to_string(response.status);
    if (*reason != '\0') {
        start_line += ' ';
        start_line += reason;
    }
    start_line += "\r\n";
    auto start = write_bytes(start_line);
    if (start.is_err())
        return ca::core::Err(start.unwrap_err());
    auto fields = write_headers(headers);
    if (fields.is_err())
        return ca::core::Err(fields.unwrap_err());
    auto end = write_bytes("\r\n");
    if (end.is_err())
        return ca::core::Err(end.unwrap_err());
    return write_body(response.body, response.trailers, chunked, send_body);
}

HttpResult<Http1ChunkedBodyWriter> Http1Writer::begin_chunked_request(
    const HttpRequestHead& request)
{
    auto idle = ensure_idle();
    if (idle.is_err())
        return ca::core::Err(idle.unwrap_err());
    if (!HttpHeaders::valid_name(request.method) || !valid_request_target(request.target))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "HTTP request method or target is invalid"));
    if (request.version != HttpVersion::Http11)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::Unsupported, "chunked requests require HTTP/1.1"));
    const auto hosts = request.headers.get_all("Host");
    if (hosts.size() != 1 || trim_ows(hosts.front()).empty() ||
        hosts.front().find(',') != std::string_view::npos)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                 "HTTP/1.1 request must contain exactly one nonempty Host header"));
    if (request.headers.contains("Content-Length"))
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "chunked request cannot contain Content-Length"));

    HttpHeaders headers = request.headers;
    auto        chunked = parse_chunked_transfer_encoding(headers);
    if (chunked.is_err())
        return ca::core::Err(chunked.unwrap_err());
    if (!chunked.unwrap()) {
        auto appended = headers.append("Transfer-Encoding", "chunked");
        if (appended.is_err())
            return ca::core::Err(appended.unwrap_err());
    }

    std::string start_line =
        request.method + " " + request.target + " " + http_version_name(request.version) + "\r\n";
    auto start = write_bytes(start_line);
    if (start.is_err())
        return ca::core::Err(start.unwrap_err());
    auto fields = write_headers(headers);
    if (fields.is_err())
        return ca::core::Err(fields.unwrap_err());
    auto end = write_bytes("\r\n");
    if (end.is_err())
        return ca::core::Err(end.unwrap_err());
    chunked_body_active_ = true;
    return ca::core::Ok(Http1ChunkedBodyWriter(*this));
}

HttpResult<Http1ChunkedBodyWriter> Http1Writer::begin_chunked_response(
    const HttpResponseHead& response, std::string_view request_method)
{
    auto idle = ensure_idle();
    if (idle.is_err())
        return ca::core::Err(idle.unwrap_err());
    if (response.status < 100 || response.status > 999 ||
        !HttpHeaders::valid_value(response.reason))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "HTTP response status or reason phrase is invalid"));
    if (response.version != HttpVersion::Http11)
        return ca::core::Err(
            HttpError::from_kind(HttpErrorKind::Unsupported, "chunked responses require HTTP/1.1"));
    const bool head_response = request_method == "HEAD";
    const bool connect_tunnel =
        request_method == "CONNECT" && response.status >= 200 && response.status < 300;
    if (status_forbids_body(response.status) || head_response || connect_tunnel)
        return ca::core::Err(HttpError::from_kind(HttpErrorKind::InvalidMessage,
                                                  "this HTTP response cannot have a chunked body"));
    if (response.headers.contains("Content-Length"))
        return ca::core::Err(HttpError::from_kind(
            HttpErrorKind::InvalidMessage, "chunked response cannot contain Content-Length"));

    HttpHeaders headers = response.headers;
    auto        chunked = parse_chunked_transfer_encoding(headers);
    if (chunked.is_err())
        return ca::core::Err(chunked.unwrap_err());
    if (!chunked.unwrap()) {
        auto appended = headers.append("Transfer-Encoding", "chunked");
        if (appended.is_err())
            return ca::core::Err(appended.unwrap_err());
    }

    const char* reason =
        response.reason.empty() ? default_reason_phrase(response.status) : response.reason.c_str();
    std::string start_line = http_version_name(response.version);
    start_line += ' ';
    start_line += std::to_string(response.status);
    if (*reason != '\0') {
        start_line += ' ';
        start_line += reason;
    }
    start_line += "\r\n";
    auto start = write_bytes(start_line);
    if (start.is_err())
        return ca::core::Err(start.unwrap_err());
    auto fields = write_headers(headers);
    if (fields.is_err())
        return ca::core::Err(fields.unwrap_err());
    auto end = write_bytes("\r\n");
    if (end.is_err())
        return ca::core::Err(end.unwrap_err());
    chunked_body_active_ = true;
    return ca::core::Ok(Http1ChunkedBodyWriter(*this));
}

}   // namespace ca::http
