#include <gtest/gtest.h>

#include <algorithm>
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

    usize remaining() const noexcept
    {
        return input_.size() - position_;
    }

private:
    std::string input_;
    usize       max_chunk_{3};
    usize       position_{0};
};

class StringWriter final : public io::Writer
{
public:
    io::IoResult<usize> write(const u8* data, usize length) override
    {
        output_.append(reinterpret_cast<const char*>(data), length);
        return ca::core::Ok(length);
    }

    io::IoResult<void> flush() override
    {
        return ca::core::Ok();
    }

    const std::string& output() const noexcept
    {
        return output_;
    }

private:
    std::string output_;
};

ca::core::Bytes bytes(std::string_view value)
{
    return ca::core::Bytes::copy_from_slice(reinterpret_cast<const u8*>(value.data()),
                                            value.size());
}

std::string text(const ca::core::Bytes& value)
{
    return std::string(reinterpret_cast<const char*>(value.as_ptr()), value.remaining());
}

HttpRequest read_request(std::string input, usize max_chunk = 3)
{
    FragmentedReader stream(std::move(input), max_chunk);
    Http1Reader      reader(stream);
    auto             result = reader.read_request();
    EXPECT_TRUE(result.is_ok()) << (result.is_err() ? result.unwrap_err().to_string() : "");
    if (result.is_err() || !result.unwrap().has_value())
        return HttpRequest();
    return std::move(result).unwrap().value();
}

TEST(Http1ReaderTest, ReadsFragmentedContentLengthAndPipelinedRequests)
{
    const std::string input =
        "POST /mcp?x=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5, 5\r\n"
        "X-Value: one\r\n"
        "x-value: two\r\n\r\n"
        "hello"
        "GET /next HTTP/1.1\r\nHost: localhost\r\n\r\n";
    FragmentedReader stream(input, 2);
    Http1Reader      reader(stream);

    auto first = reader.read_request();
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    ASSERT_TRUE(first.unwrap().has_value());
    auto first_request = std::move(first).unwrap().value();
    EXPECT_EQ(first_request.method, "POST");
    EXPECT_EQ(first_request.target, "/mcp?x=1");
    EXPECT_EQ(text(first_request.body), "hello");
    EXPECT_EQ(first_request.headers.get_all("X-Value").size(), 2U);

    auto second = reader.read_request();
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    ASSERT_TRUE(second.unwrap().has_value());
    EXPECT_EQ(second.unwrap()->method, "GET");
    EXPECT_EQ(second.unwrap()->target, "/next");
    EXPECT_TRUE(second.unwrap()->body.is_empty());

    auto eof = reader.read_request();
    ASSERT_TRUE(eof.is_ok());
    EXPECT_FALSE(eof.unwrap().has_value());
}

TEST(Http1ReaderTest, DecodesChunkExtensionsAndTrailers)
{
    const std::string input =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Trailer: X-Checksum\r\n\r\n"
        "4;part=1\r\nWiki\r\n"
        "5\r\npedia\r\n"
        "0\r\nX-Checksum: ok\r\n\r\n";
    auto request = read_request(input, 1);
    EXPECT_EQ(text(request.body), "Wikipedia");
    EXPECT_EQ(request.trailers.get("x-checksum"), "ok");
}

TEST(Http1ReaderTest, HandlesHeadNotModifiedAndCloseDelimitedResponses)
{
    FragmentedReader pipelined(
        "HTTP/1.1 304 Not Modified\r\nContent-Length: 7\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok",
        5);
    Http1Reader reader(pipelined);
    auto first = reader.read_response("GET");
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    ASSERT_TRUE(first.unwrap().has_value());
    EXPECT_EQ(first.unwrap()->status, 304);
    EXPECT_TRUE(first.unwrap()->body.is_empty());

    auto second = reader.read_response("GET");
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    ASSERT_TRUE(second.unwrap().has_value());
    EXPECT_EQ(text(second.unwrap()->body), "ok");

    FragmentedReader head_stream(
        "HTTP/1.1 200 OK\r\nContent-Length: 99\r\n\r\n",
        4);
    Http1Reader head_reader(head_stream);
    auto head = head_reader.read_response("HEAD");
    ASSERT_TRUE(head.is_ok());
    EXPECT_TRUE(head.unwrap()->body.is_empty());

    FragmentedReader close_stream("HTTP/1.0 200 OK\r\nX-Test: yes\r\n\r\nclose body", 3);
    Http1Reader close_reader(close_stream);
    auto close = close_reader.read_response("GET");
    ASSERT_TRUE(close.is_ok()) << close.unwrap_err().to_string();
    EXPECT_EQ(text(close.unwrap()->body), "close body");
}

