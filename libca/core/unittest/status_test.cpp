#include <gtest/gtest.h>

#include "libca/core/status.hpp"

#include <string>

namespace ca::core::test {

TEST(StatusTest, DefaultIsOk) {
    Status status;
    EXPECT_TRUE(status.is_ok());
    EXPECT_FALSE(status.is_err());
    EXPECT_EQ(status.code(), StatusCode::OK);
    EXPECT_TRUE(status.message().empty());
    EXPECT_EQ(status.to_string(), "OK");
}

TEST(StatusTest, ErrorCarriesCodeAndMessage) {
    auto status = Status::error(StatusCode::INVALID_ARGUMENT, "bad input");
    EXPECT_TRUE(status.is_err());
    EXPECT_EQ(status.code(), StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.message(), "bad input");
    EXPECT_EQ(status.to_string(), "INVALID_ARGUMENT: bad input");
}

TEST(StatusTest, ErrorNormalizesOkCodeToUnknown) {
    auto status = Status::error(StatusCode::OK, "not really ok");
    EXPECT_TRUE(status.is_err());
    EXPECT_EQ(status.code(), StatusCode::UNKNOWN);
    EXPECT_EQ(status.message(), "not really ok");
}

TEST(StatusTest, CodeNameAndToString) {
    EXPECT_STREQ(status_code_name(StatusCode::NOT_FOUND), "NOT_FOUND");
    EXPECT_EQ(to_string(StatusCode::UNAVAILABLE), "UNAVAILABLE");
    EXPECT_EQ(to_string(Status::error(StatusCode::INTERNAL)), "INTERNAL");
}

TEST(StatusTest, Equality) {
    EXPECT_EQ(Status::ok(), OkStatus());
    EXPECT_EQ(ErrStatus(StatusCode::NOT_FOUND, "x"),
              Status::error(StatusCode::NOT_FOUND, "x"));
    EXPECT_NE(ErrStatus(StatusCode::NOT_FOUND, "x"),
              ErrStatus(StatusCode::NOT_FOUND, "y"));
}

static StatusResult<std::string> read_name(bool ok) {
    if (!ok) {
        return Err(ErrStatus(StatusCode::NOT_FOUND, "name"));
    }
    return Ok(std::string("libca"));
}

TEST(StatusTest, StatusResultUsesStatusAsErrorType) {
    auto ok = read_name(true);
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(ok.unwrap(), "libca");

    auto err = read_name(false);
    ASSERT_TRUE(err.is_err());
    EXPECT_EQ(err.unwrap_err(), ErrStatus(StatusCode::NOT_FOUND, "name"));
}

} // namespace ca::core::test
