//
// @brief 字符串辅助工具类实现
//

#include "string_util.hpp"

#include "libca/core/datatype.hpp"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <iterator>
#include <sstream>

namespace ca::str {

namespace {
    constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    constexpr char BASE64_URL_ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    bool is_hex_digit(char ch) {
        return (ch >= '0' && ch <= '9') ||
               (ch >= 'A' && ch <= 'F') ||
               (ch >= 'a' && ch <= 'f');
    }

    ca::i32 hex_value(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return ch - 'a' + 10;
    }

    ca::i32 base64_url_value(char ch) {
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9') return ch - '0' + 52;
        if (ch == '-') return 62;
        if (ch == '_') return 63;
        return -1;
    }

    std::string byte_position_error(const char* message, std::string::size_type pos) {
        return std::string(message) + std::to_string(pos);
    }
}

// ==================== 大小写转换 ====================

std::string StringUtil::to_lower_case(const std::string& input) {
    std::string str = input;
    // 经 unsigned char 中转：char 为有符号时高位字节成负值，直接传 ::tolower 违反
    // 「参数须可表示为 unsigned char 或 EOF」的前置条件（UB，MSVC debug 断言）
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

std::string StringUtil::to_upper_case(const std::string& input) {
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
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

char StringUtil::to_char(const std::string& input) {
    char c = 0;
    std::istringstream ss(input);
    ss >> c;
    return c;
}

short StringUtil::to_short(const std::string& input) {
    short s = 0;
    std::istringstream ss(input);
    ss >> s;
    return s;
}

int StringUtil::to_int(const std::string& input) {
    int i = 0;
    std::istringstream ss(input);
    ss >> i;
    return i;
}

long StringUtil::to_long(const std::string& input) {
    long l = 0;
    std::istringstream ss(input);
    ss >> l;
    return l;
}

float StringUtil::to_float(const std::string& input) {
    float f = 0.0f;
    std::istringstream ss(input);
    ss >> f;
    return f;
}

double StringUtil::to_double(const std::string& input) {
    double d = 0.0;
    std::istringstream ss(input);
    ss >> d;
    return d;
}

// ==================== 数值转字符串 ====================

std::string StringUtil::to_string(char c) {
    return std::string(1, c);
}

std::string StringUtil::to_string(short s) {
    return std::to_string(s);
}

std::string StringUtil::to_string(int i) {
    return std::to_string(i);
}

std::string StringUtil::to_string(long l) {
    return std::to_string(l);
}

std::string StringUtil::to_string(float f) {
    return std::to_string(f);
}

std::string StringUtil::to_string(double d) {
    return std::to_string(d);
}

// ==================== 修剪 ====================

std::string StringUtil::trim_start(const std::string& input) {
    return trim_start(input, " \r\n");
}

std::string StringUtil::trim_start(const std::string& input, char trim) {
    return trim_start(input, std::string(1, trim).c_str());
}

std::string StringUtil::trim_start(const std::string& input, const char* trims) {
    std::string str = input;
    auto found = str.find_first_not_of(trims);
    if (found != std::string::npos)
        str.erase(0, found);
    else
        str.clear();
    return str;
}

std::string StringUtil::trim_end(const std::string& input) {
    return trim_end(input, " \r\n");
}

std::string StringUtil::trim_end(const std::string& input, char trim) {
    return trim_end(input, std::string(1, trim).c_str());
}

std::string StringUtil::trim_end(const std::string& input, const char* delims) {
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
    return trim_end(trim_start(input, trims), trims);
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

bool StringUtil::is_numeric(const std::string& input) {
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

bool StringUtil::is_unreserved_url_char(char ch) {
    return is_ascii_alnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

bool StringUtil::is_ascii_lower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

bool StringUtil::is_ascii_upper(char ch) {
    return ch >= 'A' && ch <= 'Z';
}

bool StringUtil::is_ascii_alpha(char ch) {
    return is_ascii_lower(ch) || is_ascii_upper(ch);
}

bool StringUtil::is_ascii_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool StringUtil::is_ascii_alnum(char ch) {
    return is_ascii_alpha(ch) || is_ascii_digit(ch);
}

char StringUtil::ascii_to_lower(char ch) {
    if (is_ascii_upper(ch)) {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return ch;
}

char StringUtil::ascii_to_upper(char ch) {
    if (is_ascii_lower(ch)) {
        return static_cast<char>(ch - 'a' + 'A');
    }
    return ch;
}

// ==================== URL / percent 编码 ====================

std::string StringUtil::percent_encode(const std::string& input, bool space_as_plus) {
    std::string output;
    output.reserve(input.size());

    for (unsigned char byte : input) {
        char ch = static_cast<char>(byte);
        if (space_as_plus && ch == ' ') {
            output.push_back('+');
        } else if (is_unreserved_url_char(ch)) {
            output.push_back(ch);
        } else {
            output.push_back('%');
            output.push_back(HEX_DIGITS[(byte >> 4) & 0x0F]);
            output.push_back(HEX_DIGITS[byte & 0x0F]);
        }
    }

    return output;
}

ca::core::Result<std::string, std::string> StringUtil::percent_decode(const std::string& input,
                                                                      bool plus_as_space) {
    std::string output;
    output.reserve(input.size());

    for (std::string::size_type i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (plus_as_space && ch == '+') {
            output.push_back(' ');
            continue;
        }
        if (ch != '%') {
            output.push_back(ch);
            continue;
        }
        if (i + 2 >= input.size()) {
            return ca::core::Err(byte_position_error("incomplete percent escape at byte ", i));
        }
        char hi = input[i + 1];
        char lo = input[i + 2];
        if (!is_hex_digit(hi) || !is_hex_digit(lo)) {
            return ca::core::Err(byte_position_error("invalid percent escape at byte ", i));
        }
        auto value = static_cast<unsigned char>((hex_value(hi) << 4) | hex_value(lo));
        output.push_back(static_cast<char>(value));
        i += 2;
    }

    return ca::core::Ok(std::move(output));
}

std::string StringUtil::url_encode_component(const std::string& input) {
    return percent_encode(input, true);
}

ca::core::Result<std::string, std::string> StringUtil::url_decode_component(
    const std::string& input) {
    return percent_decode(input, true);
}

std::string StringUtil::base64UrlEncode(const std::string& input, bool padding) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    ca::u32 buffer = 0;
    ca::i32 bit_count = 0;
    for (unsigned char byte : input) {
        buffer = (buffer << 8) | byte;
        bit_count += 8;
        while (bit_count >= 6) {
            bit_count -= 6;
            output.push_back(BASE64_URL_ALPHABET[(buffer >> bit_count) & 0x3F]);
        }
    }
    if (bit_count > 0) {
        output.push_back(BASE64_URL_ALPHABET[(buffer << (6 - bit_count)) & 0x3F]);
    }
    if (padding) {
        while ((output.size() % 4) != 0) {
            output.push_back('=');
        }
    }

    return output;
}

ca::core::Result<std::string, std::string> StringUtil::base64UrlDecode(
    const std::string& input) {
    std::string output;
    output.reserve((input.size() * 3) / 4);

    ca::u32 buffer = 0;
    ca::i32 bit_count = 0;
    std::string::size_type value_count = 0;
    std::string::size_type padding_count = 0;
    bool seen_padding = false;

    for (std::string::size_type i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (ch == '=') {
            // Padding 只能出现在末尾，且 Base64url 最多需要两个 '='。
            seen_padding = true;
            ++padding_count;
            if (padding_count > 2) {
                return ca::core::Err(byte_position_error("too much base64url padding at byte ", i));
            }
            continue;
        }
        if (seen_padding) {
            return ca::core::Err(byte_position_error("base64url data after padding at byte ", i));
        }

        ca::i32 value = base64_url_value(ch);
        if (value < 0) {
            return ca::core::Err(byte_position_error("invalid base64url character at byte ", i));
        }

        buffer = (buffer << 6) | static_cast<ca::u32>(value);
        bit_count += 6;
        ++value_count;
        if (bit_count >= 8) {
            bit_count -= 8;
            output.push_back(static_cast<char>((buffer >> bit_count) & 0xFF));
        }
    }

    if ((value_count % 4) == 1) {
        return ca::core::Err(std::string("invalid base64url length"));
    }
    if (padding_count > 0) {
        auto expected_padding = (4 - (value_count % 4)) % 4;
        if (expected_padding == 0 || padding_count != expected_padding) {
            return ca::core::Err(std::string("invalid base64url padding"));
        }
    }
    // 严格模式：末尾不足 8 bit 的填充位必须全为 0，否则同一字节串会有多个编码形式。
    if (bit_count > 0 && (buffer & ((static_cast<ca::u32>(1) << bit_count) - 1)) != 0) {
        return ca::core::Err(std::string("invalid base64url trailing bits"));
    }

    return ca::core::Ok(std::move(output));
}

// ==================== 前缀/后缀/包含 ====================

bool StringUtil::starts_with(const std::string& input, const std::string& prefix) {
    if (input.length() < prefix.length()) return false;
    return input.compare(0, prefix.length(), prefix) == 0;
}

bool StringUtil::ends_with(const std::string& input, const std::string& suffix) {
    if (input.length() < suffix.length()) return false;
    return input.compare(input.length() - suffix.length(), suffix.length(), suffix) == 0;
}

bool StringUtil::contains(const std::string& input, const std::string& substr) {
    return input.find(substr) != std::string::npos;
}

} // namespace ca::str