TEST(Http1WriterTest, WritesAndReadsBackFixedRequest)
{
    HttpRequest request;
    request.method = "POST";
    request.target = "/mcp";
    ASSERT_TRUE(request.headers.append("Host", "localhost").is_ok());
    ASSERT_TRUE(request.headers.append("Content-Type", "application/json").is_ok());
    request.body = bytes("{\"ok\":true}");

    StringWriter writer_stream;
    Http1Writer  writer(writer_stream);
    ASSERT_TRUE(writer.write_request(request).is_ok());
    EXPECT_NE(writer_stream.output().find("Content-Length: 11\r\n"), std::string::npos);

    auto parsed = read_request(writer_stream.output(), 2);
    EXPECT_EQ(parsed.method, "POST");
    EXPECT_EQ(text(parsed.body), "{\"ok\":true}");
}

TEST(Http1WriterTest, WritesAndReadsBackChunkedResponseWithTrailers)
{
    HttpResponse response;
    response.status = 200;
    ASSERT_TRUE(response.headers.append("Transfer-Encoding", "chunked").is_ok());
    ASSERT_TRUE(response.trailers.append("X-Checksum", "ok").is_ok());
    response.body = bytes("stream-data");

    StringWriter writer_stream;
    Http1Writer  writer(writer_stream);
    ASSERT_TRUE(writer.write_response(response, "GET").is_ok());
    EXPECT_NE(writer_stream.output().find("b\r\nstream-data\r\n0\r\n"), std::string::npos);

    FragmentedReader reader_stream(writer_stream.output(), 2);
    Http1Reader      reader(reader_stream);
    auto parsed = reader.read_response("GET");
    ASSERT_TRUE(parsed.is_ok()) << parsed.unwrap_err().to_string();
    ASSERT_TRUE(parsed.unwrap().has_value());
    EXPECT_EQ(text(parsed.unwrap()->body), "stream-data");
    EXPECT_EQ(parsed.unwrap()->trailers.get("x-checksum"), "ok");
}

