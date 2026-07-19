#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <utility>

#include "libca/http/http1_codec.hpp"

namespace ca::http::test {
namespace {

class FragmentedReader final : public io::Reader
{
public:
    explicit FragmentedReader(std::string input, usize max_chunk = 3)
        : input_(std::move(input))
        , max_chunk_(max_chunk)
    {}

    io::IoResult<usize> read(u8* buffer, usize capacity) override
    {
        if (position_ == input_.size())
            return ca::core::Ok(static_cast<usize>(0));
        const usize count = std::min({capacity, max_chunk_, input_.size() - position_});
        std::memcpy(buffer, input_.data() + position_, count);
        position_ += count;
        return ca::core::Ok(count);
    }

private:
    std::string input_;
    usize       max_chunk_{3};
    usize       position_{0};
};

class RecordingWriter final : public io::Writer
{
public:
    io::IoResult<usize> write(const u8* data, usize length) override
    {
        output_.append(reinterpret_cast<const char*>(data), length);
        return ca::core::Ok(length);
    }

    io::IoResult<void> flush() override
    {
        ++flush_count_;
        return ca::core::Ok();
    }

    const std::string& output() const noexcept { return output_; }

    usize flush_count() const noexcept { return flush_count_; }

private:
    std::string output_;
    usize       flush_count_{0};
};

std::string read_streamed_body(Http1Reader& reader, usize chunk_size)
{
    std::string        output;
    std::array<u8, 16> buffer{};
    while (!reader.body_finished()) {
        auto read = reader.read_body(buffer.data(), std::min(chunk_size, buffer.size()));
        EXPECT_TRUE(read.is_ok()) << (read.is_err() ? read.unwrap_err().to_string() : "");
        if (read.is_err())
            break;
        output.append(reinterpret_cast<const char*>(buffer.data()), read.unwrap());
    }
    return output;
}

TEST(Http1StreamingReaderTest, StreamsFixedBodyAndRequiresExplicitFinish)
{
    FragmentedReader stream(
        "POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello"
        "GET /next HTTP/1.1\r\nHost: localhost\r\n\r\n",
        2);
    Http1Reader reader(stream);

    auto first = reader.read_request_head();
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    ASSERT_TRUE(first.unwrap().has_value());
    EXPECT_EQ(first.unwrap()->method, "POST");
    EXPECT_EQ(reader.body_info().kind, HttpBodyKind::ContentLength);
    EXPECT_EQ(reader.body_info().content_length, 5U);
    EXPECT_EQ(reader.read_request_head().unwrap_err().kind(), HttpErrorKind::InvalidState);

    std::array<u8, 2> zero_buffer{};
    EXPECT_EQ(reader.read_body(zero_buffer.data(), 0).unwrap(), 0U);
    EXPECT_FALSE(reader.body_finished());
    EXPECT_EQ(read_streamed_body(reader, 2), "hello");
    EXPECT_TRUE(reader.body_finished());
    auto trailers = reader.finish_body();
    ASSERT_TRUE(trailers.is_ok());
    EXPECT_TRUE(trailers.unwrap().is_empty());

    auto second = reader.read_request_head();
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    ASSERT_TRUE(second.unwrap().has_value());
    EXPECT_EQ(second.unwrap()->target, "/next");
    EXPECT_EQ(reader.body_info().kind, HttpBodyKind::None);
    EXPECT_TRUE(reader.body_finished());
    ASSERT_TRUE(reader.finish_body().is_ok());

    auto eof = reader.read_request_head();
    ASSERT_TRUE(eof.is_ok());
    EXPECT_FALSE(eof.unwrap().has_value());
}

TEST(Http1StreamingReaderTest, ReadsFixedBodyLargerThanInitialCapacity)
{
    const std::string body(12 * 1024, 'x');
    FragmentedReader  stream("POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
                                std::to_string(body.size()) + "\r\n\r\n" + body,
                            257);
    Http1Reader       reader(stream);

    auto request = reader.read_request();
    ASSERT_TRUE(request.is_ok()) << request.unwrap_err().to_string();
    ASSERT_TRUE(request.unwrap().has_value());
    EXPECT_EQ(request.unwrap()->body.remaining(), body.size());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(request.unwrap()->body.as_ptr()),
                          request.unwrap()->body.remaining()),
              body);
}

