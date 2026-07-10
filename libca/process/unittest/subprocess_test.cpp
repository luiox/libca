#include <gmock/gmock.h>

#include <chrono>

#include "libca/process/subprocess.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <limits.h>
#    include <unistd.h>
#endif

namespace ca::process::test {
namespace {

std::string test_executable_path()
{
#if defined(_WIN32)
    char        path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (length == 0 || length == sizeof(path)) {
        return {};
    }
    return std::string(path, length);
#else
    char          path[PATH_MAX]{};
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path));
    if (length <= 0 || length == static_cast<ssize_t>(sizeof(path))) {
        return {};
    }
    return std::string(path, static_cast<std::size_t>(length));
#endif
}

SubprocessOptions child_options(const char* mode)
{
    SubprocessOptions options;
    options.executable = test_executable_path();
    options.args       = {mode};
    return options;
}

TEST(SubprocessTest, CapturesStdoutStderrAndExitCode)
{
    auto options = child_options("--subprocess-success");

    const auto result = run(options);

    EXPECT_EQ(result.exit_code, 0) << result.stderr_data;
    EXPECT_TRUE(result.succeeded());
    EXPECT_FALSE(result.timed_out);
    EXPECT_NE(result.stdout_data.find("stdout"), std::string::npos);
    EXPECT_NE(result.stderr_data.find("stderr"), std::string::npos);
}

TEST(SubprocessTest, TerminatesOnTimeout)
{
    auto options    = child_options("--subprocess-timeout");
    options.timeout = std::chrono::milliseconds(50);

    const auto result = run(options);

    EXPECT_TRUE(result.timed_out) << result.stderr_data;
    EXPECT_EQ(result.exit_code, -1);
    EXPECT_FALSE(result.succeeded());
}

TEST(SubprocessTest, ReturnsNonzeroExitCode)
{
    auto options = child_options("--subprocess-failure");

    const auto result = run(options);

    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.exit_code, 7) << result.stderr_data;
    EXPECT_FALSE(result.succeeded());
}

TEST(SubprocessTest, ReportsLaunchFailure)
{
    SubprocessOptions options;
    options.executable = "libca_missing_subprocess_503";

    const auto result = run(options);

    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.exit_code, -1);
    EXPECT_FALSE(result.stderr_data.empty());
    EXPECT_FALSE(result.succeeded());
}

}   // namespace
}   // namespace ca::process::test
