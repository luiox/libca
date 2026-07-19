#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "libca/http/message.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"

namespace ca::http {

class Http1Writer;

/// @brief 逐块写入一个已经开始的 HTTP/1 chunked body。
/// @details 仅由 Http1Writer::begin_chunked_request/response 创建。析构不会隐式写 final
/// chunk；未调用 finish() 时连接上的报文不完整，所属 Http1Writer 也不能继续复用。
class Http1ChunkedBodyWriter
{
public:
    Http1ChunkedBodyWriter(const Http1ChunkedBodyWriter&)            = delete;
    Http1ChunkedBodyWriter& operator=(const Http1ChunkedBodyWriter&) = delete;
    Http1ChunkedBodyWriter(Http1ChunkedBodyWriter&& other) noexcept;
    Http1ChunkedBodyWriter& operator=(Http1ChunkedBodyWriter&&) = delete;
    ~Http1ChunkedBodyWriter()                                   = default;

    /// @brief 写入一个 data chunk；空 chunk 是 no-op，不表示 body 结束。
    HttpResult<void> write_chunk(const u8* data, usize length);

    /// @brief 写入一个 Bytes 的剩余内容作为 data chunk。
    HttpResult<void> write_chunk(const ca::core::Bytes& data);

    /// @brief 写入 string_view 的原始字节作为 data chunk。
    HttpResult<void> write_chunk(std::string_view data);

    /// @brief flush 底层 writer，供 SSE 等低延迟流式协议提交事件。
    HttpResult<void> flush();

    /// @brief 写入 final chunk 与 trailers，并释放所属 Http1Writer；不会隐式 flush。
    HttpResult<void> finish(const HttpHeaders& trailers = HttpHeaders());

    /// @brief 判断 final chunk 是否已成功写入。
    bool is_finished() const noexcept;

private:
    explicit Http1ChunkedBodyWriter(Http1Writer& owner) noexcept;

    Http1Writer* owner_{nullptr};
    bool         finished_{false};

    friend class Http1Writer;
};

/// @brief 保留预读字节的同步 HTTP/1 request/response reader。
/// @details 对底层 Reader 仅借用不接管，可连续读取 keep-alive 或 pipelined 报文。底层
/// Reader 必须在本对象生命周期内保持有效且地址不变；解析返回错误后不应继续复用该连接。
class Http1Reader
{
public:
    /// @brief 创建借用 reader 的 codec，并保存解析限制。
    explicit Http1Reader(io::Reader& reader, HttpLimits limits = HttpLimits()) noexcept;

    Http1Reader(const Http1Reader&)                = delete;
    Http1Reader& operator=(const Http1Reader&)     = delete;
    Http1Reader(Http1Reader&&) noexcept            = default;
    Http1Reader& operator=(Http1Reader&&) noexcept = default;

    /// @brief 读取 request head 并开始一个 incoming body；报文前干净 EOF 返回空 optional。
    /// @note 成功后必须读完 body 并调用 finish_body()，或直接调用 discard_body()，才能读取
    /// 下一条报文；即使 body kind 为 None 也必须 finish。
    HttpResult<std::optional<HttpRequestHead>> read_request_head();

    /// @brief 读取 response head 并开始一个 incoming body。
    /// @param request_method 对应 request method，用于 HEAD/CONNECT body 规则。
    HttpResult<std::optional<HttpResponseHead>> read_response_head(
        std::string_view request_method = {});

    /// @brief 返回当前 incoming body 的 framing；没有活动报文时返回 None。
    HttpBodyInfo body_info() const noexcept;

    /// @brief 读取一段解码 transfer coding 后的 body。
    /// @return 成功返回写入 buffer 的字节数；body 完成时返回 0。
    /// @note capacity 为 0 时不推进状态，调用 body_finished() 区分完成与空读。
    HttpResult<usize> read_body(u8* buffer, usize capacity);

    /// @brief 判断当前 incoming body 是否已经读到消息边界。
    bool body_finished() const noexcept;

    /// @brief 在 body 完成后取出 trailers，并允许读取下一条报文。
    HttpResult<HttpHeaders> finish_body();

