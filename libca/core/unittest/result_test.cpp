#include <gmock/gmock.h>

#include "libca/core/result.hpp"

#include <memory>
#include <string>
#include <utility>

namespace ca::core { namespace test {

using namespace testing;
using namespace std::literals;

// ==================== 构造 / 基本查询 ====================

TEST(ResultTest, ConstructOk) {
    Result<int, std::string> r = Ok(42);
    EXPECT_TRUE(r.is_ok());
    EXPECT_FALSE(r.is_err());
    EXPECT_EQ(r.unwrap(), 42);
}

TEST(ResultTest, ConstructErr) {
    Result<int, std::string> r = Err("fail"s);
    EXPECT_FALSE(r.is_ok());
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), "fail");
}

TEST(ResultTest, OkVoid) {
    Result<void, std::string> r = Ok();
    EXPECT_TRUE(r.is_ok());
}

TEST(ResultTest, ErrVoidError) {
    Result<void, std::string> r = Err("bad"s);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), "bad");
}

TEST(ResultTest, MoveOk) {
    Result<std::string, int> r1 = Ok(std::string("hello"));
    Result<std::string, int> r2(std::move(r1));
    EXPECT_TRUE(r2.is_ok());
    EXPECT_EQ(r2.unwrap(), "hello");
}

TEST(ResultTest, CopyOk) {
    Result<int, std::string> r1 = Ok(42);
    Result<int, std::string> r2(r1);
    EXPECT_EQ(r2.unwrap(), 42);
    EXPECT_EQ(r1.unwrap(), 42);
}

// ==================== 赋值（此前隐式删除，见回归测试） ====================

TEST(ResultTest, MoveAssignOkOverErr) {
    Result<std::string, int> dst = Err(7);
    Result<std::string, int> src = Ok(std::string("hello"));
    dst = std::move(src);
    EXPECT_TRUE(dst.is_ok());
    EXPECT_EQ(dst.unwrap(), "hello");
}

TEST(ResultTest, MoveAssignErrOverOk) {
    Result<std::string, int> dst = Ok(std::string("x"));
    Result<std::string, int> src = Err(99);
    dst = std::move(src);
    ASSERT_TRUE(dst.is_err());
    EXPECT_EQ(dst.unwrap_err(), 99);
}

TEST(ResultTest, CopyAssignPreservesSource) {
    Result<int, std::string> dst = Err(std::string("boom"));
    Result<int, std::string> src = Ok(42);
    dst = src;
    EXPECT_EQ(dst.unwrap(), 42);
    EXPECT_EQ(src.unwrap(), 42);  // 拷贝赋值不动源
}

TEST(ResultTest, SelfAssignMoveIsSafe) {
    Result<std::string, int> r = Ok(std::string("keep"));
    Result<std::string, int>& ref = r;
    r = std::move(ref);  // 自赋值不应破坏内容
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap(), "keep");
}

TEST(ResultTest, VoidResultAssign) {
    Result<void, int> dst = Err(3);
    Result<void, int> src = Ok();
    dst = std::move(src);
    EXPECT_TRUE(dst.is_ok());
}

TEST(ResultTest, ReassignInLoop) {
    Result<std::string, int> r = Err(0);
    for (int i = 1; i <= 3; ++i) r = Ok(std::to_string(i));
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap(), "3");
}

// ==================== unwrap / unwrap_or / unwrap_err ====================

TEST(ResultTest, UnwrapOk) {
    Result<int, std::string> r = Ok(99);
    EXPECT_EQ(r.unwrap(), 99);
}

TEST(ResultTest, UnwrapOrOk) {
    Result<int, std::string> r = Ok(10);
    EXPECT_EQ(r.unwrap_or(0), 10);
}

TEST(ResultTest, UnwrapOrErr) {
    Result<int, std::string> r = Err("err"s);
    EXPECT_EQ(r.unwrap_or(42), 42);
}

TEST(ResultTest, UnwrapErrOnErr) {
    Result<int, std::string> r = Err("oops"s);
    EXPECT_EQ(r.unwrap_err(), "oops");
}

// ==================== expect ====================

TEST(ResultTest, ExpectOk) {
    Result<int, std::string> r = Ok(7);
    EXPECT_EQ(r.expect("should be ok"), 7);
}

// ==================== map ====================

