/*
    自动收集，并且测试的原理就是全局对象会先在main之前构造

*/

#ifndef LIBCA_TEST_TEST_HPP
#define LIBCA_TEST_TEST_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <sstream>
#include <cstring>

// 测试框架命名空间
namespace ca::test {

    // 定义测试用例结构
    struct TestCase
    {
        std::string name;
        std::function<void()> function;
        uint32_t passCount;
        uint32_t failCount;
        std::string file;      // source file where the test was defined (stores __FILE__)
        TestCase(const std::string& name, std::function<void()> function, uint32_t passCount, uint32_t failCount, const std::string& file = "")
            : name(name), function(function), passCount(passCount), failCount(failCount), file(file)
        {
        }
    };

    struct TestCaseRegister{
        TestCaseRegister(const std::string& name, std::function<void()> func, const char* file = nullptr);
    };


// 注册测试用例
void registerTestCase(const std::string& name, std::function<void()> func, const char* file = nullptr);

// 运行所有注册的测试用例（可通过 setTestFilterPredicate 或 setOnlyRunTestsFromFile 过滤要运行的测试）
void setTestFilterPredicate(const std::function<bool(const TestCase&)>& predicate);
// 便利函数：只运行来自某个源文件的测试
void setOnlyRunTestsFromFile(const std::string& filename);

// Logging control (off by default for registrations)
void setLogRegistrations(bool enable);
// Control whether individual test pass messages are printed
void setPrintPasses(bool enable);
// 运行所有注册的测试用例
void runAllTests();
// Run all tests and return number of failed tests (for programs that need an exit code)
int runAllTestsAndGetFailCount();
// 断言，如果失败会终止测试
void requireTrue(const char* file, int line, bool condition, const std::string& message = "");
void requireFalse(const char* file, int line, bool condition, const std::string& message = "");
void requireEqual(const char* file, int line, double expect, double real, double delta = 0.00001);

// C-string overloads for requireEqual (add optional message)
inline void requireEqual(const char* file, int line, const char* expect, const char* real, const std::string& message = "")
{
    if (expect && real && strcmp(expect, real) == 0) {
        requireTrue(file, line, true, message);
    }
    else {
        std::ostringstream ss;
        ss << "requireEqual fail! expect=" << (expect ? expect : "(null)") << ", real=" << (real ? real : "(null)");
        if (!message.empty()) ss << "\n" << message;
        requireTrue(file, line, false, ss.str());
    }
}

// 通用模板，用于大多数类型（内联以保持可见），增加 message 参数
template<typename T, typename U>
inline void requireEqual(const char* file, int line, const T& expect, const U& real, const std::string& message = "")
{
    if (expect == real) {
        requireTrue(file, line, true, message);
    }
    else {
        std::ostringstream ss;
        ss << "requireEqual fail! expect=" << expect << ", real=" << real;
        if (!message.empty()) ss << "\n" << message;
        requireTrue(file, line, false, ss.str());
    }
}

// 断言，如果失败会继续测试（非致命）
void assertTrue(const char* file, int line, bool condition, const std::string& message = "");

// 继续保留 double 的近似比较重载（增加可选消息参数）
void requireEqual(const char* file, int line, double expect, double real, const std::string& message = "", double delta = 0.00001);
void assertFalse(const char* file, int line, bool condition, const std::string& message = "");

// C-string overloads (compare contents, not pointers)
inline void assertEqual(const char* file, int line, const char* expect, const char* real)
{
    if (expect && real && strcmp(expect, real) == 0) {
        assertTrue(file, line, true);
    }
    else {
        std::ostringstream ss;
        ss << "assertEqual fail! expect=" << (expect ? expect : "(null)") << ", real=" << (real ? real : "(null)");
        assertTrue(file, line, false, ss.str());
    }
}

// 通用模板，用于大多数类型（非致命）
template<typename T, typename U>
inline void assertEqual(const char* file, int line, const T& expect, const U& real)
{
    if (expect == real) {
        assertTrue(file, line, true);
    }
    else {
        std::ostringstream ss;
        ss << "assertEqual fail! expect=" << expect << ", real=" << real;
        assertTrue(file, line, false, ss.str());
    }
}

// 继续保留 double 的近似比较重载（增加可选消息参数，并把 message 放在 delta 之前以方便宏使用）
void assertEqual(const char* file, int line, double expect, double real, const std::string& message = "", double delta = 0.00001);

// C-string overloads (compare contents, not pointers)
inline void assertEqual(const char* file, int line, const char* expect, const char* real, const std::string& message = "")
{
    if (expect && real && strcmp(expect, real) == 0) {
        assertTrue(file, line, true, message);
    }
    else {
        std::ostringstream ss;
        ss << "assertEqual fail! expect=" << (expect ? expect : "(null)") << ", real=" << (real ? real : "(null)");
        if (!message.empty()) ss << "\n" << message;
        assertTrue(file, line, false, ss.str());
    }
}

// 通用模板，用于大多数类型（非致命），增加 message 参数
template<typename T, typename U>
inline void assertEqual(const char* file, int line, const T& expect, const U& real, const std::string& message = "")
{
    if (expect == real) {
        assertTrue(file, line, true, message);
    }
    else {
        std::ostringstream ss;
        ss << "assertEqual fail! expect=" << expect << ", real=" << real;
        if (!message.empty()) ss << "\n" << message;
        assertTrue(file, line, false, ss.str());
    }
}

}   // namespace ca::test

