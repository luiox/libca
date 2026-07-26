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