TEST(ResultTest, MapOk) {
    Result<int, std::string> r = Ok(3);
    auto mapped = r.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.unwrap(), 6);
}

TEST(ResultTest, MapErr) {
    Result<int, std::string> r = Err("fail"s);
    auto mapped = r.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.unwrap_err(), "fail");
}

TEST(ResultTest, MapToDifferentType) {
    Result<int, std::string> r = Ok(42);
    auto mapped = r.map([](int x) { return std::to_string(x); });
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.unwrap(), "42");
}

TEST(ResultTest, MapVoidOk) {
    Result<void, std::string> r = Ok();
    int side = 0;
    auto mapped = r.map([&]() { side = 1; });
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(side, 1);
}

// ==================== map_error ====================

TEST(ResultTest, MapErrorOnErr) {
    Result<int, std::string> r = Err("fail"s);
    auto mapped = r.map_error([](const std::string& e) { return e.size(); });
    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.unwrap_err(), 4u);
}

TEST(ResultTest, MapErrorOnOk) {
    Result<int, std::string> r = Ok(10);
    auto mapped = r.map_error([](const std::string& e) { return e.size(); });
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.unwrap(), 10);
}

// ==================== then (副作用 on Ok) ====================

TEST(ResultTest, ThenOk) {
    Result<int, std::string> r = Ok(5);
    int side = 0;
    auto result = r.then([&](int x) { side = x; });
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(side, 5);
    EXPECT_EQ(result.unwrap(), 5);
}

TEST(ResultTest, ThenErr) {
    Result<int, std::string> r = Err("skip"s);
    int side = -1;
    auto result = r.then([&](int x) { side = x; });
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(side, -1);
}

// ==================== otherwise (副作用 on Err) ====================

TEST(ResultTest, OtherwiseErr) {
    Result<int, std::string> r = Err("log me"s);
    std::string logged;
    auto result = r.otherwise([&](const std::string& e) { logged = e; });
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(logged, "log me");
}

TEST(ResultTest, OtherwiseOk) {
    Result<int, std::string> r = Ok(99);
    std::string logged = "none";
    auto result = r.otherwise([&](const std::string& e) { logged = e; });
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(logged, "none");
}

// ==================== or_else (恢复) ====================

TEST(ResultTest, OrElseErr) {
    Result<int, std::string> r = Err("not found"s);
    auto recovered = r.or_else([](const std::string&) -> Result<int, std::string> {
        return Ok(42);
    });
    EXPECT_TRUE(recovered.is_ok());
    EXPECT_EQ(recovered.unwrap(), 42);
}

TEST(ResultTest, OrElseOk) {
    Result<int, std::string> r = Ok(10);
    auto recovered = r.or_else([](const std::string&) -> Result<int, std::string> {
        return Ok(99);
    });
    EXPECT_TRUE(recovered.is_ok());
    EXPECT_EQ(recovered.unwrap(), 10);
}

TEST(ResultTest, OrElseChangeErrorType) {
    Result<int, std::string> r = Err("bad"s);
    auto recovered = r.or_else([](const std::string&) -> Result<int, bool> {
        return Err(true);
    });
    EXPECT_TRUE(recovered.is_err());
    EXPECT_EQ(recovered.unwrap_err(), true);
}

// ==================== and_then ====================

TEST(ResultTest, AndThenOk) {
    Result<int, std::string> r = Ok(5);
    auto next = r.and_then([](int x) -> Result<std::string, std::string> {
        return Ok(std::to_string(x));
    });
    EXPECT_TRUE(next.is_ok());
    EXPECT_EQ(next.unwrap(), "5");
}

TEST(ResultTest, AndThenErr) {
    Result<int, std::string> r = Err("fail"s);
    auto next = r.and_then([](int x) -> Result<std::string, std::string> {
        return Ok(std::to_string(x));
    });
    EXPECT_TRUE(next.is_err());
    EXPECT_EQ(next.unwrap_err(), "fail");
}

TEST(ResultTest, AndThenChained) {
    Result<int, std::string> r = Ok(3);
    auto r2 = r
        .and_then([](int x) -> Result<int, std::string> { return Ok(x * 2); })
        .and_then([](int x) -> Result<int, std::string> { return Ok(x + 1); });
    EXPECT_EQ(r2.unwrap(), 7);
}

