#pragma once

#include <string>
#include <vector>

namespace ca {

////////////////////////////////////////////////////////////////////////////////

// 字符串辅助工具类

class StringUtil
{
public:
    static std::string toLowerCase(const std::string& input);
    static std::string toUpperCase(const std::string& input);

    static char   toChar(const std::string& input);
    static short  toShort(const std::string& input);
    static int    toInt(const std::string& input);
    static long   toLong(const std::string& input);
    static float  toFloat(const std::string& input);
    static double toDouble(const std::string& input);

    static std::string toString(char c);
    static std::string toString(short s);
    static std::string toString(int i);
    static std::string toString(long l);
    static std::string toString(float f);
    static std::string toString(double d);

    static std::string trimStart(const std::string& input);
    static std::string trimStart(const std::string& input, char trim);
    static std::string trimStart(const std::string& input, const char* trims);

    static std::string trimEnd(const std::string& input);
    static std::string trimEnd(const std::string& input, char trim);
    static std::string trimEnd(const std::string& input, const char* trims);

    static std::string trim(const std::string& input);
    static std::string trim(const std::string& input, char trim);
    static std::string trim(const std::string& input, const char* trims);

    static void split(std::vector<std::string>& output, const std::string& input);
    static void split(std::vector<std::string>& output, const std::string& input, char separator);
    static void split(std::vector<std::string>& output, const std::string& input,
                      const std::string& separators);

    static std::string join(std::vector<std::string>& input);
    static std::string join(std::vector<std::string>& input, char separator);
    static std::string join(std::vector<std::string>& input, const char* separators);

    static std::string capitalize(const std::string& input);

    static int compare(const std::string& strA, const std::string& strB, bool ignoreCase = false);
    static std::string format(const char* format, ...);

    static bool isNumeric(const std::string& input);
};

}   // namespace ca
