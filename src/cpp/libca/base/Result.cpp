#include "Result.hpp"
#include <iostream>
#include <string>

using namespace std;
using namespace ca;

#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace ca::test;

auto testFunc() -> Result<int, string>
{
    return Ok(1);
}

auto errFunc() -> Result<int, string>
{
    string e = "error";
    return Err(e);
}

TEST_CASE(ResultTest)
{
    ASSERT_FALSE(!testFunc().isOk());
    ASSERT_FALSE(testFunc().unwrap() != 1);
    ASSERT_FALSE(testFunc().unwrapOr(2) != 1);
    ASSERT_FALSE(!errFunc().isErr());
    ASSERT_FALSE(errFunc().unwrapErr() != "error");
}


// TEST_CASE("test Err")
// {
//     Err err1;
//     err1.append("test info1").append("test info2");
//     CHECK(err1.error() == "test info1test info2");
//     std::runtime_error e1("test info3");
//     Err                err2(e1);
//     CHECK(err2.error() == "test info3");
//     Err err3(std::string("test info4"));
//     CHECK(err3.error() == "test info4");
// }

// auto test_fun_helper(bool should_ok) -> Result<int, Err>
// {
//     if (should_ok) {
//         return ok(1);
//     }
//     return error<int>(Err(std::string("not ok")));
// }

// auto test_result_func() -> Result<Void, Err>
// {
//     return ok();
// }

// TEST_CASE("test Result")
// {
//     auto res = test_fun_helper(true);
//     if (res.is_ok()) {
//         CHECK(res.unwrap() == 1);
//     }
//     res = test_fun_helper(false);
//     if (!res.is_ok()) {
//         CHECK(res.unwrap_err().error() == "not ok");
//     }
//     std::cout << "sizeof(res):" << sizeof(res) << std::endl;
// }

// // 辅助函数，用于测试value_or方法
// template <typename T, typename E>
// void
// test_value_or(Result<T, E> result, T expected_value)
// {
//     T actual_value = result.value_or(T());
//     if (actual_value != expected_value) {
//         std::cerr << "Value is not as expected: " << actual_value
//                   << " != " << expected_value << std::endl;
//     }
// }

// // 辅助函数，用于测试error方法
// template <typename T, typename E>
// void
// test_error(Result<T, E> result, E expected_error)
// {
//     E actual_error = result.error();
//     if (actual_error != expected_error) {
//         std::cerr << "Error is not as expected: " << actual_error
//                   << " != " << expected_error << std::endl;
//     }
// }

// // 测试成功情况
// void
// test_ok()
// {
//     Result<int, std::string> result = ok(42);
//     test_value_or(result, 42);
// }

// // 测试错误情况
// void
// test_error()
// {
//     Result<int, std::string> result = error("Error message");
//     test_error(result, "Error message");
// }

// // 测试value_or方法
// void
// test_value_or_method()
// {
//     Result<int, std::string> result = ok(42);
//     test_value_or(result, 42);

//     Result<int, std::string> error_result = error("Error message");
//     test_value_or(error_result, 0); // 使用默认值0
// }

// // 测试error方法
// void
// test_error_method()
// {
//     Result<int, std::string> result = ok(42);
//     test_error(result, "Tried to get error from successful Result");

//     Result<int, std::string> error_result = error("Error message");
//     test_error(error_result, "Error message");
// }

// // 主函数
// int
// main()
// {
//     test_ok();
//     test_error();
//     test_value_or_method();
//     test_error_method();

//     std::cout << "All tests passed!" << std::endl;
//     return 0;
// }

// Result<int, std::string>
// divide(int dividend, int divisor)
// {
//     if (divisor == 0) {
//         return error<std::string, int>("Division by zero");
//     }
//     return ok(dividend / divisor);
// }

// int
// main()
// {
//     auto result = divide(10, 2);
//     if (result.is_ok()) {
//         std::cout << "Success: " << result.value() << std::endl;
//     }
//     else {
//         std::cout << "Error: " << result.error() << std::endl;
//     }

//     // 使用value_or提供默认值
//     int value = result.value_or(0);
//     std::cout << "Value is: " << value << std::endl;

//     return 0;
// }


#endif