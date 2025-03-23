#include "Result.hpp"
#include <iostream>
#include <string>

using namespace std;
using namespace ca;

#ifdef TEST_ENABLE

#include "libca/test/Test.hpp"
using namespace ca::test;

auto testFunc() ->Result<int, string>{
    return Ok(1);
}

auto errFunc() ->Result<int, string>{
    string e = "error";
    return Err(e );
}

TEST_CASE(ResultTest){
    std::cout << "ResultTest" << std::endl;
    ASSERT_FALSE(!testFunc().isOk());
    ASSERT_FALSE(testFunc().unwrap() != 1);
    ASSERT_FALSE(testFunc().unwrapOr(2) != 1);
    ASSERT_FALSE(!errFunc().isErr());
    ASSERT_FALSE(errFunc().unwrapErr() != "error");
}

#endif