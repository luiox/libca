/// @file datetime.hpp
/// @brief Date / Time / DateTime — 日期、时间、日期时间基本类型。
///
/// 命名空间 ca::time。Date 与 Time 是纯值类型，仅保存日历字段；
/// DateTime 提供 now() 入口，返回本地时区拆分后的 (Date, Time)。

#pragma once

#include <ctime>
#include <string>
#include <tuple>

namespace ca::time {

/// @brief 日期值类型（年/月/日），不做范围校验，依赖调用方保证合法。
class Date {
public:
    /// @brief 由年/月/日构造，不做合法性校验。
    Date(int year, int month, int day);

    /// @brief 解析 ISO 格式字符串 "YYYY-MM-DD"。
    explicit Date(const std::string& date);

    int year() const noexcept { return year_; }
    int month() const noexcept { return month_; }
    int day() const noexcept { return day_; }

    /// @brief 格式化为 "YYYY-MM-DD"。
    /// @return 固定 10 字符 ISO 格式字符串。
    std::string toString() const;

private:
    int year_;
    int month_;
    int day_;
};

/// @brief 时间值类型（时/分/秒），不做范围校验，依赖调用方保证合法。
class Time {
public:
    /// @brief 由时/分/秒构造，不做合法性校验。
    Time(int hour, int minute, int second);

    /// @brief 解析格式字符串 "HH:MM:SS"。
    explicit Time(const std::string& time);

    int hour() const noexcept { return hour_; }
    int minute() const noexcept { return minute_; }
    int second() const noexcept { return second_; }

    /// @brief 格式化为 "HH:MM:SS"。
    /// @return 固定 8 字符格式字符串。
    std::string toString() const;

private:
    int hour_;
    int minute_;
    int second_;
};

/// @brief 日期时间工具，仅提供当前时间入口。
struct DateTime {
    /// @brief 获取当前本地日期时间。
    /// @return (Date, Time)，基于 std::time 和 std::localtime 拆分。
    static std::tuple<Date, Time> now();
};

} // namespace ca::time
