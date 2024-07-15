#ifndef LIBCA_UTILITY_STRING_UTIL_H
#define LIBCA_UTILITY_STRING_UTIL_H

#include <sstream>
#include <vector>
#include <string>


namespace libca::utility {

class StringUtil
{
public:
    static std::string to_lower(const std::string& input);
    static std::string to_upper(const std::string& input);

    static char   to_char(const std::string& input);
    static short  to_short(const std::string& input);
    static int    to_int(const std::string& input);
    static long   to_long(const std::string& input);
    static float  to_float(const std::string& input);
    static double to_double(const std::string& input);

    static std::string to_string(char c);
    static std::string to_string(short s);
    static std::string to_string(int i);
    static std::string to_string(long l);
    static std::string to_string(float f);
    static std::string to_string(double d);

    static std::string trim_start(const std::string& input);
    static std::string trim_start(const std::string& input, char trim);
    static std::string trim_start(const std::string& input, const char* trims);

    static std::string trim_end(const std::string& input);
    static std::string trim_end(const std::string& input, char trim);
    static std::string trim_end(const std::string& input, const char* trims);

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

    static bool is_numeric(const std::string& input);
};

}   // namespace libca::utility

#endif   // !LIBCA_UTILITY_STRING_UTIL_H