#define MAKE_STRING(a) #a
#define MAKE_CONCAT(a, b) a##b
#define UNIQUE_ID2(a, b) MAKE_CONCAT(a, b)
#define UNIQUE_ID(id) UNIQUE_ID2(id, __COUNTER__)
#define UNIQUE_TEST_FUNCTION_NAME UNIQUE_ID(ca_test_test_func_)
#define UNIQUE_TEST_REGISTER_TYPE_NAME UNIQUE_ID(ca_test_Test_Register_)
#define UNIQUE_TEST_REGISTER_INST_NAME UNIQUE_ID(ca_test_Test_Register_reg_instance_)
#define TEST_FUNCTION_STATEMENT2(function_name) static void function_name();
#define TEST_FUNCTION_STATEMENT(function_name) TEST_FUNCTION_STATEMENT2(function_name)
#define TEST_FUNCTION_DEFINE2(function_name) static void function_name()
#define TEST_FUNCTION_DEFINE(function_name) TEST_FUNCTION_DEFINE2(function_name)

#define TEST_FUNCTION_AUTO_REGISTER3(function_name, register_name, test_case_name) \
    static ca::test::TestCaseRegister register_name(test_case_name, &function_name, __FILE__);

#define TEST_FUNCTION_AUTO_REGISTER2(function_name, register_name, test_case_name) \
    TEST_FUNCTION_AUTO_REGISTER3(                                                                 \
        function_name, register_name, MAKE_STRING(test_case_name))

#define TEST_FUNCTION_AUTO_REGISTER(function_name, register_name, test_case_name) \
    TEST_FUNCTION_AUTO_REGISTER2(function_name, register_name, test_case_name)

// 定义一个测试函数
#define TEST_CASE_IMPL(name, id)                                            \
    TEST_FUNCTION_STATEMENT( MAKE_CONCAT(ca_test_test_func_, id) )            \
    static ca::test::TestCaseRegister MAKE_CONCAT(ca_test_Test_Register_reg_instance_, id) (MAKE_STRING(name), MAKE_CONCAT(ca_test_test_func_, id), __FILE__); \
    TEST_FUNCTION_DEFINE( MAKE_CONCAT(ca_test_test_func_, id) )

#define TEST_CASE(name) TEST_CASE_IMPL(name, __COUNTER__)

#define TEST_FUNCTION_CALL_ARG2(function_name, arg1, arg2) \
    function_name(__FILE__, __LINE__, arg1, arg2)
#define TEST_FUNCTION_CALL_ARG1(function_name, arg) function_name(__FILE__, __LINE__, arg)

#define MAKE_EQUAL_STRING2(right) " == " MAKE_STRING(right)
#define MAKE_EQUAL_STRING(left, right) MAKE_STRING(left) MAKE_EQUAL_STRING2(right)

// 断言宏，如果失败会终止测试
#define REQUIRE_TRUE(condition) ca::test::requireTrue(__FILE__, __LINE__, (condition), MAKE_STRING(condition))
#define REQUIRE_FALSE(condition) ca::test::requireFalse(__FILE__, __LINE__, (condition), MAKE_STRING(condition))
#define REQUIRE_EQUAL(expect, real) ca::test::requireEqual(__FILE__, __LINE__, (expect), (real), MAKE_EQUAL_STRING(expect, real))

// 断言宏，如果失败会继续测试（非致命）
#define ASSERT_TRUE(condition) ca::test::assertTrue(__FILE__, __LINE__, (condition), MAKE_STRING(condition))
#define ASSERT_FALSE(condition) ca::test::assertFalse(__FILE__, __LINE__, (condition), MAKE_STRING(condition))
#define ASSERT_EQUAL(expect, real) ca::test::assertEqual(__FILE__, __LINE__, (expect), (real), MAKE_EQUAL_STRING(expect, real))


// 配置通常是在包含此头文件之前定义，或者在编译器命令行中定义
// 控制是否启用测试代码
#ifndef TEST_ENABLE
#    define TEST_ENABLE 0
#endif
// 控制是否启用自测试的 main 函数
#ifndef TEST_USE_DEFAULT_MAIN
#    define TEST_USE_DEFAULT_MAIN 0
#endif
// 控制成功的断言等是否也显示行号
#ifndef TEST_USE_SUCCESS_MSG
#    define TEST_USE_SUCCESS_MSG 0
#endif

#endif   // !LIBCA_TEST_TEST_HPP