TEST(Http1StreamingReaderTest, StreamsChunkedBodyAndReturnsTrailers)
{
    FragmentedReader stream(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nTrailer: X-End\r\n\r\n"
        "4;part=1\r\nWiki\r\n5\r\npedia\r\n0\r\nX-End: yes\r\n\r\n",
        1);
    Http1Reader reader(stream);

    auto head = reader.read_response_head("GET");
    ASSERT_TRUE(head.is_ok()) << head.unwrap_err().to_string();
    ASSERT_TRUE(head.unwrap().has_value());
    EXPECT_EQ(reader.body_info().kind, HttpBodyKind::Chunked);
    EXPECT_EQ(read_streamed_body(reader, 3), "Wikipedia");

    auto trailers = reader.finish_body();
    ASSERT_TRUE(trailers.is_ok()) << trailers.unwrap_err().to_string();
    EXPECT_EQ(trailers.unwrap().get("X-End"), "yes");
}

TEST(Http1StreamingReaderTest, DiscardsBodyBeforeReadingNextMessage)
{
    FragmentedReader stream(
        "POST /discard HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
        "3\r\nold\r\n0\r\nX-End: yes\r\n\r\n"
        "GET /kept HTTP/1.1\r\nHost: localhost\r\n\r\n",
        2);
    Http1Reader reader(stream);

    auto first = reader.read_request_head();
    ASSERT_TRUE(first.is_ok());
    ASSERT_TRUE(first.unwrap().has_value());
    auto trailers = reader.discard_body();
    ASSERT_TRUE(trailers.is_ok()) << trailers.unwrap_err().to_string();
    EXPECT_EQ(trailers.unwrap().get("X-End"), "yes");

    auto second = reader.read_request_head();
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    ASSERT_TRUE(second.unwrap().has_value());
    EXPECT_EQ(second.unwrap()->target, "/kept");
    ASSERT_TRUE(reader.finish_body().is_ok());
}

TEST(Http1StreamingReaderTest, StreamsCloseDelimitedBodyUntilEof)
{
    FragmentedReader stream("HTTP/1.0 200 OK\r\n\r\nclose-delimited", 2);
    Http1Reader      reader(stream);

    auto head = reader.read_response_head("GET");
    ASSERT_TRUE(head.is_ok());
    ASSERT_TRUE(head.unwrap().has_value());
    EXPECT_EQ(reader.body_info().kind, HttpBodyKind::CloseDelimited);
    EXPECT_EQ(read_streamed_body(reader, 4), "close-delimited");
    ASSERT_TRUE(reader.finish_body().is_ok());
}

TEST(Http1StreamingReaderTest, FailsChunkedStreamOnceDecodedLimitIsExceeded)
{
    HttpLimits limits;
    limits.max_body_bytes = 4;
    FragmentedReader stream(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n", 2);
    Http1Reader reader(stream, limits);
    ASSERT_TRUE(reader.read_response_head("GET").is_ok());

    std::array<u8, 8> buffer{};
    auto              read = reader.read_body(buffer.data(), buffer.size());
    ASSERT_TRUE(read.is_err());
    EXPECT_EQ(read.unwrap_err().kind(), HttpErrorKind::BodyLimitExceeded);
    EXPECT_EQ(reader.read_body(buffer.data(), buffer.size()).unwrap_err().kind(),
              HttpErrorKind::InvalidState);
}

TEST(Http1StreamingWriterTest, WritesSseChunksFlushesAndFinishesWithTrailers)
{
    RecordingWriter  stream;
    Http1Writer      writer(stream);
    HttpResponseHead head;
    ASSERT_TRUE(head.headers.append("Content-Type", "text/event-stream").is_ok());
    ASSERT_TRUE(head.headers.append("Trailer", "X-End").is_ok());

    auto begun = writer.begin_chunked_response(head, "GET");
    ASSERT_TRUE(begun.is_ok()) << begun.unwrap_err().to_string();
    auto body = std::move(begun).unwrap();
    EXPECT_FALSE(body.is_finished());

    HttpResponse interleaved;
    EXPECT_EQ(writer.write_response(interleaved, "GET").unwrap_err().kind(),
              HttpErrorKind::InvalidState);
    ASSERT_TRUE(body.write_chunk(nullptr, 0).is_ok());
    const std::string connected = ": connected\n\n";
    ASSERT_TRUE(body.write_chunk(connected).is_ok());
    ASSERT_TRUE(body.flush().is_ok());
    const std::string event = "event: message\ndata: {}\n\n";
    ASSERT_TRUE(body.write_chunk(event).is_ok());
    HttpHeaders trailers;
    ASSERT_TRUE(trailers.append("X-End", "yes").is_ok());
    ASSERT_TRUE(body.finish(trailers).is_ok());
    EXPECT_TRUE(body.is_finished());
    EXPECT_EQ(stream.flush_count(), 1U);

    FragmentedReader encoded(stream.output(), 2);
    Http1Reader      reader(encoded);
    auto             parsed = reader.read_response("GET");
    ASSERT_TRUE(parsed.is_ok()) << parsed.unwrap_err().to_string();
    ASSERT_TRUE(parsed.unwrap().has_value());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(parsed.unwrap()->body.as_ptr()),
                          parsed.unwrap()->body.remaining()),
              connected + event);
    EXPECT_EQ(parsed.unwrap()->trailers.get("X-End"), "yes");
}

