///
/// @brief 字符串辅助工具类
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::str，提供大小写转换、修剪、拆分、合并、类型转换等实用方法
///

#pragma once

#include <string>
#include <vector>

namespace ca::str {

/// 字符串辅助工具类
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
};

} // namespace ca::str
