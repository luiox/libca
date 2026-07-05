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
    static std::string toLowerCase(const std::string& input);
    static std::string toUpperCase(const std::string& input);
    static std::string capitalize(const std::string& input);

    // ==================== 字符串转数值 ====================
    static char   toChar(const std::string& input);
    static short  toShort(const std::string& input);
    static int    toInt(const std::string& input);
    static long   toLong(const std::string& input);
    static float  toFloat(const std::string& input);
    static double toDouble(const std::string& input);

    // ==================== 数值转字符串 ====================
    static std::string toString(char c);
    static std::string toString(short s);
    static std::string toString(int i);
    static std::string toString(long l);
    static std::string toString(float f);
    static std::string toString(double d);

    // ==================== 修剪 ====================
    static std::string trimStart(const std::string& input);
    static std::string trimStart(const std::string& input, char trim);
    static std::string trimStart(const std::string& input, const char* trims);

    static std::string trimEnd(const std::string& input);
    static std::string trimEnd(const std::string& input, char trim);
    static std::string trimEnd(const std::string& input, const char* trims);

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
    static bool isNumeric(const std::string& input);

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
    static std::string url_encode_component(const std::string& input);

    /// @brief URL 表单组件解码：'+' 解码为空格，并解析 %HH。
    static ca::core::Result<std::string, std::string> url_decode_component(const std::string& input);

    /// @brief Base64url 编码，使用 '-' 和 '_'，默认不输出 '=' padding。
    static std::string base64_url_encode(const std::string& input, bool padding = false);

    /// @brief Base64url 解码，接受无 padding 或带 '=' padding 的输入。
    static ca::core::Result<std::string, std::string> base64_url_decode(const std::string& input);

    // ==================== 前缀/后缀/包含 ====================
    static bool startsWith(const std::string& input, const std::string& prefix);
    static bool endsWith(const std::string& input, const std::string& suffix);
    static bool contains(const std::string& input, const std::string& substr);

    
};

} // namespace ca::str
