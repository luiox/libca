#include <gtest/gtest.h>

#include "libca/core/minidump.hpp"
#include "libca/core/platform.hpp"

#if CA_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <filesystem>
    #include <string>
#endif

namespace ca::core::test {

#if CA_PLATFORM_WINDOWS

TEST(MinidumpTest, WriteMinidumpCreatesFile) {
    // 不真崩溃：直接调用 write_minidump 把当前进程 dump 到系统临时目录。
    namespace fs = std::filesystem;
    const std::string dump_path = (fs::temp_directory_path() /
        ("libca_minidump_test-" + std::to_string(GetCurrentProcessId()) + ".dmp")).string();

    std::error_code ec;
    fs::remove(dump_path, ec);

    const auto status = write_minidump(dump_path);
    ASSERT_TRUE(status.is_ok()) << status.to_string();

    const auto size = fs::file_size(dump_path, ec);
    EXPECT_FALSE(ec);
    EXPECT_GT(size, 0u);

    fs::remove(dump_path, ec);
}

TEST(MinidumpTest, WriteMinidumpRejectsEmptyPath) {
    EXPECT_TRUE(write_minidump("").is_err());
}

TEST(MinidumpTest, InstallUninstallIdempotent) {
    const std::string prefix = (std::filesystem::temp_directory_path() /
        "libca_minidump_crash").string();

    EXPECT_TRUE(install_crash_dump(prefix).is_ok());
    EXPECT_TRUE(is_crash_dump_installed());

    // 幂等：重复安装只更新前缀，仍返回 OK。
    EXPECT_TRUE(install_crash_dump(prefix + "_2").is_ok());
    EXPECT_TRUE(is_crash_dump_installed());

    EXPECT_TRUE(uninstall_crash_dump().is_ok());
    EXPECT_FALSE(is_crash_dump_installed());

    // 幂等：未安装时再次卸载仍返回 OK。
    EXPECT_TRUE(uninstall_crash_dump().is_ok());
    EXPECT_FALSE(is_crash_dump_installed());
}

TEST(MinidumpTest, InstallRejectsInvalidPrefix) {
    EXPECT_TRUE(install_crash_dump("").is_err());
    EXPECT_TRUE(install_crash_dump(std::string(1000, 'x')).is_err());
    EXPECT_FALSE(is_crash_dump_installed());
}

#else

TEST(MinidumpTest, UnsupportedOnNonWindows) {
    EXPECT_TRUE(install_crash_dump("dump").is_err());
    EXPECT_TRUE(uninstall_crash_dump().is_err());
    EXPECT_TRUE(write_minidump("dump.dmp").is_err());
    EXPECT_FALSE(is_crash_dump_installed());
}

#endif

} // namespace ca::core::test
