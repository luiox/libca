#include <gmock/gmock.h>

#include <chrono>
#include <thread>

#include "libca/process/subprocess.hpp"
#include "libca/process/ipc.hpp"

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

TEST(CommandTest, ReusesCommandAndPreservesArgumentBoundaries)
{
    auto command = child_command("--subprocess-args");
    command.arg("contains spaces").arg("");

    auto first = command.output();
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_EQ(first.unwrap().stdout_data, "contains spaces|");

    auto second = command.output();
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_EQ(second.unwrap().stdout_data, "contains spaces|");
}

TEST(CommandTest, EnvironmentOverridePreservesInheritedVariables)
{
    auto command = child_command("--subprocess-env");
    command.env("LIBCA_PROCESS_OVERRIDE", "value with spaces");

    auto result = command.output();
    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
    EXPECT_TRUE(result.unwrap().status.success());
    EXPECT_EQ(result.unwrap().stdout_data, "inherited|value with spaces");
}

TEST(CommandTest, WaitWithOutputDrainsLargeStandardStreams)
{
    auto command = child_command("--subprocess-large-output");
    auto result  = command.output();

    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
    EXPECT_TRUE(result.unwrap().status.success());
    EXPECT_EQ(result.unwrap().stdout_data.size(), 256U * 1024U);
    EXPECT_EQ(result.unwrap().stderr_data.size(), 256U * 1024U);
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

TEST(ChildTest, WaitReturnsCachedStatusAfterTryWaitReapsChild)
{
    auto command = child_command("--subprocess-success");
    command.stdout(Stdio::null()).stderr(Stdio::null());
    auto spawned = command.spawn();
    ASSERT_TRUE(spawned.is_ok()) << spawned.unwrap_err().to_string();

    auto child = std::move(spawned).unwrap();
    std::optional<ExitStatus> status;
    for (usize index = 0; index < 100 && !status.has_value(); ++index) {
        auto result = child.try_wait();
        ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
        status = result.unwrap();
        if (!status.has_value())
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(status->success());

    auto waited = child.wait();
    ASSERT_TRUE(waited.is_ok()) << waited.unwrap_err().to_string();
    EXPECT_EQ(waited.unwrap().code, status->code);
    EXPECT_TRUE(child.kill().is_err());
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

// ipc.hpp 各原语（SharedMemory/NamedPipe/NamedSemaphore/MessageQueue/remove_*）的
// 用例已迁移至 ipc_test.cpp。

}   // namespace
}   // namespace ca::process::test
