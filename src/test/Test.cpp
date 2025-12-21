#include "Test.hpp"
#include <sstream>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#    include <io.h>
#    include <windows.h>
#else
#    include <unistd.h>
#endif

// Color helpers
static bool enableAnsiColors()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) return false;
    return true;
#else
    return isatty(fileno(stdout));
#endif
}

static bool g_enableColors = false;

static std::string colorize(const std::string& s, const char* code)
{
    if (!g_enableColors) return s;
    return std::string(code) + s + "\x1b[0m";
}
static const char* COLOR_RED = "\x1b[31m";
static const char* COLOR_GREEN = "\x1b[32m";
static const char* COLOR_YELLOW = "\x1b[33m";
static const char* COLOR_CYAN = "\x1b[36m";

namespace ca::test {

    TestCaseRegister::TestCaseRegister(const std::string& name, std::function<void()> func)
    {
        ca::test::registerTestCase(name, func);
    }

static std::vector<TestCase> g_testCases;
static int                   g_passCount = 0;
static int                   g_failCount = 0;
static bool                  g_needStop  = false;
static TestCase*             g_currentCase = nullptr;

void registerTestCase(const std::string& name, std::function<void()> func)
{
    TestCase testCase;
    testCase.name      = name;
    testCase.function  = func;
    testCase.failCount = 0;
    testCase.passCount = 0;
    g_testCases.push_back(testCase);
    std::cout << "[test] registered: " << name << " (total " << g_testCases.size() << ")" << std::endl;
    // debug: append to registrations log
    {
        FILE* f = fopen("registrations.log", "a");
        if (f) {
            fprintf(f, "%s\n", name.c_str());
            fclose(f);
        }
    }
}

// Static init diagnostic
struct TestStaticInitPrinter {
    TestStaticInitPrinter() {
        std::cout << "[test] Test.cpp static init done" << std::endl;
        std::atexit([](){ std::cout << "[test] atexit called" << std::endl; });
    }
    ~TestStaticInitPrinter() {
        std::cout << "[test] Test.cpp static dtor" << std::endl;
    }
} g_testStaticInitPrinter;

void runAllTests()
{
    // enable color output if possible
    g_enableColors = enableAnsiColors();

    std::cout << colorize(std::string("Running ") + std::to_string(g_testCases.size()) + " test cases...", COLOR_CYAN) << std::endl;
    for (size_t i = 0; i < g_testCases.size(); i++) {
        auto& testCase = g_testCases[i];
        try {
            g_currentCase = &testCase;
            testCase.function();
            g_currentCase = nullptr;
            std::cout << colorize(std::string("Test '") + testCase.name + "' passed " + std::to_string(testCase.passCount) + ".", COLOR_GREEN) << std::endl;
        }
        catch (const std::exception& e) {
            if (g_currentCase) g_currentCase->failCount++;
            std::cout << colorize(std::string("Test '") + testCase.name + "' failed: " + e.what(), COLOR_RED) << std::endl;
            if (g_needStop) {
                exit(1);
            }
        }
        catch (...) {
            std::cout << colorize(std::string("Test '") + testCase.name + "' failed with unknown exception.", COLOR_RED) << std::endl;
        }
        g_passCount += testCase.passCount;
        g_failCount += testCase.failCount;
    }
    std::cout << colorize(std::string("Total Pass :") + std::to_string(g_passCount) + "  Fail :" + std::to_string(g_failCount), COLOR_YELLOW) << std::endl;
}

int runAllTestsAndGetFailCount()
{
    runAllTests();
    return g_failCount;
}

// 断言，如果失败会终止测试
void requireTrue(const char* file, int line, bool condition, const std::string& message)
{
    if (condition) {
        if (g_currentCase) g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireTrue fail!" << std::endl << "file :" << file << ", line :" << line;
        if (!message.empty()) ss << "\n" << message;
        throw std::runtime_error(ss.str());
    }
}
void requireFalse(const char* file, int line, bool condition, const std::string& message)
{
    requireTrue(file, line, !condition, message);
}
void requireEqual(const char* file, int line, double expect, double real, double delta)
{
    if (fabs(expect - real) < delta) {
        if (g_currentCase) g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        ss << "\nexpect=" << expect << ", real=" << real << ", delta=" << delta;
        throw std::runtime_error(ss.str());
    }
}

// 断言，如果失败会继续测试（非致命）
void assertTrue(const char* file, int line, bool condition, const std::string& message)
{
    if (condition) {
        if (g_currentCase) g_currentCase->passCount++;
    }
    else {
        if (g_currentCase) g_currentCase->failCount++;
        std::stringstream ss;
        ss << "assertTrue fail!" << std::endl << "file :" << file << ", line :" << line;
        if (!message.empty()) ss << "\n" << message;
        std::cerr << colorize(ss.str(), COLOR_RED) << std::endl;
        // do not throw, continue testing
    }
}
void assertFalse(const char* file, int line, bool condition, const std::string& message)
{
    assertTrue(file, line, !condition, message);
}
void assertEqual(const char* file, int line, double expect, double real, double delta)
{
    if (fabs(expect - real) < delta) {
        if (g_currentCase) g_currentCase->passCount++;
    }
    else {
        if (g_currentCase) g_currentCase->failCount++;
        std::stringstream ss;
        ss << "assertEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        ss << "\nexpect=" << expect << ", real=" << real << ", delta=" << delta;
        std::cerr << colorize(ss.str(), COLOR_RED) << std::endl;
    }
}
}   // namespace ca::test

#ifdef TEST_USE_DEFAULT_MAIN

int main()
{
    std::cout << "[test] main start" << std::endl;
    ca::test::runAllTests();
    std::cout << "[test] main exit" << std::endl;
    return 0;
}

#endif   // TEST_USE_DEFAULT_MAIN


#ifdef TEST_ENABLE

#    include "Test.hpp"

// TEST_FUNCTION_STATEMENT(func)
// TEST_FUNCTION_AUTO_REGISTER(func, reg, inst, case_name)
// TEST_FUNCTION_DEFINE(func)
//{
//
// }

using namespace ca::test;

TEST_CASE("Test Framework Test")
{
    TEST_FUNCTION_CALL_ARG1(assertTrue, true);

    ASSERT_TRUE(true);
    ASSERT_FALSE(1 == 2);
    ASSERT_EQUAL(1, 1);
    ASSERT_EQUAL("hello", "hello");
    std::string s1 = "foo";
    ASSERT_EQUAL("foo", s1);

    REQUIRE_TRUE(1 == 1);
    REQUIRE_FALSE(2 == 3);
    REQUIRE_EQUAL(1, 1);
    REQUIRE_EQUAL("hello", "hello");
    std::string s2 = "foo";
    REQUIRE_EQUAL("foo", s2);
}


#endif   // TEST_ENABLE
