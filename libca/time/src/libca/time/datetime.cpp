//
// @brief 日期时间组件实现
//

#include "datetime.hpp"

#include <cstdio>
#include <stdexcept>

namespace ca::time {

// ============================================================================
// Date
// ============================================================================

Date::Date(int year, int month, int day)
    : year_(year), month_(month), day_(day) {}

Date::Date(const std::string& date) {
    if (date.length() < 10) {
        throw std::invalid_argument("Date: invalid format, expected YYYY-MM-DD");
    }
    year_ = std::stoi(date.substr(0, 4));
    month_ = std::stoi(date.substr(5, 2));
    day_ = std::stoi(date.substr(8, 2));
}

std::string Date::toString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year_, month_, day_);
    return std::string(buf);
}

// ============================================================================
// Time
// ============================================================================

Time::Time(int hour, int minute, int second)
    : hour_(hour), minute_(minute), second_(second) {}

Time::Time(const std::string& time) {
    if (time.length() < 8) {
        throw std::invalid_argument("Time: invalid format, expected HH:MM:SS");
    }
    hour_ = std::stoi(time.substr(0, 2));
    minute_ = std::stoi(time.substr(3, 2));
    second_ = std::stoi(time.substr(6, 2));
}

std::string Time::toString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour_, minute_, second_);
    return std::string(buf);
}

// ============================================================================
// DateTime
// ============================================================================

std::tuple<Date, Time> DateTime::now() {
    std::time_t t = std::time(nullptr);
    struct tm* now = std::localtime(&t);
    return std::make_tuple(
        Date(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday),
        Time(now->tm_hour, now->tm_min, now->tm_sec)
    );
}

} // namespace ca::time
