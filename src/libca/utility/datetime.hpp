#ifndef DATE_TIME_H
#define DATE_TIME_H

#include <ctime>
#include <string>
#include <tuple>

namespace libca::utility {

class Date
{
private:
    int year;
    int month;
    int day;

public:
    Date(int year, int month, int day);
    Date(const std::string& date);
    int getYear() const;
    int getMonth() const;
    int getDay() const;
};

class Time
{
private:
    int hour;
    int minute;
    int second;

public:
    Time(int hour, int minute, int second);
    Time(const std::string& time);
    int getHour() const;
    int getMinute() const;
    int getSecond() const;
};

class DateTime
{
public:
    static std::tuple<Date, Time> now()
    {
        time_t     t   = time(0);
        struct tm* now = localtime(&t);
        return std::make_tuple(Date(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday),
                               Time(now->tm_hour, now->tm_min, now->tm_sec));
    }
};

}   // namespace libca::utility

#endif   // !DATE_TIME_H