#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/http/http_error.hpp"

namespace ca::http {

/// @brief HTTP URL scheme。
enum class HttpScheme
{
    Http,
    Https
};

/// @brief query 参数列表中的一项；key/value 已 percent-decode。
struct HttpQueryParam
{
    std::string key;    ///< percent-decode 后的参数名。
    std::string value;  ///< percent-decode 后的参数值；无 '=' 的项为空字符串。
};

/// @brief percent-encode 任意字节串。
/// @return 编码结果：保留 unreserved 字符（A-Z a-z 0-9 - . _ ~），其余字节编码为 %XX 大写十六进制。
std::string percent_encode(std::string_view value);

/// @brief percent-decode 任意字节串。
/// @return 解码结果；'%' 后不足两个十六进制位或含非十六进制字节时返回 InvalidUrl 错误。
/// @note '+' 不作空格处理；结果不要求是合法 UTF-8。
HttpResult<std::string> percent_decode(std::string_view value);

/// @brief 按 '&' 分隔把 query 文本解析为有序 kv 列表，重复 key 按出现顺序保留。
/// @param query 不含 '?' 分隔符的 query 原始文本；空文本产生空列表。
/// @return 参数列表；任一 key/value 含非法 percent 序列时返回 InvalidUrl 错误。
/// @note 无 '=' 的项 value 为空字符串；连续或结尾的 '&' 产生空段并被跳过。
HttpResult<std::vector<HttpQueryParam>> parse_query_params(std::string_view query);

/// @brief 拥有 http/https absolute URL 全量分解结果的值类型。
class HttpUrl
{
public:
    /// @brief 解析 absolute URL，拆分 scheme、userinfo、host、port、path、query 与 fragment。
    /// @details host 支持 IPv6 方括号形式；path 为空时规范化为 `/`。拒绝空 host、非法端口、
    ///          控制字符、反斜杠与非法 percent 序列。
    static HttpResult<HttpUrl> parse(std::string_view value);

    /// @brief 返回 URL scheme。
    HttpScheme scheme() const noexcept;

    /// @brief 返回 userinfo 原始文本（保留 percent-encoding，不含 '@'）；未携带时为空字符串。
    const std::string& userinfo() const noexcept;

    /// @brief 返回 authority 是否携带 userinfo；'@' 分隔符出现即视为携带，即使内容为空。
    bool has_userinfo() const noexcept;

    /// @brief 返回不含 IPv6 方括号的 host。
    const std::string& host() const noexcept;

    /// @brief 返回显式端口或 scheme 默认端口。
    u16 port() const noexcept;

    /// @brief 返回输入是否显式指定端口。
    bool has_explicit_port() const noexcept;

    /// @brief 返回 path 原始文本（保留 percent-encoding）；空 path 规范化为 `/`。
    const std::string& path() const noexcept;

    /// @brief 返回 URL 是否携带 query 分隔符 '?'。
    bool has_query() const noexcept;

    /// @brief 返回 query 原始文本（保留 percent-encoding，不含 '?'）；未携带时为空字符串。
    const std::string& query() const noexcept;

    /// @brief 返回 URL 是否携带 fragment 分隔符 '#'。
    bool has_fragment() const noexcept;

    /// @brief 返回 fragment 原始文本（保留 percent-encoding，不含 '#'）；未携带时为空字符串。
    const std::string& fragment() const noexcept;

    /// @brief 返回 origin-form path + query，至少为 `/`；fragment 不参与。
    const std::string& target() const noexcept;

    /// @brief 返回 Host header 使用的 authority，按需补 IPv6 方括号和非默认端口；不含 userinfo。
    std::string authority() const;

    /// @brief 返回 query 的有序参数列表；key/value 已 percent-decode。
    /// @note parse 保证 query 的 percent 序列合法，本方法不会失败。
    std::vector<HttpQueryParam> query_params() const;

    /// @brief 序列化为完整 URI（scheme://userinfo@host:port/path?query#fragment）。
    /// @note parse(to_string()) 与原实例的组件分解一致（round-trip）。
    std::string to_string() const;

    /// @brief 返回 RFC 3986 §6.2.2 语法归一化副本。
    /// @details scheme 与 host 转 ASCII 小写；显式默认端口改为省略；percent 编码先规范化
    ///          （unreserved 解码、其余统一大写 %XX），再消解 path 中的 "." 与 ".." 段。
    HttpUrl normalize() const;

private:
    HttpUrl(HttpScheme scheme, std::string userinfo, bool has_userinfo, std::string host,
            u16 port, bool explicit_port, std::string path, bool has_query, std::string query,
            bool has_fragment, std::string fragment) noexcept;

    HttpScheme  scheme_{HttpScheme::Http};
    std::string userinfo_;
    bool        has_userinfo_{false};
    std::string host_;
    u16         port_{80};
    bool        explicit_port_{false};
    std::string path_{"/"};
    bool        has_query_{false};
    std::string query_;
    bool        has_fragment_{false};
    std::string fragment_;
    std::string target_{"/"};
};

}   // namespace ca::http
