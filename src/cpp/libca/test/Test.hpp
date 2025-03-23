/*
    自动收集，并且测试的原理就是全局对象会先在main之前构造

*/

#ifndef LIBCA_TEST_TEST_HPP
#define LIBCA_TEST_TEST_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

// 定义测试用例结构
struct TestCase {
    std::string name;
    void (*function)();
    uint32_t passCount;
    uint32_t failCount;
};

// 测试框架命名空间
namespace ca::test {
    // 注册测试用例
    void registerTestCase(const std::string& name, void (*func)());
    // 运行所有注册的测试用例
    void runAllTests();
    // 断言，如果失败会终止测试
    void requireTrue(const char* file, int line, bool condition);
    void requireFalse(const char* file, int line, bool condition);
    void requireEqual(const char* file, int line, int expect, int real);
    void requireEqual(const char* file, int line, const char* expect, const char* real);
    void requireEqual(const char* file, int line, std::string expect, std::string real);
    // 断言，如果失败会继续测试
    void assertTrue(const char* file, int line, bool condition);
    void assertFalse(const char* file, int line, bool condition);
    void assertEqual(const char* file, int line, int expect, int real);
    void assertEqual(const char* file, int line, const char* expect, const char* real);
    void assertEqual(const char* file, int line, std::string expect, std::string real);

} // namespace ca::test

#define MAKE_STRING(a) #a
#define MAKE_CONCAT(a, b) a##b
#define UNIQUE_ID2(a,b) MAKE_CONCAT(a,b)
#define UNIQUE_ID(id) UNIQUE_ID2(id, __LINE__)
#define UNIQUE_TEST_FUNCTION_NAME UNIQUE_ID(ca_test_test_func_)
#define UNIQUE_TEST_REGISTER_TYPE_NAME UNIQUE_ID(ca_test_Test_Register_)
#define UNIQUE_TEST_REGISTER_INST_NAME UNIQUE_ID(ca_test_Test_Register_reg_instance_)
#define TEST_FUNCTION_STATEMENT2(function_name) static void function_name();
#define TEST_FUNCTION_STATEMENT(function_name) TEST_FUNCTION_STATEMENT2(function_name)
#define TEST_FUNCTION_DEFINE2(function_name) static void function_name()
#define TEST_FUNCTION_DEFINE(function_name)  TEST_FUNCTION_DEFINE2(function_name)

#define TEST_FUNCTION_AUTO_REGISTER3(function_name, register_type, register_name, test_case_name) \
struct register_type { \
register_type() {  \
ca::test::registerTestCase(test_case_name, &function_name ); \
} \
};\
static register_type register_name;

#define TEST_FUNCTION_AUTO_REGISTER2(function_name, register_type, register_name, test_case_name) \
TEST_FUNCTION_AUTO_REGISTER3(function_name, register_type, register_name, MAKE_STRING(test_case_name))

#define TEST_FUNCTION_AUTO_REGISTER(function_name, register_type, register_name, test_case_name) \
TEST_FUNCTION_AUTO_REGISTER2(function_name, register_type, register_name, test_case_name)

// 定义一个测试函数

#define TEST_CASE2(name) TEST_FUNCTION_STATEMENT(UNIQUE_TEST_FUNCTION_NAME) \
TEST_FUNCTION_AUTO_REGISTER(UNIQUE_TEST_FUNCTION_NAME, \
UNIQUE_TEST_REGISTER_TYPE_NAME, \
UNIQUE_TEST_REGISTER_INST_NAME, name\
) \
TEST_FUNCTION_DEFINE(UNIQUE_TEST_FUNCTION_NAME)

#define TEST_CASE(name) TEST_CASE2(name)

#define TEST_FUNCTION_CALL_ARG2(function_name, arg1, arg2) function_name(__FILE__ , __LINE__, arg1, arg2)
#define TEST_FUNCTION_CALL_ARG1(function_name, arg) function_name(__FILE__ , __LINE__, arg)

// 断言宏，如果失败会终止测试
//#define REQUIRE_TRUE(condition) TEST_FUNCTION_CALL_ARG1(requireTrue, condition)
#define REQUIRE_FALSE(condition) TEST_FUNCTION_CALL_ARG1(requireFalse, condition)
#define REQUIRE_EQUAL(expect, real) TEST_FUNCTION_CALL_ARG2(requireEqual, expect, real)

// 断言宏，如果失败会继续测试
//#define ASSERT_TRUE(condition) TEST_FUNCTION_CALL_ARG1(assertTrue, condition)
#define ASSERT_FALSE(condition) TEST_FUNCTION_CALL_ARG1(assertFalse, condition)
#define ASSERT_EQUAL(expect, real) TEST_FUNCTION_CALL_ARG2(assertEqual, expect, real)



#endif // !LIBCA_TEST_TEST_HPP
