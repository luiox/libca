#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "libca/http/url.hpp"

namespace ca::http::test {
namespace {

// round-trip：parse(to_string(url)) 与原实例的组件分解完全一致。
void expect_round_trip(const HttpUrl& url)
{
    auto reparsed = HttpUrl::parse(url.to_string());
    ASSERT_TRUE(reparsed.is_ok()) << reparsed.unwrap_err().to_string();
    const auto& other = reparsed.unwrap();
    EXPECT_EQ(other.scheme(), url.scheme());
    EXPECT_EQ(other.has_userinfo(), url.has_userinfo());
    EXPECT_EQ(other.userinfo(), url.userinfo());
    EXPECT_EQ(other.host(), url.host());
    EXPECT_EQ(other.port(), url.port());
    EXPECT_EQ(other.has_explicit_port(), url.has_explicit_port());
    EXPECT_EQ(other.path(), url.path());
    EXPECT_EQ(other.has_query(), url.has_query());
    EXPECT_EQ(other.query(), url.query());
    EXPECT_EQ(other.has_fragment(), url.has_fragment());
    EXPECT_EQ(other.fragment(), url.fragment());
    EXPECT_EQ(other.target(), url.target());
    EXPECT_EQ(other.to_string(), url.to_string());
}

TEST(HttpUrlRfc3986Test, SplitsUserinfoHostPortPathQueryFragment)
{
    auto url = HttpUrl::parse("http://user:pass@example.com:8080/a/b?k=v&k2=v2#frag");
    ASSERT_TRUE(url.is_ok()) << url.unwrap_err().to_string();
    EXPECT_EQ(url.unwrap().scheme(), HttpScheme::Http);
    EXPECT_TRUE(url.unwrap().has_userinfo());
    EXPECT_EQ(url.unwrap().userinfo(), "user:pass");
    EXPECT_EQ(url.unwrap().host(), "example.com");
    EXPECT_EQ(url.unwrap().port(), 8080);
    EXPECT_TRUE(url.unwrap().has_explicit_port());
    EXPECT_EQ(url.unwrap().path(), "/a/b");
    EXPECT_TRUE(url.unwrap().has_query());
    EXPECT_EQ(url.unwrap().query(), "k=v&k2=v2");
    EXPECT_TRUE(url.unwrap().has_fragment());
    EXPECT_EQ(url.unwrap().fragment(), "frag");
    EXPECT_EQ(url.unwrap().target(), "/a/b?k=v&k2=v2");
    // Host header authority 不携带 userinfo。
    EXPECT_EQ(url.unwrap().authority(), "example.com:8080");
    expect_round_trip(url.unwrap());

    // 空userinfo：'@' 出现即视为携带。
    auto empty_userinfo = HttpUrl::parse("https://@example.com/x");
    ASSERT_TRUE(empty_userinfo.is_ok());
    EXPECT_TRUE(empty_userinfo.unwrap().has_userinfo());
    EXPECT_EQ(empty_userinfo.unwrap().userinfo(), "");
    EXPECT_EQ(empty_userinfo.unwrap().host(), "example.com");

    // userinfo 内含 '@' 时按最后一个 '@' 切分。
    auto at_in_userinfo = HttpUrl::parse("http://a@b@example.com/");
    ASSERT_TRUE(at_in_userinfo.is_ok()) << at_in_userinfo.unwrap_err().to_string();
    EXPECT_EQ(at_in_userinfo.unwrap().userinfo(), "a@b");
    EXPECT_EQ(at_in_userinfo.unwrap().host(), "example.com");

    // userinfo 里的 sub-delims 与 ':' 合法；保留原始 percent-encoding。
    auto encoded = HttpUrl::parse("http://u%20s%3Ap@example.com/");
    ASSERT_TRUE(encoded.is_ok()) << encoded.unwrap_err().to_string();
    EXPECT_EQ(encoded.unwrap().userinfo(), "u%20s%3Ap");

    // 非法 userinfo 字符仍拒绝。
    EXPECT_EQ(HttpUrl::parse("http://us er@example.com/").unwrap_err().kind(),
              HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://u%2@example.com/").unwrap_err().kind(),
              HttpErrorKind::InvalidUrl);
    EXPECT_EQ(HttpUrl::parse("http://user@/").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
}

TEST(HttpUrlRfc3986Test, ParsesIpv6AndDefaultPorts)
{
    auto url = HttpUrl::parse("http://[2001:db8::1]:8080/x?y=1#z");
    ASSERT_TRUE(url.is_ok()) << url.unwrap_err().to_string();
    EXPECT_EQ(url.unwrap().host(), "2001:db8::1");
    EXPECT_EQ(url.unwrap().port(), 8080);
    EXPECT_TRUE(url.unwrap().has_explicit_port());
    EXPECT_EQ(url.unwrap().authority(), "[2001:db8::1]:8080");
    EXPECT_EQ(url.unwrap().target(), "/x?y=1");
    EXPECT_EQ(url.unwrap().fragment(), "z");
    expect_round_trip(url.unwrap());

    auto default_http = HttpUrl::parse("http://example.com:80/x");
    ASSERT_TRUE(default_http.is_ok());
    EXPECT_EQ(default_http.unwrap().port(), 80);
    EXPECT_TRUE(default_http.unwrap().has_explicit_port());
    // 显式默认端口在 Host authority 中省略。
    EXPECT_EQ(default_http.unwrap().authority(), "example.com");

    auto default_https = HttpUrl::parse("https://[::1]/");
    ASSERT_TRUE(default_https.is_ok());
    EXPECT_EQ(default_https.unwrap().port(), 443);
    EXPECT_FALSE(default_https.unwrap().has_explicit_port());
    EXPECT_EQ(default_https.unwrap().authority(), "[::1]");
    EXPECT_EQ(default_https.unwrap().path(), "/");
    expect_round_trip(default_https.unwrap());

    auto no_port = HttpUrl::parse("https://example.com/mcp");
    ASSERT_TRUE(no_port.is_ok());
    EXPECT_EQ(no_port.unwrap().port(), 443);
    EXPECT_FALSE(no_port.unwrap().has_explicit_port());
}

TEST(HttpUrlRfc3986Test, SplitsQueryAndFragmentEdgeShapes)
{
    auto empty_query = HttpUrl::parse("http://example.com/a?");
    ASSERT_TRUE(empty_query.is_ok());
    EXPECT_TRUE(empty_query.unwrap().has_query());
    EXPECT_EQ(empty_query.unwrap().query(), "");
    EXPECT_EQ(empty_query.unwrap().target(), "/a?");
    expect_round_trip(empty_query.unwrap());

    auto query_only = HttpUrl::parse("http://example.com?mode=test");
    ASSERT_TRUE(query_only.is_ok());
    EXPECT_EQ(query_only.unwrap().path(), "/");
    EXPECT_EQ(query_only.unwrap().query(), "mode=test");
    EXPECT_EQ(query_only.unwrap().target(), "/?mode=test");

    // '#' 之后全部属于 fragment，包括其中的 '?'。
    auto hash_question = HttpUrl::parse("http://example.com/a?q#f?g");
    ASSERT_TRUE(hash_question.is_ok());
    EXPECT_EQ(hash_question.unwrap().query(), "q");
    EXPECT_EQ(hash_question.unwrap().fragment(), "f?g");

    // query 内允许裸 '?'。
    auto question_in_query = HttpUrl::parse("http://example.com/a?b?c");
    ASSERT_TRUE(question_in_query.is_ok());
    EXPECT_EQ(question_in_query.unwrap().query(), "b?c");

    auto empty_fragment = HttpUrl::parse("http://example.com/a#");
    ASSERT_TRUE(empty_fragment.is_ok());
    EXPECT_TRUE(empty_fragment.unwrap().has_fragment());
    EXPECT_EQ(empty_fragment.unwrap().fragment(), "");
    EXPECT_EQ(empty_fragment.unwrap().target(), "/a");
    expect_round_trip(empty_fragment.unwrap());

    // 只拒绝非法 percent 序列；path 中允许任意可见 ASCII。
    auto bad_percent = HttpUrl::parse("http://example.com/a%zz");
    ASSERT_TRUE(bad_percent.is_err());
    EXPECT_EQ(bad_percent.unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    auto truncated_percent = HttpUrl::parse("http://example.com/%2");
    ASSERT_TRUE(truncated_percent.is_err());
    EXPECT_EQ(truncated_percent.unwrap_err().kind(), HttpErrorKind::InvalidUrl);
}

TEST(HttpUrlRfc3986Test, PercentEncodeKeepsUnreservedAndUppercaseHex)
{
    EXPECT_EQ(percent_encode("a b/c~"), "a%20b%2Fc~");
    EXPECT_EQ(percent_encode("AZaz09-._~"), "AZaz09-._~");
    EXPECT_EQ(percent_encode(""), "");
    EXPECT_EQ(percent_encode("\r\n"), "%0D%0A");
    // UTF-8 多字节按字节逐个编码。
    EXPECT_EQ(percent_encode("ü"), "%C3%BC");
    // round-trip：decode(encode(x)) == x。
    const std::string raw = "path with spaces/&=?#ü\x01";
    auto decoded = percent_decode(percent_encode(raw));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(std::move(decoded).unwrap(), raw);
}

TEST(HttpUrlRfc3986Test, PercentDecodeRejectsInvalidSequences)
{
    EXPECT_EQ(std::move(percent_decode("a%20b%2Fc~")).unwrap(), "a b/c~");
    EXPECT_EQ(std::move(percent_decode("%C3%BC")).unwrap(), "ü");
    // '+' 不作空格处理。
    EXPECT_EQ(std::move(percent_decode("a+b")).unwrap(), "a+b");

    EXPECT_EQ(percent_decode("%").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(percent_decode("%2").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(percent_decode("100%").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(percent_decode("%zz").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(percent_decode("%2G").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
}

TEST(HttpUrlRfc3986Test, ParsesQueryParamsInOrderWithRepeatedKeys)
{
    auto params = std::move(parse_query_params("a=1&b=2&a=3").unwrap());
    ASSERT_EQ(params.size(), 3U);
    EXPECT_EQ(params[0].key, "a");
    EXPECT_EQ(params[0].value, "1");
    EXPECT_EQ(params[1].key, "b");
    EXPECT_EQ(params[1].value, "2");
    // 重复 key 保留出现顺序。
    EXPECT_EQ(params[2].key, "a");
    EXPECT_EQ(params[2].value, "3");

    // 无 '=' 的项 value 为空。
    auto no_equals = std::move(parse_query_params("flag&x=1").unwrap());
    ASSERT_EQ(no_equals.size(), 2U);
    EXPECT_EQ(no_equals[0].key, "flag");
    EXPECT_EQ(no_equals[0].value, "");
    EXPECT_EQ(no_equals[1].key, "x");

    // 空段跳过；空 query 产生空列表。
    EXPECT_TRUE(std::move(parse_query_params("").unwrap()).empty());
    EXPECT_TRUE(std::move(parse_query_params("&&").unwrap()).empty());

    // 空值 '=' 项保留。
    auto empty_value = std::move(parse_query_params("k=&x").unwrap());
    ASSERT_EQ(empty_value.size(), 2U);
    EXPECT_EQ(empty_value[0].key, "k");
    EXPECT_EQ(empty_value[0].value, "");

    // key/value 均做 percent-decode。
    auto encoded = std::move(parse_query_params("k%20a=v%2F1&%C3%BC=x").unwrap());
    ASSERT_EQ(encoded.size(), 2U);
    EXPECT_EQ(encoded[0].key, "k a");
    EXPECT_EQ(encoded[0].value, "v/1");
    EXPECT_EQ(encoded[1].key, "ü");

    // 非法 percent 序列报 InvalidUrl。
    EXPECT_EQ(parse_query_params("k=%2").unwrap_err().kind(), HttpErrorKind::InvalidUrl);
    EXPECT_EQ(parse_query_params("%zz=1").unwrap_err().kind(), HttpErrorKind::InvalidUrl);

    // 成员便捷方法：与独立解析结果一致。
    auto url = HttpUrl::parse("http://example.com/p?a=1&a=2&b=%20x%20");
    ASSERT_TRUE(url.is_ok());
    auto member_params = url.unwrap().query_params();
    ASSERT_EQ(member_params.size(), 3U);
    EXPECT_EQ(member_params[0].key, "a");
    EXPECT_EQ(member_params[0].value, "1");
    EXPECT_EQ(member_params[1].value, "2");
    EXPECT_EQ(member_params[2].key, "b");
    EXPECT_EQ(member_params[2].value, " x ");
}

TEST(HttpUrlRfc3986Test, NormalizesCasePortAndDotSegments)
{
    auto mixed = HttpUrl::parse("HTTP://EXAMPLE.COM:80/A/../B/%7Euser?Q=1#F");
    ASSERT_TRUE(mixed.is_ok());
    auto normalized = mixed.unwrap().normalize();
    EXPECT_EQ(normalized.scheme(), HttpScheme::Http);
    EXPECT_EQ(normalized.host(), "example.com");
    EXPECT_FALSE(normalized.has_explicit_port());
    EXPECT_EQ(normalized.port(), 80);
    // %7E 是 unreserved '~'，解码；dot 段消解后 path 变短。
    EXPECT_EQ(normalized.path(), "/B/~user");
    EXPECT_EQ(normalized.query(), "Q=1");
    EXPECT_EQ(normalized.fragment(), "F");
    EXPECT_EQ(normalized.to_string(), "http://example.com/B/~user?Q=1#F");
    expect_round_trip(normalized);

    auto dot_segments = HttpUrl::parse("http://example.com/a/b/c/../../d");
    ASSERT_TRUE(dot_segments.is_ok());
    EXPECT_EQ(dot_segments.unwrap().normalize().path(), "/a/d");

    auto trailing_dots = HttpUrl::parse("http://example.com/a/b/..");
    ASSERT_TRUE(trailing_dots.is_ok());
    EXPECT_EQ(trailing_dots.unwrap().normalize().path(), "/a/");

    auto above_root = HttpUrl::parse("http://example.com/../../x");
    ASSERT_TRUE(above_root.is_ok());
    EXPECT_EQ(above_root.unwrap().normalize().path(), "/x");

    auto current_segment = HttpUrl::parse("http://example.com/a/./b/");
    ASSERT_TRUE(current_segment.is_ok());
    EXPECT_EQ(current_segment.unwrap().normalize().path(), "/a/b/");

    // 保留字符不解码，只统一 %XX 大小写。
    auto reserved = HttpUrl::parse("http://example.com/a%2fb");
    ASSERT_TRUE(reserved.is_ok());
    EXPECT_EQ(reserved.unwrap().normalize().path(), "/a%2Fb");

    // 非默认端口保留。
    auto custom_port = HttpUrl::parse("https://EXAMPLE.com:8443/x");
    ASSERT_TRUE(custom_port.is_ok());
    auto normalized_port = custom_port.unwrap().normalize();
    EXPECT_EQ(normalized_port.host(), "example.com");
    EXPECT_TRUE(normalized_port.has_explicit_port());
    EXPECT_EQ(normalized_port.port(), 8443);
    EXPECT_EQ(normalized_port.to_string(), "https://example.com:8443/x");
}

TEST(HttpUrlRfc3986Test, ToStringRoundTripsAcrossComponentShapes)
{
    const std::string urls[] = {
        "http://example.com",
        "https://example.com:8443",
        "http://example.com/",
        "https://u%20p@example.com:8443/p%20a?q%20=%201#f%20",
        "http://u@h:p@host.example/path",
        "https://[2001:DB8::1]:80/path?query#fragment",
        "http://[::1]/",
        "http://example.com?",
        "http://example.com#",
        "http://example.com/a/b/../c?x#y",
        "http://example.com?x",
        "https://example.com:443/x",
        "http://example.com:65535/x",
    };
    for (const auto& text : urls) {
        auto url = HttpUrl::parse(text);
        ASSERT_TRUE(url.is_ok()) << text << ": " << url.unwrap_err().to_string();
        SCOPED_TRACE(text);
        expect_round_trip(url.unwrap());
    }
}

}   // namespace
}   // namespace ca::http::test
