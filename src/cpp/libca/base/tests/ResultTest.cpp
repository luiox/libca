#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "Result.hpp"
#include <iostream>
#include <string>
#include <doctest/doctest.h>
using namespace std;
using namespace ca;

auto testFunc() ->Result<int, string>{
    return Ok(1);
}

auto errFunc() ->Result<int, string>{
    string e = "error";
    return Err(e );
}

TEST_CASE("ResultTest"){
    REQUIRE(testFunc().isOk());
    REQUIRE(testFunc().unwrap() == 1);
    REQUIRE(testFunc().unwrapOr(2) == 1);
    REQUIRE(errFunc().isErr());
    REQUIRE(errFunc().unwrapErr() == "error");

}