TEST(Http1StreamingWriterTest, WritesChunkedRequestAndCanBeReusedAfterFinish)
{
    RecordingWriter stream;
    Http1Writer     writer(stream);
    HttpRequestHead head;
    head.method = "POST";
    head.target = "/upload";
    ASSERT_TRUE(head.headers.append("Host", "localhost").is_ok());

    auto begun = writer.begin_chunked_request(head);
    ASSERT_TRUE(begun.is_ok()) << begun.unwrap_err().to_string();
    auto              body   = std::move(begun).unwrap();
    const std::string first  = "abc";
    const std::string second = "def";
    ASSERT_TRUE(body.write_chunk(first).is_ok());
    ASSERT_TRUE(body.write_chunk(second).is_ok());

    HttpHeaders invalid_trailers;
    ASSERT_TRUE(invalid_trailers.append("Content-Length", "6").is_ok());
    EXPECT_EQ(body.finish(invalid_trailers).unwrap_err().kind(), HttpErrorKind::InvalidMessage);
    ASSERT_TRUE(body.finish().is_ok());

    HttpResponse response;
    ASSERT_TRUE(writer.write_response(response, "GET").is_ok());

    FragmentedReader encoded(stream.output(), 3);
    Http1Reader      reader(encoded);
    auto             parsed = reader.read_request();
    ASSERT_TRUE(parsed.is_ok()) << parsed.unwrap_err().to_string();
    ASSERT_TRUE(parsed.unwrap().has_value());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(parsed.unwrap()->body.as_ptr()),
                          parsed.unwrap()->body.remaining()),
              "abcdef");
}

TEST(Http1StreamingWriterTest, RejectsInvalidChunkedMessageHeads)
{
    RecordingWriter stream;
    Http1Writer     writer(stream);

    HttpRequestHead request;
    ASSERT_TRUE(request.headers.append("Host", "localhost").is_ok());
    ASSERT_TRUE(request.headers.append("Content-Length", "1").is_ok());
    EXPECT_EQ(writer.begin_chunked_request(request).unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    HttpResponseHead no_content;
    no_content.status = 204;
    EXPECT_EQ(writer.begin_chunked_response(no_content, "GET").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    HttpResponseHead http10;
    http10.version = HttpVersion::Http10;
    EXPECT_EQ(writer.begin_chunked_response(http10, "GET").unwrap_err().kind(),
              HttpErrorKind::Unsupported);
}

TEST(Http1StreamingWriterTest, AbandonedBodyDoesNotFinishOrReleaseWriter)
{
    RecordingWriter  stream;
    Http1Writer      writer(stream);
    HttpResponseHead head;

    {
        auto begun = writer.begin_chunked_response(head, "GET");
        ASSERT_TRUE(begun.is_ok()) << begun.unwrap_err().to_string();
        auto body = std::move(begun).unwrap();
        ASSERT_TRUE(body.write_chunk("partial").is_ok());
    }

    EXPECT_EQ(stream.output(),
              "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n7\r\npartial\r\n");
    HttpResponse response;
    EXPECT_EQ(writer.write_response(response, "GET").unwrap_err().kind(),
              HttpErrorKind::InvalidState);
}

}   // namespace
}   // namespace ca::http::test