    /// @brief 读取并丢弃剩余 body，返回 trailers，并允许读取下一条报文。
    HttpResult<HttpHeaders> discard_body();

    /// @brief 读取一个完整 request；报文前干净 EOF 返回空 optional。
    HttpResult<std::optional<HttpRequest>> read_request();

    /// @brief 读取一个完整 response；request_method 用于 HEAD/CONNECT body 规则。
    /// @param request_method 对应 request method；未知时传空字符串。
    HttpResult<std::optional<HttpResponse>> read_response(std::string_view request_method = {});

    /// @brief 返回已从底层预读但尚未消费的字节数。
    usize buffered_len() const noexcept;

private:
    enum class ReadState
    {
        Ready,
        ReadingHead,
        FixedBody,
        ChunkSize,
        ChunkData,
        ChunkDataEnding,
        CloseDelimitedBody,
        BodyComplete,
        Failed
    };

    HttpResult<void>                       validate_limits() const;
    HttpResult<void>                       ensure_ready();
    HttpResult<void>                       begin_body(HttpBodyInfo body, usize used_header_bytes,
                                                      usize used_header_count);
    HttpResult<bool>                       fill_buffer();
    HttpResult<std::optional<u8>>          read_byte();
    HttpResult<std::optional<std::string>> read_line(usize limit, bool allow_clean_eof);
    HttpResult<HttpHeaders>                read_headers(usize& used_bytes, usize& used_count);
    HttpResult<usize>                      read_fixed_body_part(u8* output, usize capacity);
    HttpResult<usize>                      read_chunked_body_part(u8* output, usize capacity);
    HttpResult<usize>           read_close_delimited_body_part(u8* output, usize capacity);
    HttpResult<ca::core::Bytes> read_body_all(HttpHeaders& trailers);

    io::Reader*          reader_{nullptr};
    HttpLimits           limits_;
    std::array<u8, 8192> buffer_{};
    usize                position_{0};
    usize                filled_{0};
    ReadState            state_{ReadState::Ready};
    HttpBodyInfo         body_info_{};
    usize                body_remaining_{0};
    usize                body_decoded_{0};
    usize                used_header_bytes_{0};
    usize                used_header_count_{0};
    HttpHeaders          body_trailers_;
};

/// @brief 把完整缓冲或 chunked streaming HTTP/1 request/response 写入同步 Writer。
class Http1Writer
{
public:
    /// @brief 创建借用 writer 的 codec。
    explicit Http1Writer(io::Writer& writer) noexcept;

    Http1Writer(const Http1Writer&)            = delete;
    Http1Writer& operator=(const Http1Writer&) = delete;
    Http1Writer(Http1Writer&&)                 = delete;
    Http1Writer& operator=(Http1Writer&&)      = delete;

    /// @brief 校验并写入 request；自动补必要的 Content-Length。
    HttpResult<void> write_request(const HttpRequest& request);

    /// @brief 校验并写入 response；request_method 用于 HEAD/CONNECT body 规则。
    HttpResult<void> write_response(const HttpResponse& response,
                                    std::string_view    request_method = {});

    /// @brief 写入 request head 并开始 chunked body。
    HttpResult<Http1ChunkedBodyWriter> begin_chunked_request(const HttpRequestHead& request);

    /// @brief 写入 response head 并开始 chunked body。
    /// @param request_method 对应 request method；HEAD/CONNECT 与无 body 状态会被拒绝。
    HttpResult<Http1ChunkedBodyWriter> begin_chunked_response(const HttpResponseHead& response,
                                                              std::string_view request_method = {});

private:
    HttpResult<void> ensure_idle() const;
    HttpResult<void> write_bytes(std::string_view value);
    HttpResult<void> write_headers(const HttpHeaders& headers);
    HttpResult<void> write_body(const ca::core::Bytes& body, const HttpHeaders& trailers,
                                bool chunked, bool send_body);

    io::Writer* writer_{nullptr};
    bool        chunked_body_active_{false};

    friend class Http1ChunkedBodyWriter;
};

}   // namespace ca::http
