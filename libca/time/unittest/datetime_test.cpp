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

TEST(DateTest, constructFromString) {
    Date d("2026-05-31");
    EXPECT_EQ(d.year(), 2026);
    EXPECT_EQ(d.month(), 5);
    EXPECT_EQ(d.day(), 31);
}

TEST(DateTest, toString) {
    Date d(2026, 1, 2);
    EXPECT_EQ(d.toString(), "2026-01-02");
}

TEST(DateTest, roundtrip) {
    Date d1(2026, 5, 31);
    Date d2(d1.toString());
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

TEST(TimeTest, constructFromString) {
    Time t("14:30:00");
    EXPECT_EQ(t.hour(), 14);
    EXPECT_EQ(t.minute(), 30);
    EXPECT_EQ(t.second(), 0);
}

TEST(TimeTest, toString) {
    Time t(9, 5, 3);
    EXPECT_EQ(t.toString(), "09:05:03");
}

TEST(TimeTest, roundtrip) {
    Time t1(14, 30, 0);
    Time t2(t1.toString());
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
