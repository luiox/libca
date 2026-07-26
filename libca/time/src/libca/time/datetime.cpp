//
// @brief 日期时间组件实现
//

#include "datetime.hpp"

#include <cstdio>
#include <ctime>

namespace ca::time {

namespace {

bool is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

// 解析定长十进制字段（调用方已保证每个字符是数字）。
int digits_to_int(const char* s, int count) {
    int value = 0;
    for (int i = 0; i < count; ++i) value = value * 10 + (s[i] - '0');
    return value;
}

}  // namespace

// ============================================================================
// Date
// ============================================================================

Date::Date(int year, int month, int day)
    : year_(year), month_(month), day_(day) {}

ca::core::Result<Date, std::string> Date::from_string(const std::string& date) {
    // 校验 "YYYY-MM-DD" 的 10 字符前缀；不用 std::stoi（预期失败不该走异常）。
    if (date.length() < 10) {
        return ca::core::Err(std::string("Date: invalid format, expected YYYY-MM-DD"));
    }
    const char* s = date.c_str();
    const bool shape_ok = is_ascii_digit(s[0]) && is_ascii_digit(s[1]) &&
                          is_ascii_digit(s[2]) && is_ascii_digit(s[3]) && s[4] == '-' &&
                          is_ascii_digit(s[5]) && is_ascii_digit(s[6]) && s[7] == '-' &&
                          is_ascii_digit(s[8]) && is_ascii_digit(s[9]);
    if (!shape_ok) {
        return ca::core::Err(std::string("Date: invalid format, expected YYYY-MM-DD"));
    }
    return ca::core::Ok(Date(digits_to_int(s, 4), digits_to_int(s + 5, 2), digits_to_int(s + 8, 2)));
}

std::string Date::to_string() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year_, month_, day_);
    return std::string(buf);
}

// ============================================================================
// Time
// ============================================================================

Time::Time(int hour, int minute, int second)
    : hour_(hour), minute_(minute), second_(second) {}

ca::core::Result<Time, std::string> Time::from_string(const std::string& time) {
    // 校验 "HH:MM:SS" 的 8 字符前缀。
    if (time.length() < 8) {
        return ca::core::Err(std::string("Time: invalid format, expected HH:MM:SS"));
    }
    const char* s = time.c_str();
    const bool shape_ok = is_ascii_digit(s[0]) && is_ascii_digit(s[1]) && s[2] == ':' &&
                          is_ascii_digit(s[3]) && is_ascii_digit(s[4]) && s[5] == ':' &&
                          is_ascii_digit(s[6]) && is_ascii_digit(s[7]);
    if (!shape_ok) {
        return ca::core::Err(std::string("Time: invalid format, expected HH:MM:SS"));
    }
    return ca::core::Ok(Time(digits_to_int(s, 2), digits_to_int(s + 3, 2), digits_to_int(s + 6, 2)));
}

std::string Time::to_string() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour_, minute_, second_);
    return std::string(buf);
}

// ============================================================================
// DateTime
// ============================================================================

std::tuple<Date, Time> DateTime::now() {
    std::time_t t = std::time(nullptr);
    // std::localtime 返回共享静态存储（非线程安全）且可能返回 nullptr，
    // 改用平台的可重入版本写入栈上 tm；极端失败时兜底返回 epoch。
    struct tm now {};
#if defined(_WIN32)
    const bool ok = localtime_s(&now, &t) == 0;
#else
    const bool ok = localtime_r(&t, &now) != nullptr;
#endif
    if (!ok) {
        return std::make_tuple(Date(1970, 1, 1), Time(0, 0, 0));
    }
    return std::make_tuple(
        Date(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday),
        Time(now.tm_hour, now.tm_min, now.tm_sec)
    );
}

} // namespace ca::time
