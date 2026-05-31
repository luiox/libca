///
/// @brief 字符串辅助工具类实现
///

#include "string_util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <iterator>
#include <sstream>

namespace ca::str {

// ==================== 大小写转换 ====================

std::string StringUtil::toLowerCase(const std::string& input) {
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string StringUtil::toUpperCase(const std::string& input) {
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::string StringUtil::capitalize(const std::string& input) {
    if (input.empty()) return input;
    std::string result = input;
    if (result[0] >= 'a' && result[0] <= 'z') {
        result[0] = static_cast<char>(result[0] - 32);
    }
    return result;
}

// ==================== 字符串转数值 ====================

char StringUtil::toChar(const std::string& input) {
    char c = 0;
    std::istringstream ss(input);
    ss >> c;
    return c;
}

short StringUtil::toShort(const std::string& input) {
    short s = 0;
    std::istringstream ss(input);
    ss >> s;
    return s;
}

int StringUtil::toInt(const std::string& input) {
    int i = 0;
    std::istringstream ss(input);
    ss >> i;
    return i;
}

long StringUtil::toLong(const std::string& input) {
    long l = 0;
    std::istringstream ss(input);
    ss >> l;
    return l;
}

float StringUtil::toFloat(const std::string& input) {
    float f = 0.0f;
    std::istringstream ss(input);
    ss >> f;
    return f;
}

double StringUtil::toDouble(const std::string& input) {
    double d = 0.0;
    std::istringstream ss(input);
    ss >> d;
    return d;
}

// ==================== 数值转字符串 ====================

std::string StringUtil::toString(char c) {
    return std::string(1, c);
}

std::string StringUtil::toString(short s) {
    return std::to_string(s);
}

std::string StringUtil::toString(int i) {
    return std::to_string(i);
}

std::string StringUtil::toString(long l) {
    return std::to_string(l);
}

std::string StringUtil::toString(float f) {
    return std::to_string(f);
}

std::string StringUtil::toString(double d) {
    return std::to_string(d);
}

// ==================== 修剪 ====================

std::string StringUtil::trimStart(const std::string& input) {
    return trimStart(input, " \r\n");
}

std::string StringUtil::trimStart(const std::string& input, char trim) {
    return trimStart(input, std::string(1, trim).c_str());
}

std::string StringUtil::trimStart(const std::string& input, const char* trims) {
    std::string str = input;
    auto found = str.find_first_not_of(trims);
    if (found != std::string::npos)
        str.erase(0, found);
    else
        str.clear();
    return str;
}

std::string StringUtil::trimEnd(const std::string& input) {
    return trimEnd(input, " \r\n");
}

std::string StringUtil::trimEnd(const std::string& input, char trim) {
    return trimEnd(input, std::string(1, trim).c_str());
}

std::string StringUtil::trimEnd(const std::string& input, const char* delims) {
    std::string str = input;
    auto found = str.find_last_not_of(delims);
    if (found != std::string::npos)
        str.erase(found + 1);
    else
        str.clear();
    return str;
}

std::string StringUtil::trim(const std::string& input) {
    return trim(input, " \r\n");
}

std::string StringUtil::trim(const std::string& input, char trim) {
    return StringUtil::trim(input, std::string(1, trim).c_str());
}

std::string StringUtil::trim(const std::string& input, const char* trims) {
    return trimEnd(trimStart(input, trims), trims);
}

// ==================== 拆分与合并 ====================

void StringUtil::split(std::vector<std::string>& output, const std::string& input) {
    output.clear();
    std::istringstream iss(input);
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              std::back_inserter(output));
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input, char separator) {
    output.clear();
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, separator)) {
        output.push_back(item);
    }
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input,
                       const std::string& separators) {
    output.clear();
    size_t last = 0;
    size_t index = input.find_first_of(separators, last);
    while (index != std::string::npos) {
        output.push_back(input.substr(last, index - last));
        last = index + 1;
        index = input.find_first_of(separators, last);
    }
    if (last < input.length()) {
        output.push_back(input.substr(last));
    }
}

std::string StringUtil::join(const std::vector<std::string>& input) {
    return join(input, ' ');
}

std::string StringUtil::join(const std::vector<std::string>& input, char separator) {
    std::ostringstream os;
    for (const auto& item : input) {
        if (&item != &input.front()) os << separator;
        os << item;
    }
    return os.str();
}

std::string StringUtil::join(const std::vector<std::string>& input, const char* separators) {
    std::ostringstream os;
    for (const auto& item : input) {
        if (&item != &input.front()) os << separators;
        os << item;
    }
    return os.str();
}

// ==================== 比较 ====================

// POSIX strcasecmp 在 Windows 上为 _stricmp，此处提供便携实现
namespace {
    int portableStrcasecmp(const char* s1, const char* s2) {
        while (*s1 && *s2) {
            int c1 = std::tolower(static_cast<unsigned char>(*s1));
            int c2 = std::tolower(static_cast<unsigned char>(*s2));
            if (c1 != c2) return c1 - c2;
            s1++;
            s2++;
        }
        return std::tolower(static_cast<unsigned char>(*s1)) -
               std::tolower(static_cast<unsigned char>(*s2));
    }
}

int StringUtil::compare(const std::string& strA, const std::string& strB, bool ignoreCase) {
    if (ignoreCase) {
        return portableStrcasecmp(strA.c_str(), strB.c_str());
    }
    return strA.compare(strB);
}

// ==================== 判断 ====================

bool StringUtil::isNumeric(const std::string& input) {
    if (input.empty()) return false;
    size_t start = (input[0] == '-') ? 1 : 0;
    if (start >= input.length()) return false;
    for (size_t i = start; i < input.length(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(input[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace ca::str
