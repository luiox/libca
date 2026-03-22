#include "StringUtil.hpp"
#include <sstream>
#include <algorithm>
#include <cstdarg>
#include <cctype>

namespace ca {

////////////////////////////////////////////////////////////////////////////////

// 字符串辅助工具类

std::string StringUtil::toLowerCase(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string StringUtil::toUpperCase(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

char StringUtil::toChar(const std::string& input)
{
    char              c = 0;
    std::stringstream ss;
    ss << input;
    ss >> c;
    return c;
}

short StringUtil::toShort(const std::string& input)
{
    short             s = 0;
    std::stringstream ss;
    ss << input;
    ss >> s;
    return s;
}

int StringUtil::toInt(const std::string& input)
{
    // return atoi(input.c_str());
    int               i = 0;
    std::stringstream ss;
    ss << input;
    ss >> i;
    return i;
}

long StringUtil::toLong(const std::string& input)
{
    // return atol(input.c_str());
    long              l = 0;
    std::stringstream ss;
    ss << input;
    ss >> l;
    return l;
}

float StringUtil::toFloat(const std::string& input)
{
    float             f = 0.0;
    std::stringstream ss;
    ss << input;
    ss >> f;
    return f;
}

double StringUtil::toDouble(const std::string& input)
{
    // return atof(input.c_str());
    double            d = 0.0;
    std::stringstream ss;
    ss << input;
    ss >> d;
    return d;
}

std::string StringUtil::toString(char c)
{
    std::ostringstream os;
    os << c;
    return os.str();
}

std::string StringUtil::toString(short s)
{
    std::ostringstream os;
    os << s;
    return os.str();
}

std::string StringUtil::toString(int i)
{
    std::stringstream os;
    os << i;
    return os.str();
}

std::string StringUtil::toString(long l)
{
    std::stringstream os;
    os << l;
    return os.str();
}

std::string StringUtil::toString(float f)
{
    std::ostringstream os;
    os << f;
    return os.str();
}

std::string StringUtil::toString(double d)
{
    std::ostringstream os;
    os << d;
    return os.str();
}

std::string StringUtil::trimStart(const std::string& input)
{
    return trimStart(input, " \r\n");
}

std::string StringUtil::trimStart(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return trimStart(input, str.c_str());
}

std::string StringUtil::trimStart(const std::string& input, const char* trims)
{
    std::string delimiter = trims;
    std::string str       = input;
    size_t      found;
    found = str.find_first_not_of(delimiter);
    if (found != std::string::npos)
        str.erase(0, found);
    else
        str.clear();
    return str;
}

std::string StringUtil::trimEnd(const std::string& input)
{
    return trimEnd(input, " \r\n");
}

std::string StringUtil::trimEnd(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return trimEnd(input, str.c_str());
}

std::string StringUtil::trimEnd(const std::string& input, const char* delims)
{
    std::string delimiter = delims;
    std::string str       = input;
    size_t      found;
    found = str.find_last_not_of(delimiter);
    if (found != std::string::npos)
        str.erase(found + 1);
    else
        str.clear();
    return str;
}

std::string StringUtil::trim(const std::string& input)
{
    return trim(input, " \r\n");
}

std::string StringUtil::trim(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return StringUtil::trim(input, str.c_str());
}

std::string StringUtil::trim(const std::string& input, const char* trims)
{
    std::string str = trimStart(input, trims);
    return trimEnd(str, trims);
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input)
{
    output.clear();
    std::istringstream iss(input);
    copy(std::istream_iterator<std::string>(iss),
         std::istream_iterator<std::string>(),
         back_inserter(output));
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input, char separator)
{
    output.clear();
    std::stringstream ss(input);
    std::string       item;
    while (getline(ss, item, separator)) {
        output.push_back(item);
    }
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input,
                       const std::string& separators)
{
    output.clear();
    size_t last  = 0;
    size_t index = input.find_first_of(separators, last);
    while (index != std::string::npos) {
        std::string str = input.substr(last, index - last);
        output.push_back(str);
        last  = index + 1;
        index = input.find_first_of(separators, last);
    }
    if (index - last > 0) {
        output.push_back(input.substr(last, index - last));
    }
}

std::string StringUtil::join(std::vector<std::string>& input)
{
    return join(input, ' ');
}

std::string StringUtil::join(std::vector<std::string>& input, char separator)
{
    std::ostringstream                 os;
    std::vector<std::string>::iterator it;
    for (it = input.begin(); it != input.end(); it++) {
        os << *it;
        os << separator;
    }
    return os.str();
}

std::string StringUtil::join(std::vector<std::string>& input, const char* separators)
{
    std::stringstream                  os;
    std::vector<std::string>::iterator it;
    for (it = input.begin(); it != input.end(); it++) {
        os << *it;
        os << separators;
    }
    return os.str();
}

std::string StringUtil::capitalize(const std::string& input)
{
    std::ostringstream os;
    int                size = input.size();
    for (int i = 0; i < size; i++) {
        if (i == 0) {
            if (input[i] >= 97 && input[i] <= 122) {
                os << (char)(input[i] - 32);
            }
        }
        else {
            os << input[i];
        }
    }
    return os.str();
}

int strcasecmp(const char* s1, const char* s2)
{
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            break;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int StringUtil::compare(const std::string& strA, const std::string& strB, bool ignoreCase)
{
    if (ignoreCase) {
        return strcasecmp(strA.c_str(), strB.c_str());
    }
    else {
        return strA.compare(strB);
    }
}

std::string StringUtil::format(const char* format, ...)
{
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsnprintf(buf, sizeof(buf), format, arg_ptr);
    va_end(arg_ptr);
    return std::string(buf);
}

bool StringUtil::isNumeric(const std::string& input)
{
    if (input.find_first_not_of("0123456789.") != std::string::npos)
        return false;
    if (count(input.begin(), input.end(), '.') > 1)
        return false;
    return true;
}
}   // namespace ca
