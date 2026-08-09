#include <gtest/gtest.h>

#include "libca/log/level.hpp"

namespace ca::log::test {
namespace {

TEST(LevelTest, FromStringCaseInsensitive)
{
    EXPECT_EQ(from_string("trace"), Level::Trace);
    EXPECT_EQ(from_string("DEBUG"), Level::Debug);
    EXPECT_EQ(from_string("Info"), Level::Info);
    EXPECT_EQ(from_string("critical"), Level::Critical);
}

TEST(LevelTest, FromStringUnknownReturnsOff)
{
    EXPECT_EQ(from_string("nope"), Level::Off);
    EXPECT_EQ(from_string(""), Level::Off);
}

TEST(LevelTest, ToString)
{
    EXPECT_EQ(to_string(Level::Trace), "Trace");
    EXPECT_EQ(to_string(Level::Critical), "Critical");
    EXPECT_EQ(to_string(Level::Off), "Off");
}

}  // namespace
}  // namespace ca::log::test