TEST(ResultTest, AndThenShortCircuit) {
    Result<int, std::string> r = Ok(3);
    auto r2 = r
        .and_then([](int) -> Result<int, std::string> { return Err("stop"s); })
        .and_then([](int) -> Result<int, std::string> { return Ok(999); });
    EXPECT_TRUE(r2.is_err());
    EXPECT_EQ(r2.unwrap_err(), "stop");
}

TEST(ResultTest, AndThenVoidToNonVoid) {
    Result<void, std::string> r = Ok();
    auto next = r.and_then([]() -> Result<int, std::string> { return Ok(42); });
    EXPECT_TRUE(next.is_ok());
    EXPECT_EQ(next.unwrap(), 42);
}

TEST(ResultTest, AndThenVoidErr) {
    Result<void, std::string> r = Err("bad"s);
    auto next = r.and_then([]() -> Result<int, std::string> { return Ok(42); });
    EXPECT_TRUE(next.is_err());
    EXPECT_EQ(next.unwrap_err(), "bad");
}

// ==================== operator== ====================

TEST(ResultTest, EqualOk) {
    Result<int, std::string> a = Ok(1);
    Result<int, std::string> b = Ok(1);
    Result<int, std::string> c = Ok(2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ResultTest, EqualErr) {
    Result<int, std::string> a = Err("a"s);
    Result<int, std::string> b = Err("a"s);
    Result<int, std::string> c = Err("b"s);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ResultTest, EqualOkVoid) {
    Result<void, std::string> a = Ok();
    Result<void, std::string> b = Ok();
    EXPECT_TRUE(a == b);
}

// ==================== map + and_then 混用 ====================

TEST(ResultTest, MapThenAndThen) {
    Result<int, std::string> r = Ok(10);
    auto r2 = r
        .map([](int x) { return x * 2; })
        .and_then([](int x) -> Result<int, std::string> { return Ok(x + 3); });
    EXPECT_EQ(r2.unwrap(), 23);
}

// ==================== TRY 宏 ====================
// TRY uses GCC statement expression __extension__ ({ ... }), not available on MSVC

#if !defined(_MSC_VER) || defined(__clang__)

static Result<int, std::string> try_unwrap_impl(Result<int, std::string> input) {
    auto val = TRY(input);
    return Ok(val * 2);
}

TEST(ResultTest, TryMacroOk) {
    auto r = try_unwrap_impl(Ok(21));
    EXPECT_EQ(r.unwrap(), 42);
}

TEST(ResultTest, TryMacroErr) {
    auto r = try_unwrap_impl(Err(std::string("fail")));
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), "fail");
}

static Result<void, std::string> try_void_ok_source() {
    return Ok();
}

static Result<int, std::string> try_void_ok_impl() {
    TRY(try_void_ok_source());
    return Ok(42);
}

TEST(ResultTest, TryMacroVoidOk) {
    auto r = try_void_ok_impl();
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap(), 42);
}

static Result<void, std::string> try_void_err_source() {
    return Err("fail"s);
}

static Result<int, std::string> try_void_err_impl() {
    TRY(try_void_err_source());
    return Ok(42);
}

TEST(ResultTest, TryMacroVoidErr) {
    auto r = try_void_err_impl();
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), "fail");
}

static Result<std::unique_ptr<int>, std::string> make_move_only_ok() {
    return Ok(std::make_unique<int>(42));
}

static Result<std::unique_ptr<int>, std::string> try_move_only_ok_impl() {
    auto val = TRY(make_move_only_ok());
    return Ok(std::move(val));
}

TEST(ResultTest, TryMacroMoveOnlyOk) {
    auto r = try_move_only_ok_impl();
    ASSERT_TRUE(r.is_ok());
    auto& val = r.storage().get<std::unique_ptr<int>>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 42);
}

static Result<int, std::unique_ptr<int>> make_move_only_err() {
    return Err(std::make_unique<int>(7));
}

static Result<int, std::unique_ptr<int>> try_move_only_err_impl() {
    auto val = TRY(make_move_only_err());
    return Ok(val);
}

TEST(ResultTest, TryMacroMoveOnlyErr) {
    auto r = try_move_only_err_impl();
    ASSERT_TRUE(r.is_err());
    auto& err = r.storage().get<std::unique_ptr<int>>();
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(*err, 7);
}

#endif

}} // namespace ca::core::test
