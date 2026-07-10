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
    return length == 0 || length == sizeof(path) ? std::string{} : std::string(path, length);
#else
    char          path[PATH_MAX]{};
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path));
    return length <= 0 || length == static_cast<ssize_t>(sizeof(path))
               ? std::string{}
               : std::string(path, static_cast<usize>(length));
#endif
}

Command child_command(const char* mode)
{
    Command command(test_executable_path());
    command.arg(mode);
    return command;
}

TEST(CommandTest, OutputCapturesBothStreams)
{
    auto command = child_command("--subprocess-success");
    auto result  = command.output();

    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
    const auto output = result.unwrap();
    EXPECT_TRUE(output.status.success());
    EXPECT_EQ(output.stdout_data, "stdout");
    EXPECT_EQ(output.stderr_data, "stderr");
}

TEST(CommandTest, ChildExposesInteractiveStandardPipes)
{
    auto command = child_command("--subprocess-echo");
    command.stdin(Stdio::piped()).stdout(Stdio::piped());
    auto spawned = command.spawn();
    ASSERT_TRUE(spawned.is_ok()) << spawned.unwrap_err().to_string();

    auto child  = std::move(spawned).unwrap();
    auto input  = child.take_stdin();
    auto output = child.take_stdout();
    ASSERT_TRUE(input.has_value());
    ASSERT_TRUE(output.has_value());
    ASSERT_TRUE(input->write_all("ping\n").is_ok());
    input->close();

    auto echoed = output->read_to_end();
    ASSERT_TRUE(echoed.is_ok()) << echoed.unwrap_err().to_string();
    EXPECT_EQ(echoed.unwrap(), "ping");

    auto status = child.wait();
    ASSERT_TRUE(status.is_ok()) << status.unwrap_err().to_string();
    EXPECT_TRUE(status.unwrap().success());
}

TEST(CommandTest, StatusReturnsChildExitCode)
{
    auto command = child_command("--subprocess-failure");
    auto status  = command.status();

    ASSERT_TRUE(status.is_ok()) << status.unwrap_err().to_string();
    EXPECT_EQ(status.unwrap().code, 7);
    EXPECT_FALSE(status.unwrap().success());
}

TEST(ChildTest, WaitForThenKillAndReap)
{
    auto command = child_command("--subprocess-timeout");
    auto spawned = command.spawn();
    ASSERT_TRUE(spawned.is_ok()) << spawned.unwrap_err().to_string();

    auto child   = std::move(spawned).unwrap();
    auto pending = child.wait_for(std::chrono::milliseconds(20));
    ASSERT_TRUE(pending.is_ok()) << pending.unwrap_err().to_string();
    EXPECT_FALSE(pending.unwrap().has_value());

    ASSERT_TRUE(child.kill().is_ok());
    auto status = child.wait();
    ASSERT_TRUE(status.is_ok()) << status.unwrap_err().to_string();
    EXPECT_FALSE(status.unwrap().success());
}

TEST(AnonymousPipeTest, TransfersDataAndSignalsEndOfStream)
{
    auto pipe = ipc::create_anonymous_pipe();
    ASSERT_TRUE(pipe.is_ok()) << pipe.unwrap_err().to_string();

    auto endpoints = std::move(pipe).unwrap();
    ASSERT_TRUE(endpoints.writer.write_all("hello").is_ok());
    endpoints.writer.close();

    auto data = endpoints.reader.read_to_end();
    ASSERT_TRUE(data.is_ok()) << data.unwrap_err().to_string();
    EXPECT_EQ(data.unwrap(), "hello");
}

}   // namespace
}   // namespace ca::process::test
