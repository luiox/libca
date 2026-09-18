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

bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month) {
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    return kDays[month - 1];
}

}  // namespace

// ============================================================================
// Date
// ============================================================================

Date::Date(int year, int month, int day)
    : year_(year), month_(month), day_(day) {}

ca::core::Result<Date, std::string> Date::from_string(const std::string& date) {
    // 严格校验 "YYYY-MM-DD"：恰好 10 字符（不接受尾部脏字符），字段形状
    // 匹配后还要过真实日历范围检查（月份 1-12、日在当月天数内）。
    if (date.length() != 10) {
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
    const int year  = digits_to_int(s, 4);
    const int month = digits_to_int(s + 5, 2);
    const int day   = digits_to_int(s + 8, 2);
    if (month < 1 || month > 12) {
        return ca::core::Err(std::string("Date: month out of range"));
    }
    if (day < 1 || day > days_in_month(year, month)) {
        return ca::core::Err(std::string("Date: day out of range"));
    }
    return ca::core::Ok(Date(year, month, day));
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
    // 严格校验 "HH:MM:SS"：恰好 8 字符 + 字段范围（时 0-23、分/秒 0-59）。
    if (time.length() != 8) {
        return ca::core::Err(std::string("Time: invalid format, expected HH:MM:SS"));
    }
    const char* s = time.c_str();
    const bool shape_ok = is_ascii_digit(s[0]) && is_ascii_digit(s[1]) && s[2] == ':' &&
                          is_ascii_digit(s[3]) && is_ascii_digit(s[4]) && s[5] == ':' &&
                          is_ascii_digit(s[6]) && is_ascii_digit(s[7]);
    if (!shape_ok) {
        return ca::core::Err(std::string("Time: invalid format, expected HH:MM:SS"));
    }
    const int hour   = digits_to_int(s, 2);
    const int minute = digits_to_int(s + 3, 2);
    const int second = digits_to_int(s + 6, 2);
    if (hour > 23) {
        return ca::core::Err(std::string("Time: hour out of range"));
    }
    if (minute > 59 || second > 59) {
        return ca::core::Err(std::string("Time: minute/second out of range"));
    }
    return ca::core::Ok(Time(hour, minute, second));
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
