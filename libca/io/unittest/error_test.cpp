#include <gmock/gmock.h>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <winsock2.h>
#    include <windows.h>
#else
#    include <cerrno>
#endif

#include "libca/io/error.hpp"

namespace ca::io::test {
namespace {

TEST(IoErrorTest, SyntheticErrorPreservesKindAndHasNoNativeCode)
{
    auto error = IoError::from_kind(IoErrorKind::UnexpectedEof, "missing header");

    EXPECT_EQ(error.kind(), IoErrorKind::UnexpectedEof);
    EXPECT_EQ(error.native_code(), 0);
    EXPECT_EQ(error.message(), "missing header");
    EXPECT_NE(error.to_string().find("UnexpectedEof"), std::string::npos);
    EXPECT_EQ(error.to_status().code(), ca::core::StatusCode::DATA_LOSS);
}

TEST(IoErrorTest, NativeErrorPreservesCodeAndMapsKind)
{
#if defined(_WIN32)
    constexpr i64 native_code = ERROR_FILE_NOT_FOUND;
#else
    constexpr i64 native_code = ENOENT;
#endif
    auto error = IoError::from_native_error(native_code, "open");

    EXPECT_EQ(error.kind(), IoErrorKind::NotFound);
    EXPECT_EQ(error.native_code(), native_code);
    EXPECT_NE(error.message().find("open"), std::string::npos);
    EXPECT_EQ(error.to_status().code(), ca::core::StatusCode::NOT_FOUND);
}

TEST(IoErrorTest, KindNamesAreStable)
{
    EXPECT_STREQ(io_error_kind_name(IoErrorKind::WouldBlock), "WouldBlock");
    EXPECT_STREQ(io_error_kind_name(IoErrorKind::BrokenPipe), "BrokenPipe");
    EXPECT_STREQ(io_error_kind_name(IoErrorKind::Other), "Other");
}

TEST(IoErrorTest, WouldBlockNativeCodeMapsAcrossPlatforms)
{
#if defined(_WIN32)
    constexpr i64 native_code = WSAEWOULDBLOCK;
#else
    constexpr i64 native_code = EWOULDBLOCK;
#endif
    auto error = IoError::from_native_error(native_code, "read");

    EXPECT_EQ(error.kind(), IoErrorKind::WouldBlock);
    EXPECT_EQ(error.native_code(), native_code);
}

}   // namespace
}   // namespace ca::io::test
