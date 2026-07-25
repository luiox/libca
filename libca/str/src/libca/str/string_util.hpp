///
/// @brief 字符串辅助工具类
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::str，提供大小写转换、修剪、拆分、合并、类型转换等实用方法
///

#pragma once

#include "libca/core/result.hpp"

#include <string>
#include <vector>

namespace ca::str {

/// @brief std::string 辅助工具（大小写/修剪/拆分/合并/数值互转/比较）。
/// @note 面向 std::string、按字节/ASCII 操作；需要 UTF-8 码点语义请用 Utf8String/Utf8StringRef。
class StringUtil {
public:
    // ==================== 大小写转换 ====================
    static std::string to_lower_case(const std::string& input);
    static std::string to_upper_case(const std::string& input);
    static std::string capitalize(const std::string& input);

    // ==================== 字符串转数值 ====================
    static char   to_char(const std::string& input);
    static short  to_short(const std::string& input);
    static int    to_int(const std::string& input);
    static long   to_long(const std::string& input);
    static float  to_float(const std::string& input);
    static double to_double(const std::string& input);

    // ==================== 数值转字符串 ====================
    static std::string toString(char c);
    static std::string toString(short s);
    static std::string toString(int i);
    static std::string toString(long l);
    static std::string toString(float f);
    static std::string toString(double d);

    // ==================== 修剪 ====================
    static std::string trim_start(const std::string& input);
    static std::string trim_start(const std::string& input, char trim);
    static std::string trim_start(const std::string& input, const char* trims);

    static std::string trim_end(const std::string& input);
    static std::string trim_end(const std::string& input, char trim);
    static std::string trim_end(const std::string& input, const char* trims);

    static std::string trim(const std::string& input);
    static std::string trim(const std::string& input, char trim);
    static std::string trim(const std::string& input, const char* trims);

    // ==================== 拆分与合并 ====================
    static void split(std::vector<std::string>& output, const std::string& input);
    static void split(std::vector<std::string>& output, const std::string& input, char separator);
    static void split(std::vector<std::string>& output, const std::string& input,
                      const std::string& separators);

    static std::string join(const std::vector<std::string>& input);
    static std::string join(const std::vector<std::string>& input, char separator);
    static std::string join(const std::vector<std::string>& input, const char* separators);

    // ==================== 比较 ====================
    static int compare(const std::string& strA, const std::string& strB, bool ignoreCase = false);

    // ==================== 判断 ====================
    static bool is_numeric(const std::string& input);

    /// @brief 判断字符是否属于 RFC 3986 unreserved 集合（A-Z/a-z/0-9/-._~）。
    static bool is_unreserved_url_char(char ch);

    /// @brief 判断字符是否为 ASCII 小写字母。
    static bool is_ascii_lower(char ch);

    /// @brief 判断字符是否为 ASCII 大写字母。
    static bool is_ascii_upper(char ch);

    /// @brief 判断字符是否为 ASCII 字母。
    static bool is_ascii_alpha(char ch);

    /// @brief 判断字符是否为 ASCII 数字。
    static bool is_ascii_digit(char ch);

    /// @brief 判断字符是否为 ASCII 字母或数字。
    static bool is_ascii_alnum(char ch);

    /// @brief 将 ASCII 大写字母转为小写；非 ASCII 大写原样返回。
    static char ascii_to_lower(char ch);

    /// @brief 将 ASCII 小写字母转为大写；非 ASCII 小写原样返回。
    static char ascii_to_upper(char ch);

    // ==================== URL / percent 编码 ====================
    /// @brief 按 RFC 3986 对字符串逐字节百分号编码，unreserved 字符保持原样。
    /// @param input 输入文本，按 UTF-8 字节序列处理，不校验 UTF-8 合法性。
    /// @param space_as_plus true 时空格编码为 '+'，用于表单风格编码。
    /// @return 编码后的字符串，十六进制字母使用大写。
    static std::string percent_encode(const std::string& input, bool space_as_plus = false);

    /// @brief 解码百分号编码字符串。
    /// @param input 输入文本。
    /// @param plus_as_space true 时将 '+' 解码为空格，用于表单风格解码。
    /// @return 成功返回解码后的字节串；遇到不完整或非法十六进制转义返回错误说明。
    static ca::core::Result<std::string, std::string> percent_decode(const std::string& input,
                                                                      bool plus_as_space = false);

    /// @brief URL 表单组件编码：空格编码为 '+'，其它非 unreserved 字节编码为 %HH。
    /// @param input 输入文本，按字节处理。
    /// @return 可放入 query/form 组件的编码字符串。
    static std::string url_encode_component(const std::string& input);

    /// @brief URL 表单组件解码：'+' 解码为空格，并解析 %HH。
    /// @param input URL 表单组件文本。
    /// @return 成功返回解码后的字节串；非法 percent escape 返回错误说明。
    static ca::core::Result<std::string, std::string> url_decode_component(const std::string& input);

    /// @brief Base64url 编码，使用 '-' 和 '_'，默认不输出 '=' padding。
    /// @param input 原始字节串。
    /// @param padding true 时补齐 '='，false 时输出无 padding 形式。
    /// @return Base64url 文本，不插入换行。
    static std::string base64UrlEncode(const std::string& input, bool padding = false);

    /// @brief Base64url 解码，接受无 padding 或带 '=' padding 的输入。
    /// @param input Base64url 文本。
    /// @return 成功返回原始字节串；非法字符、非法长度、非法 padding 或非零尾部填充位返回错误说明。
    /// @note 解码是严格模式，会拒绝 `Zh` / `Zm9` 这类尾部填充位非零的输入。
    static ca::core::Result<std::string, std::string> base64UrlDecode(const std::string& input);

    // ==================== 前缀/后缀/包含 ====================
    static bool starts_with(const std::string& input, const std::string& prefix);
    static bool ends_with(const std::string& input, const std::string& suffix);
    static bool contains(const std::string& input, const std::string& substr);

    
};

} // namespace ca::str
