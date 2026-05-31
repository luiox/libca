///
/// @brief 日期时间组件
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::time，提供 Date / Time / DateTime 基本类型
///

#pragma once

#include <ctime>
#include <string>
#include <tuple>

namespace ca::time {

/// 日期（年/月/日）
class Date {
public:
    Date(int year, int month, int day);
    explicit Date(const std::string& date);  ///< ISO 格式 "YYYY-MM-DD"

    int year() const noexcept { return year_; }
    int month() const noexcept { return month_; }
    int day() const noexcept { return day_; }

    /// 格式化为 "YYYY-MM-DD"
    std::string toString() const;

private:
    int year_;
    int month_;
    int day_;
};

/// 时间（时/分/秒）
class Time {
public:
    Time(int hour, int minute, int second);
    explicit Time(const std::string& time);  ///< 格式 "HH:MM:SS"

    int hour() const noexcept { return hour_; }
    int minute() const noexcept { return minute_; }
    int second() const noexcept { return second_; }

    /// 格式化为 "HH:MM:SS"
    std::string toString() const;

private:
    int hour_;
    int minute_;
    int second_;
};

/// 日期时间工具
struct DateTime {
    /// 获取当前本地日期时间
    static std::tuple<Date, Time> now();
};

} // namespace ca::time
