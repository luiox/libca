#include <gtest/gtest.h>

#include <string>

#include "libca/http/headers.hpp"
#include "libca/http/message.hpp"
#include "libca/http/url.hpp"

namespace ca::http::test {
namespace {

TEST(HttpHeadersTest, PreservesOrderDuplicatesAndCaseInsensitiveLookup)
{
    HttpHeaders headers;
    ASSERT_TRUE(headers.append("Set-Cookie", "a=1").is_ok());
    ASSERT_TRUE(headers.append("set-cookie", "b=2").is_ok());
    ASSERT_TRUE(headers.append("Content-Type", "application/json").is_ok());

    EXPECT_EQ(headers.size(), 3U);
    EXPECT_EQ(headers.get("CONTENT-TYPE"), "application/json");
    const auto cookies = headers.get_all("SET-cookie");
    ASSERT_EQ(cookies.size(), 2U);
    EXPECT_EQ(cookies[0], "a=1");
    EXPECT_EQ(cookies[1], "b=2");
    EXPECT_EQ(headers.entries()[0].name, "Set-Cookie");

    ASSERT_TRUE(headers.set("set-cookie", "c=3").is_ok());
    EXPECT_EQ(headers.get_all("Set-Cookie").size(), 1U);
    EXPECT_EQ(headers.get("Set-Cookie"), "c=3");
    EXPECT_EQ(headers.remove("content-type"), 1U);
    EXPECT_FALSE(headers.contains("Content-Type"));
}

TEST(HttpHeadersTest, RejectsInvalidNamesAndInjectionValues)
{
    HttpHeaders headers;
    EXPECT_EQ(headers.append("Bad Name", "value").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);
    EXPECT_EQ(headers.append("X-Test", "one\r\ntwo").unwrap_err().kind(),
              HttpErrorKind::InvalidMessage);
    EXPECT_FALSE(HttpHeaders::valid_name(""));
    EXPECT_TRUE(HttpHeaders::valid_name("MCP-Session-Id"));
    EXPECT_TRUE(HttpHeaders::valid_value("text\tvalue"));
    EXPECT_FALSE(HttpHeaders::valid_value(std::string("bad\0value", 9)));
}

TEST(HttpHeadersTest, ComputesHttp10AndHttp11Persistence)
{
    HttpHeaders headers;
    EXPECT_TRUE(should_keep_alive(HttpVersion::Http11, headers));
    EXPECT_FALSE(should_keep_alive(HttpVersion::Http10, headers));

    ASSERT_TRUE(headers.append("Connection", "upgrade, Close").is_ok());
    EXPECT_FALSE(should_keep_alive(HttpVersion::Http11, headers));
    headers.remove("Connection");
    ASSERT_TRUE(headers.append("connection", "keep-alive").is_ok());
    EXPECT_TRUE(should_keep_alive(HttpVersion::Http10, headers));
}

TEST(HttpUrlTest, ParsesHttpHttpsQueryAndFragment)
{
    auto plain = HttpUrl::parse("http://example.com");
    ASSERT_TRUE(plain.is_ok()) << plain.unwrap_err().to_string();
    EXPECT_EQ(plain.unwrap().scheme(), HttpScheme::Http);
    EXPECT_EQ(plain.unwrap().host(), "example.com");
    EXPECT_EQ(plain.unwrap().port(), 80);
    EXPECT_FALSE(plain.unwrap().has_explicit_port());
    EXPECT_EQ(plain.unwrap().target(), "/");
    EXPECT_EQ(plain.unwrap().authority(), "example.com");

    auto secure = HttpUrl::parse("HTTPS://api.example.com:8443/mcp?q=1#ignored");
    ASSERT_TRUE(secure.is_ok()) << secure.unwrap_err().to_string();
    EXPECT_EQ(secure.unwrap().scheme(), HttpScheme::Https);
    EXPECT_EQ(secure.unwrap().port(), 8443);
    EXPECT_TRUE(secure.unwrap().has_explicit_port());
    EXPECT_EQ(secure.unwrap().target(), "/mcp?q=1");
    EXPECT_EQ(secure.unwrap().authority(), "api.example.com:8443");

    auto query_only = HttpUrl::parse("http://localhost?mode=test");
    ASSERT_TRUE(query_only.is_ok());
    EXPECT_EQ(query_only.unwrap().target(), "/?mode=test");
}

TEST(HttpUrlTest, ParsesBracketedIpv6AndOmitsDefaultPortFromAuthority)
{
    auto url = HttpUrl::parse("https://[::1]:443/mcp");
    ASSERT_TRUE(url.is_ok()) << url.unwrap_err().to_string();
    EXPECT_EQ(url.unwrap().host(), "::1");
    EXPECT_EQ(url.unwrap().port(), 443);
    EXPECT_TRUE(url.unwrap().has_explicit_port());
    EXPECT_EQ(url.unwrap().authority(), "[::1]");
}

TEST(HttpUrlTest, RejectsAmbiguousOrUnsupportedAuthorities)
{
    EXPECT_EQ(HttpUrl::parse("ftp://example.com/").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://user@example.com/").unwrap_err().kind(),
              HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://::1/").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://example.com:0/").unwrap_err().kind(),
              HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://example.com:65536/").unwrap_err().kind(),
              HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://example.com/bad path").unwrap_err().kind(),
              HttpErrorKind::InvalidUrl);
}

}   // namespace
}   // namespace ca::http::test
