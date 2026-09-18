#include <gtest/gtest.h>
#include <string>

#include "libca/time/datetime.hpp"

using namespace ca::time;

TEST(DateTest, constructFromYMD) {
    Date d(2026, 5, 31);
    EXPECT_EQ(d.year(), 2026);
    EXPECT_EQ(d.month(), 5);
    EXPECT_EQ(d.day(), 31);
}

TEST(DateTest, fromString) {
    auto r = Date::from_string("2026-05-31");
    ASSERT_TRUE(r.is_ok());
    Date d = r.unwrap();
    EXPECT_EQ(d.year(), 2026);
    EXPECT_EQ(d.month(), 5);
    EXPECT_EQ(d.day(), 31);
}

TEST(DateTest, fromStringRejectsInvalid) {
    EXPECT_TRUE(Date::from_string("").is_err());
    EXPECT_TRUE(Date::from_string("2026").is_err());
    EXPECT_TRUE(Date::from_string("2026/05/31").is_err());
    EXPECT_TRUE(Date::from_string("20a6-05-31").is_err());
}

TEST(DateTest, fromStringRejectsOutOfRange) {
    // 字段范围：月份与日在真实日历下校验（含闰年）。
    EXPECT_TRUE(Date::from_string("2026-13-01").is_err());   // 月 13
    EXPECT_TRUE(Date::from_string("2026-00-10").is_err());   // 月 0
    EXPECT_TRUE(Date::from_string("2026-04-31").is_err());   // 4 月只有 30 天
    EXPECT_TRUE(Date::from_string("2026-02-29").is_err());   // 平年无 2/29
    EXPECT_TRUE(Date::from_string("2024-02-29").is_ok());    // 闰年有 2/29
    EXPECT_TRUE(Date::from_string("2000-02-29").is_ok());    // 世纪闰年
    EXPECT_TRUE(Date::from_string("1900-02-29").is_err());   // 世纪非闰年
    EXPECT_TRUE(Date::from_string("2026-01-00").is_err());   // 日 0
}

TEST(DateTest, fromStringRejectsTrailingGarbage) {
    // 严格长度：尾部脏字符不再被前缀匹配放过。
    EXPECT_TRUE(Date::from_string("2026-01-01XYZ").is_err());
    EXPECT_TRUE(Date::from_string("2026-01-011").is_err());
}

TEST(DateTest, toString) {
    Date d(2026, 1, 2);
    EXPECT_EQ(d.to_string(), "2026-01-02");
}

TEST(DateTest, roundtrip) {
    Date d1(2026, 5, 31);
    Date d2 = Date::from_string(d1.to_string()).unwrap();
    EXPECT_EQ(d1.year(), d2.year());
    EXPECT_EQ(d1.month(), d2.month());
    EXPECT_EQ(d1.day(), d2.day());
}

TEST(TimeTest, constructFromHMS) {
    Time t(14, 30, 0);
    EXPECT_EQ(t.hour(), 14);
    EXPECT_EQ(t.minute(), 30);
    EXPECT_EQ(t.second(), 0);
}

TEST(TimeTest, fromString) {
    auto r = Time::from_string("14:30:00");
    ASSERT_TRUE(r.is_ok());
    Time t = r.unwrap();
    EXPECT_EQ(t.hour(), 14);
    EXPECT_EQ(t.minute(), 30);
    EXPECT_EQ(t.second(), 0);
}

TEST(TimeTest, fromStringRejectsInvalid) {
    EXPECT_TRUE(Time::from_string("").is_err());
    EXPECT_TRUE(Time::from_string("14:30").is_err());
    EXPECT_TRUE(Time::from_string("14-30-00").is_err());
    EXPECT_TRUE(Time::from_string("1a:30:00").is_err());
    // 字段范围与严格长度。
    EXPECT_TRUE(Time::from_string("24:00:00").is_err());
    EXPECT_TRUE(Time::from_string("23:60:00").is_err());
    EXPECT_TRUE(Time::from_string("23:59:60").is_err());
    EXPECT_TRUE(Time::from_string("14:30:00XYZ").is_err());
    EXPECT_TRUE(Time::from_string("23:59:59").is_ok());
}

TEST(TimeTest, toString) {
    Time t(9, 5, 3);
    EXPECT_EQ(t.to_string(), "09:05:03");
}

TEST(TimeTest, roundtrip) {
    Time t1(14, 30, 0);
    Time t2 = Time::from_string(t1.to_string()).unwrap();
    EXPECT_EQ(t1.hour(), t2.hour());
    EXPECT_EQ(t1.minute(), t2.minute());
    EXPECT_EQ(t1.second(), t2.second());
}

TEST(DateTimeTest, nowReturnsValidValues) {
    auto [date, time] = DateTime::now();
    // Just check that the values are reasonable
    EXPECT_GE(date.year(), 2020);
    EXPECT_LE(date.year(), 2100);
    EXPECT_GE(date.month(), 1);
    EXPECT_LE(date.month(), 12);
    EXPECT_GE(date.day(), 1);
    EXPECT_LE(date.day(), 31);
    EXPECT_GE(time.hour(), 0);
    EXPECT_LE(time.hour(), 23);
    EXPECT_GE(time.minute(), 0);
    EXPECT_LE(time.minute(), 59);
    EXPECT_GE(time.second(), 0);
    EXPECT_LE(time.second(), 59);
}
