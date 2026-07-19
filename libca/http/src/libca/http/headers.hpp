#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/http/http_error.hpp"

namespace ca::http {

/// @brief 一个保持原始大小写的 HTTP header 字段。
struct HttpHeader
{
    std::string name;    ///< ASCII token 字段名。
    std::string value;   ///< 不含 CR/LF/NUL 的字段值。
};

/// @brief 保序、允许重复字段、按 ASCII 大小写不敏感查询的 HTTP headers。
class HttpHeaders
{
public:
    /// @brief 校验字段名是否为非空 RFC token。
    static bool valid_name(std::string_view name) noexcept;

    /// @brief 校验字段值不含 CR、LF、NUL 或其它非法控制字符。
    static bool valid_value(std::string_view value) noexcept;

    /// @brief 在末尾追加字段；非法输入返回 InvalidMessage。
    HttpResult<void> append(std::string name, std::string value);

    /// @brief 删除全部同名字段并在末尾写入一个新字段。
    HttpResult<void> set(std::string name, std::string value);

    /// @brief 删除全部同名字段并返回删除数量。
    usize remove(std::string_view name) noexcept;

    /// @brief 返回第一个同名字段值；未找到返回 nullopt。
    /// @note 返回的 view 在本集合下一次修改前有效。
    std::optional<std::string_view> get(std::string_view name) const noexcept;

    /// @brief 返回全部同名字段值，顺序与报文一致。
    /// @note 返回的 view 在本集合下一次修改前有效。
    std::vector<std::string_view> get_all(std::string_view name) const;

    /// @brief 判断是否至少存在一个同名字段。
    bool contains(std::string_view name) const noexcept;

    /// @brief 返回字段数量，重复字段分别计数。
    usize size() const noexcept;

    /// @brief 判断集合是否为空。
    bool is_empty() const noexcept;

    /// @brief 返回保序字段存储。
    const std::vector<HttpHeader>& entries() const noexcept;

private:
    static bool name_equals(std::string_view lhs, std::string_view rhs) noexcept;

    std::vector<HttpHeader> entries_;
};

}   // namespace ca::http
