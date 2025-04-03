#include "Test.hpp"
#include <sstream>
#include <cstring>

namespace ca::test {

    TestCaseRegister::TestCaseRegister(const std::string& name, void (*func)())
    {
        ca::test::registerTestCase(name, func);
    }

static std::vector<TestCase> g_testCases;
static int                   g_passCount = 0;
static int                   g_failCount = 0;
static bool                  g_needStop  = false;
static TestCase*             g_currentCase;

void registerTestCase(const std::string& name, void (*func)())
{
    TestCase testCase;
    testCase.name      = name;
    testCase.function  = func;
    testCase.failCount = 0;
    testCase.passCount = 0;
    g_testCases.push_back(testCase);
}

void runAllTests()
{
    std::cout << "Running " << g_testCases.size() << " test cases..." << std::endl;
    for (size_t i = 0; i < g_testCases.size(); i++) {
        auto testCase = g_testCases[i];
        try {
            g_currentCase = &testCase;
            testCase.function();
            g_currentCase = nullptr;
            std::cout << "Test '" << testCase.name << "' passed " << testCase.passCount << "."
                      << std::endl;
        }
        catch (const std::exception& e) {
            g_currentCase->failCount++;
            std::cout << "Test '" << testCase.name << "' failed: " << e.what() << std::endl;
            if (g_needStop) {
                exit(1);
            }
        }
        catch (...) {
            std::cout << "Test '" << testCase.name << "' failed with unknown exception."
                      << std::endl;
        }
        g_passCount += testCase.passCount;
        g_failCount += testCase.failCount;
    }
    std::cout << "Total Pass :" << g_passCount << std::endl;
}

// 断言，如果失败会终止测试
void requireTrue(const char* file, int line, bool condition)
{
    if (condition) {
        g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireTrue fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void requireFalse(const char* file, int line, bool condition)
{
    if (!condition) {
        g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireFalse fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void requireEqual(const char* file, int line, int expect, int real)
{
    if (expect == real) {
        g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void requireEqual(const char* file, int line, double expect, double real, double delta)
{
    if (fabs(expect - real) < delta) {
        g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void requireEqual(const char* file, int line, const char* expect, const char* real)
{
    if (strcmp(expect, real) == 0) {
        g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void requireEqual(const char* file, int line, std::string expect, std::string real)
{
    if (expect == real) {
        g_currentCase->passCount++;
    }
    else {
        g_needStop = true;
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}

// 断言，如果失败会继续测试
void assertTrue(const char* file, int line, bool condition)
{
    if (condition) {
        g_currentCase->passCount++;
    }
    else {
        std::stringstream ss;
        ss << "assertTrue fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void assertFalse(const char* file, int line, bool condition)
{
    if (!condition) {
        g_currentCase->passCount++;
    }
    else {
        std::stringstream ss;
        ss << "assertFalse fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void assertEqual(const char* file, int line, int expect, int real)
{
    if (expect == real) {
        g_currentCase->passCount++;
    }
    else {
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void assertEqual(const char* file, int line, double expect, double real, double delta)
{
    if (fabs(expect - real) < delta) {
        g_currentCase->passCount++;
    }
    else {
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void assertEqual(const char* file, int line, const char* expect, const char* real)
{
    if (strcmp(expect, real) == 0) {
        g_currentCase->passCount++;
    }
    else {
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
void assertEqual(const char* file, int line, std::string expect, std::string real)
{
    if (expect == real) {
        g_currentCase->passCount++;
    }
    else {
        std::stringstream ss;
        ss << "requireEqual fail!" << std::endl << "file :" << file << ", line :" << line;
        throw std::runtime_error(ss.str());
    }
}
}   // namespace ca::test

#ifdef TEST_USE_DEFAULT_MAIN

int main()
{
    ca::test::runAllTests();
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

    // ASSERT_TRUE(true);
    ASSERT_FALSE(1 == 2);
    ASSERT_EQUAL(1, 1);
    ASSERT_EQUAL("hello", "hello");
    std::string s1 = "foo";
    ASSERT_EQUAL("foo", s1);

    // REQUIRE_TRUE(1 == 1);
    REQUIRE_FALSE(2 == 3);
    REQUIRE_EQUAL(1, 1);
    REQUIRE_EQUAL("hello", "hello");
    std::string s2 = "foo";
    REQUIRE_EQUAL("foo", s2);
}


#endif   // TEST_ENABLE