TEST(Http1WriterTest, OmitsHeadBodyAndRejectsLengthOrTrailerMismatches)
{
    HttpResponse head;
    head.body = bytes("representation");
    StringWriter head_stream;
    Http1Writer  head_writer(head_stream);
    ASSERT_TRUE(head_writer.write_response(head, "HEAD").is_ok());
    EXPECT_NE(head_stream.output().find("Content-Length: 14\r\n"), std::string::npos);
    EXPECT_EQ(head_stream.output().find("representation"), std::string::npos);

    HttpRequest mismatch;
    ASSERT_TRUE(mismatch.headers.append("Host", "localhost").is_ok());
    ASSERT_TRUE(mismatch.headers.append("Content-Length", "9").is_ok());
    mismatch.body = bytes("short");
    StringWriter mismatch_stream;
    Http1Writer  mismatch_writer(mismatch_stream);
    EXPECT_EQ(mismatch_writer.write_request(mismatch).unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    HttpRequest trailers;
    ASSERT_TRUE(trailers.headers.append("Host", "localhost").is_ok());
    ASSERT_TRUE(trailers.trailers.append("X-End", "yes").is_ok());
    StringWriter trailers_stream;
    Http1Writer  trailers_writer(trailers_stream);
    EXPECT_EQ(trailers_writer.write_request(trailers).unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);
}

TEST(Http1WriterTest, RejectsFramingAndTrailersOnResponsesWithoutBodies)
{
    HttpResponse no_content;
    no_content.status = 204;
    ASSERT_TRUE(no_content.headers.append("Content-Length", "0").is_ok());
    StringWriter no_content_stream;
    Http1Writer  no_content_writer(no_content_stream);
    EXPECT_EQ(no_content_writer.write_response(no_content, "GET").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    HttpResponse tunnel;
    ASSERT_TRUE(tunnel.headers.append("Transfer-Encoding", "chunked").is_ok());
    StringWriter tunnel_stream;
    Http1Writer  tunnel_writer(tunnel_stream);
    EXPECT_EQ(tunnel_writer.write_response(tunnel, "CONNECT").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    HttpResponse head;
    ASSERT_TRUE(head.headers.append("Transfer-Encoding", "chunked").is_ok());
    ASSERT_TRUE(head.trailers.append("X-End", "yes").is_ok());
    StringWriter head_stream;
    Http1Writer  head_writer(head_stream);
    EXPECT_EQ(head_writer.write_response(head, "HEAD").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);
}

class Http1InvalidMessageTest : public ::testing::TestWithParam<std::string>
{};

TEST_P(Http1InvalidMessageTest, RejectsAmbiguousOrUnsafeRequest)
{
    FragmentedReader stream(GetParam(), 2);
    Http1Reader      reader(stream);
    auto             result = reader.read_request();
    ASSERT_TRUE(result.is_err());
    EXPECT_TRUE(result.unwrap_err().kind() == HttpErrorKind::InvalidMessage ||
                result.unwrap_err().kind() == HttpErrorKind::Unsupported);
}

INSTANTIATE_TEST_SUITE_P(
    SmugglingAndLineSyntax,
    Http1InvalidMessageTest,
    ::testing::Values(
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n",
        "GET / HTTP/1.1\nHost: x\n\n",
        "GET / HTTP/1.1\r\nHost: x\r\n folded\r\n\r\n",
        "GET / HTTP/1.1\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\nHost: y\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x, y\r\n\r\n",
        "POST / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip, chunked\r\n\r\n"));

TEST(Http1ReaderTest, EnforcesStartHeaderAndBodyLimits)
{
    HttpLimits limits;
    limits.max_start_line_bytes = 8;
    FragmentedReader long_start("GET /toolong HTTP/1.1\r\nHost: x\r\n\r\n", 2);
    Http1Reader      start_reader(long_start, limits);
    EXPECT_EQ(start_reader.read_request().unwrap_err().kind(),
              HttpErrorKind::HeaderLimitExceeded);

    limits = HttpLimits();
    limits.max_header_count = 1;
    FragmentedReader many_headers("GET / HTTP/1.1\r\nHost: x\r\nX-Test: y\r\n\r\n", 2);
    Http1Reader      header_reader(many_headers, limits);
    EXPECT_EQ(header_reader.read_request().unwrap_err().kind(),
              HttpErrorKind::HeaderLimitExceeded);

    limits = HttpLimits();
    limits.max_body_bytes = 4;
    FragmentedReader large_body(
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nhello",
        2);
    Http1Reader body_reader(large_body, limits);
    EXPECT_EQ(body_reader.read_request().unwrap_err().kind(), HttpErrorKind::BodyLimitExceeded);
}

TEST(Http1ReaderTest, RejectsTruncatedFixedAndChunkedBodies)
{
    FragmentedReader fixed(
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nabc",
        2);
    Http1Reader fixed_reader(fixed);
    EXPECT_EQ(fixed_reader.read_request().unwrap_err().kind(), HttpErrorKind::InvalidMessage);

    FragmentedReader chunked(
        "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabcX",
        2);
    Http1Reader chunked_reader(chunked);
    EXPECT_EQ(chunked_reader.read_request().unwrap_err().kind(), HttpErrorKind::InvalidMessage);
}

TEST(Http1ReaderTest, EnforcesResponseFramingRules)
{
    FragmentedReader no_content(
        "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n",
        2);
    Http1Reader no_content_reader(no_content);
    EXPECT_EQ(no_content_reader.read_response("GET").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    FragmentedReader http10_chunked(
        "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        2);
    Http1Reader http10_reader(http10_chunked);
    EXPECT_EQ(http10_reader.read_response("GET").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);

    FragmentedReader connect(
        "HTTP/1.1 200 Connection Established\r\n"
        "Content-Length: 7\r\nTransfer-Encoding: gzip\r\n\r\ntunnel",
        2);
    Http1Reader connect_reader(connect);
    auto        connected = connect_reader.read_response("CONNECT");
    ASSERT_TRUE(connected.is_ok()) << connected.unwrap_err().to_string();
    ASSERT_TRUE(connected.unwrap().has_value());
    EXPECT_TRUE(connected.unwrap()->body.is_empty());
    EXPECT_EQ(connect_reader.buffered_len() + connect.remaining(), 6U);
}

}   // namespace
}   // namespace ca::http::test
