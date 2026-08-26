#include <gmock/gmock.h>

#include <utility>

#include "libca/core/dynamic_library.hpp"

#if defined(_WIN32)
#    include <windows.h>
#endif

namespace ca::core::test {
namespace {

#if defined(_WIN32)
const char* system_library_path()
{
    return "kernel32.dll";
}

#if defined(_MSC_VER)
using ProcessIdFunction = DWORD(WINAPI)();
#else
// MinGW gcc 不接受 "DWORD(WINAPI)()" 的调用约定内嵌别名语法（MSVC 扩展）。
// x64 Windows 下 WINAPI(stdcall) 即默认调用约定，直接省略不影响符号解析与调用。
using ProcessIdFunction = DWORD();
#endif
const char* process_id_symbol()
{
    return "GetCurrentProcessId";
}
#else
const char* system_library_path()
{
    return "libc.so.6";
}

using ProcessIdFunction = int();
const char* process_id_symbol()
{
    return "getpid";
}
#endif

TEST(DynamicLibraryTest, LoadsLooksUpAndCallsSystemSymbol)
{
    auto result = DynamicLibrary::load(system_library_path());
    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();

    auto library = std::move(result).unwrap();
    EXPECT_TRUE(library.is_loaded());

    auto symbol = library.lookup<ProcessIdFunction>(process_id_symbol());
    ASSERT_TRUE(symbol.is_ok()) << symbol.unwrap_err().to_string();
    EXPECT_NE(symbol.unwrap()(), 0);
}

TEST(DynamicLibraryTest, ReturnsNotFoundForMissingLibrary)
{
    auto result = DynamicLibrary::load("libca_missing_dynamic_library_504");

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code(), StatusCode::NOT_FOUND);
}

TEST(DynamicLibraryTest, ReturnsNotFoundForMissingSymbol)
{
    auto result = DynamicLibrary::load(system_library_path());
    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();

    auto library = std::move(result).unwrap();
    auto symbol  = library.lookup<ProcessIdFunction>("libca_missing_symbol_504");

    ASSERT_TRUE(symbol.is_err());
    EXPECT_EQ(symbol.unwrap_err().code(), StatusCode::NOT_FOUND);
}

TEST(DynamicLibraryTest, RejectsLookupAfterUnload)
{
    auto result = DynamicLibrary::load(system_library_path());
    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();

    auto library = std::move(result).unwrap();
    library.unload();

    EXPECT_FALSE(library.is_loaded());
    auto symbol = library.lookup<ProcessIdFunction>(process_id_symbol());
    ASSERT_TRUE(symbol.is_err());
    EXPECT_EQ(symbol.unwrap_err().code(), StatusCode::FAILED_PRECONDITION);
}

}   // namespace
}   // namespace ca::core::test
