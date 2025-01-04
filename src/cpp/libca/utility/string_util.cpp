#include <libca/utility/string_util.hpp>

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
#include <cctype>
#include <algorithm>
#include <iterator>

namespace libca::utility {
std::string StringUtil::to_lower(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string StringUtil::to_upper(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

char StringUtil::to_char(const std::string& input)
{
    char              c = 0;
    std::stringstream ss;
    ss << input;
    ss >> c;
    return c;
}

short StringUtil::to_short(const std::string& input)
{
    short             s = 0;
    std::stringstream ss;
    ss << input;
    ss >> s;
    return s;
}

int StringUtil::to_int(const std::string& input)
{
    // return atoi(input.c_str());
    int               i = 0;
    std::stringstream ss;
    ss << input;
    ss >> i;
    return i;
}

long StringUtil::to_long(const std::string& input)
{
    // return atol(input.c_str());
    long              l = 0;
    std::stringstream ss;
    ss << input;
    ss >> l;
    return l;
}

float StringUtil::to_float(const std::string& input)
{
    float             f = 0.0;
    std::stringstream ss;
    ss << input;
    ss >> f;
    return f;
}

double StringUtil::to_double(const std::string& input)
{
    // return atof(input.c_str());
    double            d = 0.0;
    std::stringstream ss;
    ss << input;
    ss >> d;
    return d;
}

std::string StringUtil::to_string(char c)
{
    std::ostringstream os;
    os << c;
    return os.str();
}

std::string StringUtil::to_string(short s)
{
    std::ostringstream os;
    os << s;
    return os.str();
}

std::string StringUtil::to_string(int i)
{
    std::stringstream os;
    os << i;
    return os.str();
}

std::string StringUtil::to_string(long l)
{
    std::stringstream os;
    os << l;
    return os.str();
}

std::string StringUtil::to_string(float f)
{
    std::ostringstream os;
    os << f;
    return os.str();
}

std::string StringUtil::to_string(double d)
{
    std::ostringstream os;
    os << d;
    return os.str();
}

std::string StringUtil::trim_start(const std::string& input)
{
    return trim_start(input, " \r\n");
}

std::string StringUtil::trim_start(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return trim_start(input, str.c_str());
}

std::string StringUtil::trim_start(const std::string& input, const char* trims)
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

std::string StringUtil::trim_end(const std::string& input)
{
    return trim_end(input, " \r\n");
}

std::string StringUtil::trim_end(const std::string& input, char delim)
{
    std::string str;
    str = delim;
    return trim_end(input, str.c_str());
}

std::string StringUtil::trim_end(const std::string& input, const char* delims)
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
    std::string str = trim_start(input, trims);
    return trim_end(str, trims);
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

bool StringUtil::is_numeric(const std::string& input)
{
    if (input.find_first_not_of("0123456789.") != std::string::npos)
        return false;
    if (count(input.begin(), input.end(), '.') > 1)
        return false;
    return true;
}

}   // namespace libca::utility
