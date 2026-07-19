#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "libca/http/message.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"

namespace ca::http {

/// @brief 保留预读字节的同步 HTTP/1 request/response reader。
/// @details 对底层 Reader 仅借用不接管，可连续读取 keep-alive 或 pipelined 报文。底层
/// Reader 必须在本对象生命周期内保持有效且地址不变；解析返回错误后不应继续复用该连接。
class Http1Reader
{
public:
    /// @brief 创建借用 reader 的 codec，并保存解析限制。
    explicit Http1Reader(io::Reader& reader, HttpLimits limits = HttpLimits()) noexcept;

    Http1Reader(const Http1Reader&)            = delete;
    Http1Reader& operator=(const Http1Reader&) = delete;
    Http1Reader(Http1Reader&&) noexcept        = default;
    Http1Reader& operator=(Http1Reader&&) noexcept = default;

    /// @brief 读取一个完整 request；报文前干净 EOF 返回空 optional。
    HttpResult<std::optional<HttpRequest>> read_request();

    /// @brief 读取一个完整 response；request_method 用于 HEAD/CONNECT body 规则。
    /// @param request_method 对应 request method；未知时传空字符串。
    HttpResult<std::optional<HttpResponse>> read_response(std::string_view request_method = {});

    /// @brief 返回已从底层预读但尚未消费的字节数。
    usize buffered_len() const noexcept;

private:
    HttpResult<void> validate_limits() const;
    HttpResult<bool> fill_buffer();
    HttpResult<std::optional<u8>> read_byte();
    HttpResult<std::optional<std::string>> read_line(usize limit, bool allow_clean_eof);
    HttpResult<HttpHeaders> read_headers(usize& used_bytes, usize& used_count);
    HttpResult<ca::core::Bytes> read_body(const HttpHeaders& headers, HttpHeaders& trailers,
                                          bool body_forbidden, bool close_delimited,
                                          usize used_header_bytes, usize used_header_count);
    HttpResult<void> read_exact_into(ca::core::BytesMut& output, usize length);
    HttpResult<ca::core::Bytes> read_fixed_body(usize length);
    HttpResult<ca::core::Bytes> read_chunked_body(HttpHeaders& trailers,
                                                  usize used_header_bytes,
                                                  usize used_header_count);
    HttpResult<ca::core::Bytes> read_close_delimited_body();

    io::Reader*          reader_{nullptr};
    HttpLimits           limits_;
    std::array<u8, 8192> buffer_{};
    usize                position_{0};
    usize                filled_{0};
};

/// @brief 把完整缓冲的 HTTP/1 request/response 写入同步 Writer。
class Http1Writer
{
public:
    /// @brief 创建借用 writer 的 codec。
    explicit Http1Writer(io::Writer& writer) noexcept;

    /// @brief 校验并写入 request；自动补必要的 Content-Length。
    HttpResult<void> write_request(const HttpRequest& request);

    /// @brief 校验并写入 response；request_method 用于 HEAD/CONNECT body 规则。
    HttpResult<void> write_response(const HttpResponse& response,
                                    std::string_view request_method = {});

private:
    HttpResult<void> write_bytes(std::string_view value);
    HttpResult<void> write_headers(const HttpHeaders& headers);
    HttpResult<void> write_body(const ca::core::Bytes& body, const HttpHeaders& trailers,
                                bool chunked, bool send_body);

    io::Writer* writer_{nullptr};
};

}   // namespace ca::http
