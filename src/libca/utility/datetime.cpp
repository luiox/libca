#include <libca/utility/datetime.hpp>

namespace libca::utility {
Date::Date(int year, int month, int day) {}
Date::Date(const std::string& date) {}
int Date::getYear() const
{
    return year;
}
int Date::getMonth() const
{
    return month;
}
int Date::getDay() const
{
    return day;
}

Time::Time(int hour, int minute, int second)
    : hour(hour)
    , minute(minute)
    , second(second)
{}
Time::Time(const std::string& time)
    : hour(std::stoi(time.substr(0, 2)))
    , minute(std::stoi(time.substr(3, 2)))
    , second(std::stoi(time.substr(6, 2)))
{}
int Time::getHour() const
{
    return hour;
}
int Time::getMinute() const
{
    return minute;
}
int Time::getSecond() const
{
    return second;
}

}   // namespace libca::utility
